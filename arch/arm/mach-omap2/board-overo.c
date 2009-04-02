/*
 * board-overo.c (Gumstix Overo)
 *
 * Initial code: Steve Sakoman <steve@sakoman.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl4030.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>

#include <mach/board-overo.h>
#include <mach/board.h>
#include <mach/common.h>
#include <mach/display.h>
#include <mach/gpio.h>
#include <mach/gpmc.h>
#include <mach/hardware.h>
#include <mach/nand.h>
#include <mach/usb-ehci.h>
#include <mach/usb-musb.h>

#include "sdram-micron-mt46h32m32lf-6.h"
#include "twl4030-generic-scripts.h"
#include "mmc-twl4030.h"

#define NAND_BLOCK_SIZE SZ_128K
#define GPMC_CS0_BASE  0x60
#define GPMC_CS_SIZE   0x30

/* DSS */

#define OVERO_GPIO_LCD_EN 144

static void __init overo_display_init(void)
{
	int r;

	r = gpio_request(OVERO_GPIO_LCD_EN, "display enable");
	if (r)
		printk("fail1\n");
	r = gpio_direction_output(OVERO_GPIO_LCD_EN, 1);
	if (r)
		printk("fail2\n");
	gpio_export(OVERO_GPIO_LCD_EN, 0);
}

static int overo_panel_enable_dvi(struct omap_display *display)
{
	if (lcd_enabled) {
		printk(KERN_ERR "cannot enable DVI, LCD is enabled\n");
		return -EINVAL;
	}
	dvi_enabled = 1;

	gpio_set_value(OVERO_GPIO_LCD_EN, 1);

	return 0;
}

static void overo_panel_disable_dvi(struct omap_display *display)
{
	gpio_set_value(OVERO_GPIO_LCD_EN, 0);

	dvi_enabled = 0;
}

static struct omap_dss_display_config overo_display_data_dvi = {
	.type = OMAP_DISPLAY_TYPE_DPI,
	.name = "dvi",
	.panel_name = "panel-generic",
	.u.dpi.data_lines = 24,
	.panel_enable = overo_panel_enable_dvi,
	.panel_disable = overo_panel_disable_dvi,
};

static int overo_panel_enable_lcd(struct omap_display *display)
{
	if (dvi_enabled) {
		printk(KERN_ERR "cannot enable LCD, DVI is enabled\n");
		return -EINVAL;
	}

	gpio_set_value(OVERO_GPIO_LCD_EN, 1);
	lcd_enabled = 1;
	return 0;
}

static void overo_panel_disable_lcd(struct omap_display *display)
{
	gpio_set_value(OVERO_GPIO_LCD_EN, 0);
	lcd_enabled = 0;
}

static struct omap_dss_display_config overo_display_data_lcd = {
	.type = OMAP_DISPLAY_TYPE_DPI,
	.name = "lcd",
	.panel_name = "samsung-lte430wq-f0c",
	.u.dpi.data_lines = 24,
	.panel_enable = overo_panel_enable_lcd,
	.panel_disable = overo_panel_disable_lcd,
 };

static struct omap_dss_board_info overo_dss_data = {
	.num_displays = 2,
	.displays = {
		&overo_display_data_dvi,
		&overo_display_data_lcd,
	}
};

static struct platform_device overo_dss_device = {
	.name          = "omapdss",
	.id            = -1,
	.dev            = {
		.platform_data = &overo_dss_data,
	},
};

static struct omap_board_config_kernel overo_config[] __initdata = {
	{ OMAP_TAG_UART,	&overo_uart_config },
};

static struct platform_device *overo_devices[] __initdata = {
	&overo_dss_device,
};

static struct twl4030_hsmmc_info mmc[] __initdata = {
	{
		.mmc		= 1,
		.wires		= 4,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
	{
		.mmc		= 2,
		.wires		= 4,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
	{}	/* Terminator */
};

static void __init overo_init(void)
{
	overo_i2c_init();
	platform_add_devices(overo_devices, ARRAY_SIZE(overo_devices));
	omap_board_config = overo_config;
	omap_board_config_size = ARRAY_SIZE(overo_config);
	omap_serial_init();
	twl4030_mmc_init(mmc);
	usb_musb_init();
	usb_ehci_init();
	overo_flash_init();
	overo_display_init();

	if ((gpio_request(OVERO_GPIO_W2W_NRESET,
			  "OVERO_GPIO_W2W_NRESET") == 0) &&
	    (gpio_direction_output(OVERO_GPIO_W2W_NRESET, 1) == 0)) {
		gpio_export(OVERO_GPIO_W2W_NRESET, 0);
		gpio_set_value(OVERO_GPIO_W2W_NRESET, 0);
		udelay(10);
		gpio_set_value(OVERO_GPIO_W2W_NRESET, 1);
	} else {
		printk(KERN_ERR "could not obtain gpio for "
					"OVERO_GPIO_W2W_NRESET\n");
	}

	if ((gpio_request(OVERO_GPIO_BT_XGATE, "OVERO_GPIO_BT_XGATE") == 0) &&
	    (gpio_direction_output(OVERO_GPIO_BT_XGATE, 0) == 0))
		gpio_export(OVERO_GPIO_BT_XGATE, 0);
	else
		printk(KERN_ERR "could not obtain gpio for OVERO_GPIO_BT_XGATE\n");

	if ((gpio_request(OVERO_GPIO_BT_NRESET, "OVERO_GPIO_BT_NRESET") == 0) &&
	    (gpio_direction_output(OVERO_GPIO_BT_NRESET, 1) == 0)) {
		gpio_export(OVERO_GPIO_BT_NRESET, 0);
		gpio_set_value(OVERO_GPIO_BT_NRESET, 0);
		mdelay(6);
		gpio_set_value(OVERO_GPIO_BT_NRESET, 1);
	} else {
		printk(KERN_ERR "could not obtain gpio for "
					"OVERO_GPIO_BT_NRESET\n");
	}

	if ((gpio_request(OVERO_GPIO_USBH_CPEN, "OVERO_GPIO_USBH_CPEN") == 0) &&
	    (gpio_direction_output(OVERO_GPIO_USBH_CPEN, 1) == 0))
		gpio_export(OVERO_GPIO_USBH_CPEN, 0);
	else
		printk(KERN_ERR "could not obtain gpio for "
					"OVERO_GPIO_USBH_CPEN\n");

	if ((gpio_request(OVERO_GPIO_USBH_NRESET,
			  "OVERO_GPIO_USBH_NRESET") == 0) &&
	    (gpio_direction_output(OVERO_GPIO_USBH_NRESET, 1) == 0))
		gpio_export(OVERO_GPIO_USBH_NRESET, 0);
	else
		printk(KERN_ERR "could not obtain gpio for "
					"OVERO_GPIO_USBH_NRESET\n");
}

static void __init overo_map_io(void)
{
	omap2_set_globals_343x();
	omap2_map_common_io();
}

MACHINE_START(OVERO, "Gumstix Overo")
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xd8000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= overo_map_io,
	.init_irq	= overo_init_irq,
	.init_machine	= overo_init,
	.timer		= &omap_timer,
MACHINE_END
