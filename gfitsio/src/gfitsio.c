//
// GFITSIO 3.1
// July 2007
//
// Author: George Gatling (ggatling@gmail.com)
//
// This file contains wrapper funtions for CFITSIO that LabVIEW uses to support
// reading and writing fits files.  The CFITSIO functions use datatypes not
// native in LabVIEW and therefore cannot be called directly from LabVIEW.
//
// Only safe on 32-bit platforms.
//

// Windows compiler directives
#ifdef _WIN32
	#include <windows.h>
	#define EXPORT _declspec (dllexport)
	#define PATH_MAX 256
#endif

// Mac OS X compiler directives
#ifdef __APPLE__
	#define EXPORT
	#define PATH_MAX 256
#endif

#include "extcode.h"
#include "fitsio.h"

#define kMagic Cat4Chrs('S','T','I','F')

// A GFITSIO file refnum
typedef struct {
	int32     magic;
	char     filename[PATH_MAX];
	fitsfile *fptr;
} RefnumRecord, *Refnum, **RefnumHdl;

// This function allows to register and unregister a cleanup callback function
// that is called by LabVIEW at certain moments when the state of the active VI
// changes. This is the functionality used by LabVIEW to do auto cleanup of its
// refnums when a VI hierarchy goes idle or is aborted.
// This functionality is present since LabVIEW 2.5 (and declared, although not
// explained in the first released extcode.h distributed with LabVIEW 2.5) and
// it hasn't really changed, other than that there is at least one more
// unidentifed mode in below enum in LabVIEW 8.0

//  When should LabVIEW call the proc
enum {
	kCleanRemove,
	kCleanExit,
	kCleanOnIdle,
	kCleanAfterReset,
	kCleanOnIdleIfNotTop,
	kCleanAfterResetIfNotTop,
};

typedef MgErr (*CleanupProcPtr)(UPtr arg);
Bool32 RTSetCleanupProc(CleanupProcPtr proc, UPtr arg, int32 mode);

// ----------------------------------------------------------------------------
// CleanupIdleProc
//
// A callback function to clean up the GFITSIO refnum if the VI goes idle
// ----------------------------------------------------------------------------
MgErr CleanupIdleProc(Refnum refnum)
{
	MgErr status = noErr;

	// If the refnum is NULL return an error
	if (!refnum)
		return mgArgErr;

	// If the refnum is not NULL but kMagic is not set free the refnum and
	// return an error
	if (refnum->magic != kMagic) {
		DSDisposePtr(refnum);
		return mgArgErr;
	}

	// Close the FITS file
	fits_close_file(refnum->fptr, &status);

	// Unregister the cleanup callback function and free the refnum
	RTSetCleanupProc((CleanupProcPtr)CleanupIdleProc
	                ,(UPtr)refnum
	                ,kCleanRemove);
	DSDisposePtr(refnum);

	return status;
}

// ----------------------------------------------------------------------------
// CStrToHdl
//
// Copy a C string to a LabVIEW string handle
// ----------------------------------------------------------------------------
MgErr CStrToHdl(LStrHandle *handlePtr
               ,CStr        string)
{
	int32 length, size;
	MgErr status = noErr;

	if (!handlePtr)
			return mgArgErr;

	// If the string is NULL or of zero length free the handle.  LabVIEW
	// interprets NULL handles as empty strings.
	if (string == NULL || strlen(string) == 0) {
		DSDisposeHandle(*handlePtr);
		*handlePtr = NULL;
		return noErr;
	}

	// Allocate a handle of the correct size.
	length = strlen(string); 
	size   = length + sizeof(int32);
	status = NumericArrayResize(uB, /*numDimms*/1,(UHandle*)handlePtr,size);
	if (status)
			return status;

	// Copy the size parameter and the string to the handle.
	(**handlePtr)->cnt = length;
	StrNCpy((**handlePtr)->str,string,length);

	return status;
}

// ----------------------------------------------------------------------------
// gfits_get_errstatus
//
// Return a descriptive text string corresponding to a CFITSIO error status
// code.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_errstatus(int32        status
                                ,LStrHandle  *err_text)
{
	char  err_text_buf[FLEN_STATUS];

	fits_get_errstatus(status, err_text_buf);
	return CStrToHdl(err_text,err_text_buf);
}

// ----------------------------------------------------------------------------
// gfits_read_errmsg
//
// Return the top (oldest) error message from the internal CFITSIO stack of
// error messages and shift any remaining messages on the stack up one level.
// Call this routine repeatedly to get each message in sequence. The function
// returns a value = 0 and a null error message when the error stack is empty.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_read_errmsg(LStrHandle *err_msg)
{
	char  err_msg_buf[FLEN_ERRMSG];

	fits_read_errmsg(err_msg_buf);
	return CStrToHdl(err_msg,err_msg_buf);
}

// ----------------------------------------------------------------------------
// gfits_check_refnum
//
// Returns TRUE if the refnum is invalid.
// ----------------------------------------------------------------------------
EXPORT int32 gfits_check_refnum(RefnumHdl handle)
{
	Refnum refnum = handle ? *handle : NULL;

	if (!refnum || (refnum->magic != kMagic))
		return TRUE;
	return FALSE;
}

// ----------------------------------------------------------------------------
// gfits_create_file
//
// Create a FITS file.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_create_file(RefnumHdl handle
                              ,CStr      filename)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum or filename are NULL return an error
	if (!handle || !filename)
		return mgArgErr;

	// Allocate a new refnum
	refnum = *handle = (Refnum)DSNewPClr(sizeof(RefnumRecord));
	if (!refnum)
		return mFullErr;

	// Create the FITS file
	fits_create_file(&(refnum->fptr)
	                ,filename
	                ,&status);

	// GFITS does not use the PRIMARY HDU so write default values
	fits_write_imghdr(refnum->fptr
	                 ,8
	                 ,0
	                 ,NULL
	                 ,&status);
	fits_flush_file(refnum->fptr, &status);
	if (status) {
		// CLean up the pointer
		DSDisposePtr(refnum);
		refnum = *handle = NULL;
		return status;
	}

	// File created successfully, so register the cleanup callback function
	refnum->magic = kMagic;
	StrCpy(refnum->filename,filename);
	RTSetCleanupProc((CleanupProcPtr)CleanupIdleProc
	                ,(UPtr)refnum
	                ,kCleanOnIdle);

	return noErr;
}

// ----------------------------------------------------------------------------
// gfits_open_file
//
// Open a FITS file
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_open_file(RefnumHdl  handle
                            ,CStr       filename
                            ,int32      iomode)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If handle or filename are NULL return an error
	if (!handle || !filename)
		return mgArgErr;

	// Allocate a new refnum
	refnum = *handle = (Refnum)DSNewPClr(sizeof(RefnumRecord));
	if (!refnum)
		return mFullErr;
	
	// Open the FITS file
	fits_open_file(&(refnum->fptr)
	              ,filename
	              ,iomode
	              ,&status);
	if (status) {
		DSDisposePtr(refnum);
		refnum = *handle = NULL;
		return status;
	}

	// File opened successfully, so register the cleanup callback function
	refnum->magic = kMagic;
	StrCpy(refnum->filename,filename);
	RTSetCleanupProc((CleanupProcPtr)CleanupIdleProc
	                ,(UPtr)refnum
	                ,kCleanOnIdle);

	return status;
}

// ----------------------------------------------------------------------------
// gfits_flush_file
//
// Flush the file to disk.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_flush_file(RefnumHdl handle)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	/* Get the total number of HDUs */
	return fits_flush_file(refnum->fptr, &status);
}

// ----------------------------------------------------------------------------
// gfits_close_file
//
// Close a FITS file
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_close_file(RefnumHdl handle, LStrHandle *filename)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Close the FITS file
	fits_close_file(refnum->fptr, &status);

	// Unregister the cleanup callback function and free the refnum
	RTSetCleanupProc((CleanupProcPtr)CleanupIdleProc
	                ,(UPtr)refnum
	                ,kCleanRemove);

	if (!status)
		status=CStrToHdl(filename,refnum->filename);

	// Clear kMagic so others trying to access this pointer after it is
	// disposed will be able to tell it is no longer valid.
	refnum->magic=0;
	DSDisposePtr(refnum);

	return status;
}

// ----------------------------------------------------------------------------
// gfits_get_num_hdus
//
// Get the total number of HDUs in the file
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_num_hdus(RefnumHdl handle
                               ,int32    *numhdus)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	/* Get the total number of HDUs */
	return fits_get_num_hdus(refnum->fptr
	                        ,numhdus
	                        ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_hdu_num
//
// Get the number of the current HDU (counting starts at 1)
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_hdu_num(RefnumHdl handle
                              ,int32    *hdunum)
{
	Refnum refnum = handle ? *handle : NULL;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Get the number of the current HDU
	fits_get_hdu_num(refnum->fptr
	                ,hdunum);

	return noErr;
}

// ----------------------------------------------------------------------------
// gfits_get_hdu_type
//
// Get the type of the current HDU
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_hdu_type(RefnumHdl handle
                              ,int32     *hdutype)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Get the number of the current HDU
	fits_get_hdu_type(refnum->fptr
	                 ,hdutype
	                 ,&status);

	return status;
}

// ----------------------------------------------------------------------------
// gfits_movabs_hdu
//
// Move to a specified HDU number (counting starts at 1)
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_movabs_hdu(RefnumHdl handle
                             ,int32     hdunum
                             ,int32    *hdutype)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Move to the specifed HDU
	return fits_movabs_hdu(refnum->fptr
	                      ,hdunum
	                      ,hdutype
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_movnam_hdu
//
// Move to an HDU with the specified name
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_movnam_hdu(RefnumHdl handle
                             ,int32     hdutype
                             ,CStr      extname
                             ,int32     extver)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Move to the specified HDU
	return fits_movnam_hdu(refnum->fptr
	                      ,hdutype
	                      ,extname
	                      ,extver
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_delete_hdu
//
// Delete the current HDU
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_delete_hdu(RefnumHdl handle)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Delete the current HDU
	return fits_delete_hdu(refnum->fptr
	                      ,NULL
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_hdrspace
//
// Read a keyword.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_hdrspace(RefnumHdl   handle
                               ,int32      *nexist)
{

	Refnum refnum = handle ? *handle : NULL;
	int32  nmore;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_get_hdrspace(refnum->fptr
	                        ,nexist
	                        ,&nmore
	                        ,&status);
}

// ----------------------------------------------------------------------------
// gfits_read_record
//
// Read a keyword.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_read_record(RefnumHdl   handle
                              ,int32       nrec
                              ,LStrHandle *card)
{

	Refnum refnum = handle ? *handle : NULL;
	char   card_buf[FLEN_CARD];
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	fits_read_record(refnum->fptr
	                ,nrec
	                ,card_buf
	                ,&status);

	if (status)
		return status;

	return CStrToHdl(card,card_buf);
}

// ----------------------------------------------------------------------------
// gfits_read_key
//
// Read a keyword.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_read_key(RefnumHdl   handle
                           ,int32       datatype
                           ,CStr        keyname
                           ,void       *value
                           ,LStrHandle *units
                           ,LStrHandle *comment)
{

	Refnum refnum = handle ? *handle : NULL;
	char   value_buf[FLEN_VALUE];
	char   comment_buf[FLEN_COMMENT];
	char   units_buf[FLEN_COMMENT];
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Read the key and store the comment in a temporary buffer
	if (datatype == TSTRING)
		fits_read_key(refnum->fptr
		             ,datatype
		             ,keyname
		             ,value_buf
		             ,comment_buf
		             ,&status);
	else
		fits_read_key(refnum->fptr
		             ,datatype
		             ,keyname
		             ,value
		             ,comment_buf
		             ,&status);

	// Read the key's units and store the result in a temporary buffer
	fits_read_key_unit(refnum->fptr
	                  ,keyname
	                  ,units_buf
	                  ,&status);

	if (status)
		return status;

	if (datatype == TSTRING)
		CStrToHdl(value, value_buf);
	CStrToHdl(comment, comment_buf);
	CStrToHdl(units, units_buf);
	
	return status;
}

// ----------------------------------------------------------------------------
// gfits_update_key
//
// Write a keyword.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_update_key(RefnumHdl handle
                             ,int32     datatype
                             ,CStr      keyname
                             ,void     *value
                             ,CStr      unit
                             ,CStr      comment)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Write the key
	fits_update_key(refnum->fptr
	               ,datatype
	               ,keyname
	               ,value
	               ,comment
	               ,&status);

	// If unit is not NULL write the key's unit
	if (unit)
		fits_write_key_unit(refnum->fptr
		                   ,keyname
		                   ,unit
		                   ,&status);

	return status;
}

// ----------------------------------------------------------------------------
// gfits_write_comment
//
// Write a COMMENT key 
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_write_comment(RefnumHdl handle
                                ,CStr      comment)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Write the comment
	return fits_write_comment(refnum->fptr
	                         ,comment
	                         ,&status);
}

// ----------------------------------------------------------------------------
// gfits_write_history
//
// Write a HISTORY key
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_write_history(RefnumHdl handle
                                ,CStr      history)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Write the history
	return fits_write_history(refnum->fptr
	                         ,history
	                         ,&status);
}

// ----------------------------------------------------------------------------
// gfits_delete_key
//
// Delete the specified key
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_delete_key(RefnumHdl handle
                             ,CStr      keyname)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	/* Delete the key */
	return fits_delete_key(refnum->fptr
	                      ,keyname
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_create_img
//
// Create an image extension
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_create_img(RefnumHdl handle
                             ,int32     bitpix
                             ,int32     naxis
                             ,int32    *naxes)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_create_img(refnum->fptr
	                      ,bitpix
	                      ,naxis
	                      ,naxes
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_resize_img
//
// Create an image extension
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_resize_img(RefnumHdl handle
                             ,int32     bitpix
                             ,int32     naxis
                             ,int32    *naxes)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_resize_img(refnum->fptr
	                      ,bitpix
	                      ,naxis
	                      ,naxes
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_img_equivtype
//
// Get the equivalent data type of the image.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_img_equivtype(RefnumHdl handle
                                    ,int32    *bitpix)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_get_img_equivtype(refnum->fptr
	                             ,bitpix
	                             ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_img_dim
//
// Get the number of dimensions of the image.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_img_dim(RefnumHdl handle
                              ,int32    *naxis)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_get_img_dim(refnum->fptr
	                       ,naxis
	                       ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_img_size
//
// Get the size of the image.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_img_size(RefnumHdl handle
                               ,int32     maxdim
                               ,int32    *naxes)
{
	Refnum refnum = handle ? *handle : NULL;
	int32  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_get_img_size(refnum->fptr
	                        ,maxdim
	                        ,naxes
	                        ,&status);
}

// ----------------------------------------------------------------------------
// gfits_read_subset
//
// Read an rectangular subimage (or the whole image) from the FITS data array.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_read_subset(RefnumHdl handle
                              ,int32     datatype
                              ,int32    *fpixel
                              ,int32    *lpixel
                              ,int32    *inc
                              ,void     *array)
{
	Refnum refnum = handle ? *handle : NULL;
	int32 status = noErr;
	int32 anynul;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Read the subimage
	return fits_read_subset(refnum->fptr
	                       ,datatype
	                       ,fpixel
	                       ,lpixel
	                       ,inc
	                       ,NULL
	                       ,array
	                       ,&anynul
	                       ,&status);
}

// ----------------------------------------------------------------------------
// gfits_write_subset
//
// Write image subset (or the whole image).
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_write_subset(RefnumHdl handle
                               ,int32     datatype
                               ,int32    *fpixel
                               ,int32    *lpixel
                               ,void     *array)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status  = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_write_subset(refnum->fptr
	                        ,datatype
	                        ,fpixel
	                        ,lpixel
	                        ,array
	                        ,&status);
}

// ----------------------------------------------------------------------------
// gfits_create_tbl
//
// Create and ASCII or BINARY table
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_create_tbl(RefnumHdl handle
                             ,int32     tbltype
                             ,CStr      extname)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Create the table
	return fits_create_tbl(refnum->fptr
	                      ,tbltype
	                      ,0    /* NAXIS2  */
	                      ,0    /* tfields */
	                      ,NULL /* ttype[] */
	                      ,NULL /* tform[] */
	                      ,NULL /* tunit[] */
	                      ,extname
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_num_rows
//
// Get number of rows in an ASCII or BINARY table
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_num_rows(RefnumHdl handle
                               ,int32    *nrows)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Get the number of rows
	return fits_get_num_rows(refnum->fptr
	                        ,nrows
	                        ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_num_cols
//
// Get number of columns in an ASCII or BINARY table
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_num_cols(RefnumHdl handle
                               ,int32    *ncols)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Get the number of rows
	return fits_get_num_cols(refnum->fptr
	                        ,ncols
	                        ,&status);
}

// ----------------------------------------------------------------------------
// gfits_insert_col
//
// Insert a column into a table.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_insert_col(RefnumHdl handle
                             ,int32     colnum
                             ,CStr      ttype
                             ,CStr      tform)
{
	Refnum refnum = handle ? *handle : NULL;
	int32  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	fits_insert_col(refnum->fptr
	               ,colnum
	               ,ttype
	               ,tform
	               ,&status);


	return status;
}

// ----------------------------------------------------------------------------
// gfits_get_colnum
//
// Get the number of the named column.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_colnum(RefnumHdl handle
                             ,int32     casesen
                             ,CStr      column
                             ,int32    *colnum)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Get the column number
	return fits_get_colnum(refnum->fptr
	                      ,casesen
	                      ,column
	                      ,colnum
	                      ,&status);
}

// ----------------------------------------------------------------------------
// gfits_get_eqcoltype
//
// Get the type and number of elements in a cell of a table.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_get_eqcoltype(RefnumHdl handle
                                ,int32     colnum
                                ,int32    *datatype
                                ,int32    *nelements
                                ,int32    *width)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;
	int32  offset, nrows;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// Get the type and number of elements
	return fits_get_coltype(refnum->fptr
	                       ,colnum
	                       ,datatype
	                       ,nelements
	                       ,width
	                       ,&status);	
}

// ----------------------------------------------------------------------------
// gfits_read_descript
//
// Get the descriptor for a variable length array
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_read_descript(RefnumHdl handle
                                ,int32     colnum
                                ,int32     row
                                ,int32    *nelements
                                ,int32    *offset)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	return fits_read_descript(refnum->fptr
	                         ,colnum
	                         ,row
	                         ,nelements
	                         ,offset
	                         ,&status);
}

// ----------------------------------------------------------------------------
// gfits_read_col
//
// Read column from a FITS binary table.
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_read_col(RefnumHdl handle
                           ,int32     datatype
                           ,int32     colnum
                           ,int32     row
                           ,int32     nelements
                           ,void     *data)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status = noErr;
	int32  anynul;
	
	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// CFITSIO treats TSTRING differently for this function.
	// It expects a char** instead of the normal char*
	if (datatype == TSTRING)
		// Read the array
		return fits_read_col(refnum->fptr
	                    ,datatype
	                    ,colnum
	                    ,row
	                    ,1    /* firstelem */
	                    ,nelements
	                    ,NULL /* nulval    */
	                    ,&data
	                    ,&anynul
	                    ,&status);

	// Read the array
	return fits_read_col(refnum->fptr
	                    ,datatype
	                    ,colnum
	                    ,row
	                    ,1    /* firstelem */
	                    ,nelements
	                    ,NULL /* nulval    */
	                    ,data
	                    ,&anynul
	                    ,&status);
}

// ----------------------------------------------------------------------------
// gfits_write_col
//
// Write column
// ----------------------------------------------------------------------------
EXPORT MgErr gfits_write_col(RefnumHdl handle
                            ,int32     datatype
                            ,int32     colnum
                            ,int32     row
                            ,int32     nelements
                            ,void     *data)
{
	Refnum refnum = handle ? *handle : NULL;
	MgErr  status  = noErr;
	int32  typecode, repeat, offset;

	// If the refnum isn't valid return an error
	if (!refnum || (refnum->magic != kMagic))
		return mgArgErr;

	// CFITSIO treats TSTRING differently for this function.
	// It expects a char** instead of the normal char*
	if (datatype == TSTRING)
		// Write to the table
		return fits_write_col(refnum->fptr
		                     ,datatype
	        	             ,colnum
	                	     ,row
		                     ,1
		                     ,nelements
	        	             ,&data
	                	     ,&status);

	// Write to the table
	return fits_write_col(refnum->fptr
	                     ,datatype
	                     ,colnum
	                     ,row
	                     ,1
	                     ,nelements
	                     ,data
	                     ,&status);
}
