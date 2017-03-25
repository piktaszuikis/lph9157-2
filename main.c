#include <stdio.h>
#include <libsoc_gpio.h>
#include <libsoc_debug.h>
#include <libsoc_spi.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#define CMD_RESET	0x01
#define CMD_WAKEUP	0x11
#define CMD_PALETTE	0x3A
#define CMD_ENABLE	0x29
#define CMD_SET_X	0x2A
#define CMD_SET_Y	0x2B
#define CMD_START_WRITE 0x2C
#define CMD_MEMORY_ACCESS_CONTROL 0x36

#define PALETTE_8 0x02
#define PALETTE_12 0x03
#define PALETTE_16 0x05

#define SCREEN_WIDTH 132
#define SCREEN_HEIGHT 176

struct bmp_image_header{
	int32_t headerSize;
	int32_t width;
	int32_t height;
	int16_t planes;
	int16_t bitsPerPixel;
	//All other values are irrelevant
};

gpio *gpioPower, *gpioReset, *gpioDataOrCommand;
spi *spiDev;
uint8_t currentPalette;

void initializeGlobalVariables()
{
	//libsoc_set_debug(1);

	gpioPower = libsoc_gpio_request(233, LS_GREEDY); //lets open GPIO7, also known as PH9 or 233
	gpioReset = libsoc_gpio_request(229, LS_GREEDY); //lets open GPIO9, also known as PH5 or 229
	gpioDataOrCommand = libsoc_gpio_request(234, LS_GREEDY); //lets open GPIO8, also known as PH10 or 234

	libsoc_gpio_set_direction(gpioPower, OUTPUT);
	libsoc_gpio_set_direction(gpioReset, OUTPUT);
	libsoc_gpio_set_direction(gpioDataOrCommand, OUTPUT);

	spiDev = libsoc_spi_init(0, 0); //let's open /dev/spidev0.0
	libsoc_spi_set_speed(spiDev, 12000000); //let's operate on 12 MHz

	libsoc_spi_set_mode(spiDev, MODE_0);
	libsoc_spi_set_bits_per_word(spiDev, BITS_8);
}

void cleanupGlobalVariables()
{
	//libsoc_gpio_free(gpioPower); //uncomment if you don't want to leave display on after exit
	libsoc_gpio_free(gpioReset);
	libsoc_gpio_free(gpioDataOrCommand);
	libsoc_spi_free(spiDev);
}

void msleep(int ms){
	usleep(ms * 1000);
}

uint32_t getDataSize()
{
	return SCREEN_WIDTH * SCREEN_HEIGHT * (currentPalette > PALETTE_8 ? 2 : 1);
}

void sendCommand(uint8_t cmd)
{
	libsoc_gpio_set_level(gpioDataOrCommand, LOW);
	libsoc_spi_write(spiDev, &cmd, sizeof(uint8_t));
}

void sendData(uint8_t data)
{
	libsoc_gpio_set_level(gpioDataOrCommand, HIGH);
	libsoc_spi_write(spiDev, &data, sizeof(uint8_t));
}

void setDrawWindow(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	sendCommand(CMD_SET_X);
	sendData(x1);
	sendData(x2);

	sendCommand(CMD_SET_Y);
	sendData(y1);
	sendData(y2);
}

void draw(uint8_t *data)
{
	sendCommand(CMD_START_WRITE);
	libsoc_gpio_set_level(gpioDataOrCommand, HIGH);

	//pcduino can handle transfers only up to 64 bytes
	int chunkSize = 63;
	for(int i = getDataSize(); i > 0; i -= chunkSize)
	{
		if(libsoc_spi_write(spiDev, data, i > chunkSize ? chunkSize : i) == EXIT_FAILURE)
		{
			perror("Failed to write image data into SPI.");
			break;
		}

		data += chunkSize;
	}
}

void initializeDevice()
{
	//If LCD was left powered on, we need to hard-reset it by powering it off for over a second.
	if(libsoc_gpio_get_direction(gpioPower) == HIGH)
	{
		libsoc_gpio_set_level(gpioPower, LOW);
		libsoc_gpio_set_level(gpioReset, LOW);
		libsoc_gpio_set_level(gpioDataOrCommand, LOW);
		msleep(1500);
	}

	//Let's power on LCD
	libsoc_gpio_set_level(gpioPower, HIGH);

	//Let's reset it
	libsoc_gpio_set_level(gpioReset, LOW);
	msleep(2);
	libsoc_gpio_set_level(gpioReset, HIGH);
	sendCommand(CMD_RESET);

	//And now it's time to initialize registers:
	sendCommand(CMD_MEMORY_ACCESS_CONTROL);
	sendData(0b00000000);
	sendCommand(CMD_WAKEUP);

	sendCommand(CMD_PALETTE);
	sendData(currentPalette);

	sendCommand(CMD_ENABLE);
}

uint8_t *readImageFile(char *filename){
	//Information about bmp file format can be found at:
	//http://www.dragonwins.com/domains/getteched/bmp/bmpfileformat.htm

	int file = open(filename, O_RDONLY);

	if(file < 0)
	{
		perror("Failed to open bmp file.");
		exit(2);
	}


	lseek(file, 0x0a, SEEK_SET);
	uint32_t imageDataOffset;
	read(file, &imageDataOffset, sizeof(uint32_t));

	struct bmp_image_header header;
	read(file, &header, sizeof(struct bmp_image_header));

	if(header.width != SCREEN_WIDTH)
	{
		fprintf(stderr, "BMP image width must be %d, but it was %d.\n", SCREEN_WIDTH, header.width);
		exit(3);
	}

	if(header.height != SCREEN_HEIGHT)
	{
		fprintf(stderr, "BMP image height must be %d, but it was %d.\n", SCREEN_HEIGHT, header.height);
		exit(3);
	}

	switch (header.bitsPerPixel) {
		case 16:
			currentPalette = PALETTE_16;
			printf("Using 16bit palette.\n");;
			break;
		case 8:
			currentPalette = PALETTE_8;
			printf("Using 8bit palette.\n");;
			break;
		default:
			fprintf(stderr, "BMP image must be 16 or 8 bits per pixel, but it was %d\n", header.bitsPerPixel);
			exit(3);
			break;
	}


	lseek(file, imageDataOffset, SEEK_SET);
	uint8_t *result = malloc(getDataSize());
	if(read(file, result, getDataSize()) != getDataSize())
	{
		fprintf(stderr, "Failed to read %d bytes from the bmp file. File is too short.\n", getDataSize());
		exit(2);
	}

	//Screen expects big-endian data, but BMP data is little-endian. Swaping bytes will fix it.
	//For 8 bits palette there is only 1 byte, therefore no fixing is needed.
	if(currentPalette == PALETTE_16)
	{
		uint8_t *end = result + getDataSize();
		for(uint8_t *i = result; i < end; i += 2)
		{
			uint8_t tmp = i[0];
			i[0] = i[1];
			i[1] = tmp;
		}
	}

	return result;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Usage: %s filename.bmp\n\tDisplays bmp file on the LCD screen.\n", argv[0]);
		exit(1);
	}

	uint8_t *bmpData = readImageFile(argv[1]);

	initializeGlobalVariables();
	initializeDevice();

	setDrawWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
	draw(bmpData);

	cleanupGlobalVariables();
	free(bmpData);

	return 0;
}
