/*
 * This file is part of the TREZOR project.
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>

#include "bootloader.h"
#include "buttons.h"
#include "setup.h"
#include "usb.h"
#include "oled.h"
#include "util.h"
#include "signatures.h"
#include "layout.h"
#include "serialno.h"

#ifdef APPVER
#error Bootloader cannot be used in app mode
#endif

void show_unofficial_warning(void)
{
	layoutDialog(DIALOG_ICON_WARNING, "Abort", "I'll take the risk", NULL, "WARNING!", NULL, "Unofficial firmware", "detected.", NULL, NULL);

	do {
		delay(100000);
		buttonUpdate();
	} while (!button.YesUp && !button.NoUp);

	if (button.YesUp) {
		return; // yes button was pressed -> return
	}

	layoutDialog(DIALOG_ICON_ERROR, NULL, NULL, NULL, "Unofficial firmware", "aborted.", NULL, "Unplug your TREZOR", "and see our support", "page at mytrezor.com");
	system_halt();
}

void load_app(void)
{
	// jump to app
	SCB_VTOR = FLASH_APP_START; // & 0xFFFF;
	asm volatile("msr msp, %0"::"g" (*(volatile uint32_t *)FLASH_APP_START));
	(*(void (**)())(FLASH_APP_START + 4))();
}

void bootloader_loop(void)
{
	static char serial[25];

	fill_serialno_fixed(serial);

	oledDrawBitmap(0, 0, &bmp_logo64);
	oledDrawString(52, 0, "TREZOR");

	oledDrawString(52, 20, "Serial No.");
	oledDrawString(52, 40, serial + 12); // second part of serial
	serial[12] = 0;
	oledDrawString(52, 30, serial);      // first part of serial

	oledDrawStringRight(OLED_WIDTH - 1, OLED_HEIGHT - 8, "BLv" VERSTR(VERSION_MAJOR) "." VERSTR(VERSION_MINOR) "." VERSTR(VERSION_PATCH));

	oledRefresh();

	usbInit();
	usbLoop();
}

void check_firmware_sanity(void)
{
	int broken = 0;
	if (memcmp((void *)FLASH_META_MAGIC, "TRZR", 4)) { // magic does not match
		broken++;
	}
	if (*((uint32_t *)FLASH_META_CODELEN) < 4096) { // firmware reports smaller size than 4kB
		broken++;
	}
	if (*((uint32_t *)FLASH_META_CODELEN) > FLASH_TOTAL_SIZE - (FLASH_APP_START - FLASH_ORIGIN)) { // firmware reports bigger size than flash size
		broken++;
	}
	if (broken) {
		layoutDialog(DIALOG_ICON_ERROR, NULL, NULL, NULL, "Firmware appears", "to be broken.", NULL, "Unplug your TREZOR", "and see our support", "page at mytrezor.com");
		system_halt();
	}
}

int main(void)
{
	setup();
	memory_protect();
	oledInit();

	// at least one button is unpressed
	uint16_t state = gpio_port_read(BTN_PORT);
	if ((state & BTN_PIN_YES) == BTN_PIN_YES || (state & BTN_PIN_NO) == BTN_PIN_NO) {

		check_firmware_sanity();

		oledClear();
		oledDrawBitmap(40, 0, &bmp_logo64_empty);
		oledRefresh();

		if (!signatures_ok()) {
			show_unofficial_warning();
		}

		load_app();

	}

	bootloader_loop();

	return 0;
}
