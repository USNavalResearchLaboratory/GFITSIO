LABVIEW="C:\Program Files (x86)\National Instruments\LabVIEW 2012"

gfitsio.dll: gfitsio.c 
	cl /LD /I$(LABVIEW)\cintools gfitsio.c user32.lib cfitsio.lib $(LABVIEW)\cintools\labview.lib
	move /Y gfitsio.dll ..

