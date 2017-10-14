#include <drm/tinydrm/mipi-dbi.h>
#include <drm/tinydrm/tinydrm-helpers.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <video/mipi_display.h>

static int (*mipi_command_base)(struct mipi_dbi *mipi, u8 cmd, u8 *param, size_t num) = 0;

static int mipi_command_override(struct mipi_dbi *mipi, u8 cmd, u8 *data, size_t len)
{
	//Our LPH9157 wants x and y coordinates for draw window to be 1 byte length, but default driver sends 2 bytes.
	//We will fix that here
	if(len == 4 && (cmd == MIPI_DCS_SET_COLUMN_ADDRESS || cmd == MIPI_DCS_SET_PAGE_ADDRESS)){
		data[0] = data[1];
		data[1] = data[3];
		len = 2;
	}

	return (*mipi_command_base)(mipi, cmd, data, len);
}

//Initialization of the screen
static int lph9157_init(struct mipi_dbi *mipi)
{
	struct tinydrm_device *tdev = &mipi->tinydrm;
	struct device *dev = tdev->drm->dev;
	int ret;

	mipi_dbi_hw_reset(mipi);
	ret = mipi_dbi_command(mipi, MIPI_DCS_SOFT_RESET);
	if (ret) {
		dev_err(dev, "Error sending command MIPI_DCS_SOFT_RESET (%d): %d\n", MIPI_DCS_SOFT_RESET, ret);
		return ret;
	}

	msleep(20);

	mipi_dbi_command(mipi, MIPI_DCS_SET_ADDRESS_MODE, 0b00000000);
	mipi_dbi_command(mipi, MIPI_DCS_EXIT_SLEEP_MODE);
	mipi_dbi_command(mipi, MIPI_DCS_SET_PIXEL_FORMAT, 0x05); //16 bit palette
	mipi_dbi_command(mipi, MIPI_DCS_SET_DISPLAY_ON);

	msleep(20);

	return 0;
}

//We will be using default drawing and framebuffer handling functions
static const struct drm_simple_display_pipe_funcs lph9157_pipe_funcs = {
	.enable = mipi_dbi_pipe_enable,
	.disable = mipi_dbi_pipe_disable,
	.update = tinydrm_display_pipe_update,
	.prepare_fb = tinydrm_display_pipe_prepare_fb,
};

//Width and height of the screen in pixels and in mm
static const struct drm_display_mode lph9157_mode = {
	TINYDRM_MODE(132, 176, 37, 27),
};

DEFINE_DRM_GEM_CMA_FOPS(lph9157_fops);

static struct drm_driver lph9157_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_PRIME | DRIVER_ATOMIC,
	.fops			= &lph9157_fops,
	TINYDRM_GEM_DRIVER_OPS,
	.lastclose		= tinydrm_lastclose,
	.debugfs_init	= mipi_dbi_debugfs_init,
	.name			= "lph9157",
	.desc			= "Ekranėlio LPH9157-2 video draiveris.",
	.date			= "20171011",
	.major			= 1,
	.minor			= 0,
};

struct modulio_apjungti_duomenys
{
	struct mipi_dbi *mipi;
	struct gpio_desc *dc; //Data/Controll indicator
	struct gpio_desc *power;
};

//This will be called on insmod
static int lph9157_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct tinydrm_device *tdev;
	struct modulio_apjungti_duomenys *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->mipi = devm_kzalloc(dev, sizeof(*data->mipi), GFP_KERNEL);
	if (!data->mipi)
		return -ENOMEM;

	data->power = devm_gpiod_get_optional(dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(data->power)) {
		dev_err(dev, "Failed to get gpio 'power'\n");
		return PTR_ERR(data->power);
	}

	data->mipi->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(data->mipi->reset)) {
		dev_err(dev, "Failed to get gpio 'reset'\n");
		return PTR_ERR(data->mipi->reset);
	}

	data->dc = devm_gpiod_get_optional(dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(data->dc)) {
		dev_err(dev, "Failed to get gpio 'dc'\n");
		return PTR_ERR(data->dc);
	}

	ret = mipi_dbi_spi_init(spi, data->mipi, data->dc, &lph9157_pipe_funcs, &lph9157_driver, &lph9157_mode, 0);
	if (ret)
		return ret;

	//Override of mipi->command
	mipi_command_base = data->mipi->command;
	data->mipi->command = mipi_command_override;

	ret = lph9157_init(data->mipi);
	if (ret)
		return ret;

	tdev = &data->mipi->tinydrm;

	ret = devm_tinydrm_register(tdev);
	if (ret)
		return ret;

	//We will need this data in lph9157_shutdown and lph9157_remove
	spi_set_drvdata(spi, data);

	printk(KERN_INFO "Initialized %s:%s @%uMHz on minor %d\n",
			 tdev->drm->driver->name, dev_name(dev),
			 spi->max_speed_hz / 1000000,
			 tdev->drm->primary->index);

	return 0;
}

void lph9157_poweroff(struct modulio_apjungti_duomenys *data)
{
	if(data) {
		gpiod_set_value(data->power, 0);
		gpiod_set_value(data->dc, 0);
		gpiod_set_value(data->mipi->reset, 0);

		printk(KERN_INFO "LPH9157 was powered off. Wait at least 20s before powering it on again.");
	} else {
		printk(KERN_WARNING "Failed to poweroff LPH9157: no GPIO data.");
	}
}

//This will be called on PC shutdown/reboot
static void lph9157_shutdown(struct spi_device *spi)
{
	struct modulio_apjungti_duomenys *data = spi_get_drvdata(spi);
	lph9157_poweroff(data);
	tinydrm_shutdown(&data->mipi->tinydrm);
}

//This will be called on rmmod
static int lph9157_remove(struct spi_device *spi)
{
	struct modulio_apjungti_duomenys *data = spi_get_drvdata(spi);
	lph9157_poweroff(data);
	return 0;
}

static const struct of_device_id lph9157_of_match[] = {
	{ .compatible = "lph9157" },
	{},
};
MODULE_DEVICE_TABLE(of, lph9157_of_match);

static struct spi_driver lph9157_spi_driver = {
	.driver = {
		.name = "lph9157",
		.owner = THIS_MODULE,
		.of_match_table = lph9157_of_match,
	},
	.probe = lph9157_probe,
	.remove = lph9157_remove,
	.shutdown = lph9157_shutdown,
};
module_spi_driver(lph9157_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Piktas Zuikis");
MODULE_DESCRIPTION("Ekranėlio LPH9157-2 video draiveris.");
