# Linux kernel module for LCD screen LPH9157-2 usually found in Siemens C75 mobile phones

Module uses tinydrm interface.

More details about the project on http://piktas-zuikis.netlify.com/2017/10/12/lph9157-2-kernelio-modulis/

There is also a userspace program version of this driver, you can find it on the [master branch](https://github.com/piktaszuikis/lph9157-2/tree/master).

## Usage
To compile the program you will need linux kernel > v4.11 and it's headers. Kernel must me compilled with `DRM_TINYDRM` and `TINYDRM_MIPI_DBI` enabled.
To enable `TINYDRM_MIPI_DBI` you will most likely will have to enable `TINYDRM_MI0283QT` too.

Compiling the module is as simple as typing `make`.

After sucessfull compilation you can insmod the module. If you compiled kernel with TINYDRM_MIPI_DBI as a module, you will have to modprobe mipi_dbi first.

```
modprobe mipi_dbi
insmod lph9157-2.ko
```

If you attached your sceen to the pcDuio3b correctly, you will see a colorful noise. To map a console to the screen type

```
con2fbmap 1 1
```

## Wiring
Information about pcDuino3b pins can be found on [linux-sinxi wiki page for pcduino](http://linux-sunxi.org/LinkSprite_pcDuino3#Expansion_headers)

In this example these pins are used (as defined in the device-tree):

| pcDuino pin | # LCD | LCD
|-------------|-------|----|
| J11.8 - PH9 | 7 | VCC
| J8.1 - PH10 | 1 |	RS
| J8.2 - PH5  | 2 |	~RST
| J8.4 - PI12 | 6 |	DATA
| J8.6 - PI11 | 5 |	CLK
| J8.7 - GND  | 3 |	~CS
| J8.7 - GND  | 8 |	GND
