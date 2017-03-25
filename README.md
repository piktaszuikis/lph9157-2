# Example program for displaying images on LCD screen LPH9157-2 usually found in Siemens C75 mobile phones

Program uses [libsoc](https://github.com/jackmitch/libsoc) library.

More details about the project on http://piktas-zuikis.netlify.com/2017/03/18/lph9157-2/

## Usage
To compile the program you will need cmake and libsoc witch in turn requires autotools and libtool.

After dependencies are installed, all you need to do is:

```
mkdir bin
cd bin
cmake ../
```

Executable will be called 'lph9157-2'.
To run it type

```
./lph9157-2 ../examples/test_16.bmp
```

If you attached your sceen to the pcDuio3b correctly, you will see the image in it.

## Wiring
Information about pcDuino3b pins can be found on [linux-sinxi wiki page for pcduino](http://linux-sunxi.org/LinkSprite_pcDuino3#Expansion_headers)

In this example these pins are used:

| pcDuino pin | # LCD | LCD
|-------------|-------|----|
| J11.8 - PH9 | 7 | VCC
| J8.1 - PH10 | 1 |	RS
| J8.2 - PH5  | 2 |	~RST
| J8.4 - PI12 | 6 |	DATA
| J8.6 - PI11 | 5 |	CLK
| J8.7 - GND  | 3 |	~CS
| J8.7 - GND  | 8 |	GND

