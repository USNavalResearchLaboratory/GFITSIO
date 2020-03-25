LABVIEW=National Instruments\LabVIEW 2019
LABVIEW32="C:\Program Files (x86)\$(LABVIEW)"
LABVIEW64="C:\Program Files\$(LABVIEW)"

gfitsio32.dll: gfitsio.c 
	cl /LD /MD /I$(LABVIEW32)\cintools gfitsio.c user32.lib cfitsio.lib $(LABVIEW32)\cintools\labview.lib /Fe$@
	move /Y $@ ..

gfitsio64.dll: gfitsio.c 
	cl /LD /MD /I$(LABVIEW64)\cintools gfitsio.c user32.lib cfitsio.lib $(LABVIEW64)\cintools\labview.lib /Fe$@
	move /Y $@ ..