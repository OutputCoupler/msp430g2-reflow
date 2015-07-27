/*

	Stripped down, simplified, ADS1118 driver. TI's ADS1118 sample code used as a reference.
	Copyright (C) 2015 William McGloon

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "ADS1118.h"

#include <stdint.h>
#include <SPI.h>

#define ADS_DATA_MODE       1
#define ADS_READ_FREQUENCY  50

#define ADS1118_CH0 (0x0000)
#define ADS1118_CH1 (0x3000)
#define ADS1118_TS  (0x0010)

// These are copied from the ADS1118 sample code. ADSCON_CH1 is not used, currently.
//Set the configuration to AIN0/AIN1, FS=+/-0.256, SS, DR=128sps, PULLUP on DOUT
#define ADSCON_CH0		(0x8B8A)
//Set the configuration to AIN2/AIN3, FS=+/-0.256, SS, DR=128sps, PULLUP on DOUT
#define ADSCON_CH1		(0xBB8A)

#define ADS_CS_LOW      digitalWrite(8, LOW)
#define ADS_CS_HIGH     digitalWrite(8, HIGH)

static  int32_t localData   = 0;
static  int32_t farData     = 0;
static uint32_t lastRead    = 0;
static  uint8_t readLocal   = 0;  // 0 == Far data is read, 1 == Local data is read. Toggles on each read.

void ADS1118::begin() {
	SPI.begin();
	pinMode(8, OUTPUT);

	writeAds(ADSCON_CH0 + ADS1118_TS);

	localData = readAds(1);
	readLocal = 0;
}

int32_t ADS1118::readCelsius() {
	unsigned long now = millis();

	if((now - lastRead) > ADS_READ_FREQUENCY) {
		if (!readLocal) {
			farData = readAds(0);
		} else {
			localData = readAds(1);
		}

		lastRead = now;
		readLocal = !readLocal;
	}

	return getTempFromCode(farData + getLocalCompensation(localData));
}

int32_t ADS1118::readFahrenheit() {
	return (readCelsius() * 9 / 5) + 32.0;
}

// Local compensation and temp conversion is more or less identical to what was in the sample code, just cleaned up.
int32_t	ADS1118::getLocalCompensation(int32_t adcCode)
{
	float temp = ((float)(adcCode / 4)) / 32.0;

	if (temp >= 0 && temp <= 5) {
		return (0x0019 * temp) / 5;
	} else if (temp > 5 && temp <= 10) {
		return (0x001A * (temp - 5))  /  5 + 0x0019;
	} else if (temp > 10 && temp <= 20) {
		return (0x0033 * (temp - 10)) / 10 + 0x0033;
	} else if (temp > 20 && temp <= 30) {
		return (0x0034 * (temp - 20)) / 10 + 0x0066;
	} else if (temp > 30 && temp <= 40) {
		return (0x0034 * (temp - 30)) / 10 + 0x009A;
	} else if (temp > 40 && temp <= 50) {
		return (0x0035 * (temp - 40)) / 10 + 0x00CE;
	} else if (temp > 50 && temp <= 60) {
		return (0x0035 * (temp - 50)) / 10 + 0x0103;
	} else if (temp > 60 && temp <= 80) {
		return (0x006A * (temp - 60)) / 20 + 0x0138;
	} else if (temp > 80 && temp <= 125) {
		return (0x00EE * (temp - 80)) / 45 + 0x01A2;
	}
	
	return 0;
}

// TODO: The MSP430 does NOT like these floats. Do it with ints.
int32_t ADS1118::getTempFromCode(int32_t code) {
	if (code > 0xFF6C && code <= 0xFFB5) {
		return 10 * ((float)(15 * ((float)code - 0xFF6C)) / 0x0049 - 30.0f);
	} else if (code > 0xFFB5 && code <= 0xFFFF) {
		return 10 * ((float)(15 * ((float)code - 0xFFB5)) / 0x004B - 15.0f);
	} else if (code >= 0 && code <= 0x0019) {
		return 10 * ((float)(5  * ((float)code - 0x0000)) / 0x0019);
	} else if (code > 0x0019 && code <= 0x0033) {
		return 10 * ((float)(5  * ((float)code - 0x0019)) / 0x001A + 5.0f);
	} else if (code > 0x0033 && code <= 0x0066) {
		return 10 * ((float)(10 * ((float)code - 0x0033)) / 0x0033 + 10.0f);
	} else if (code > 0x0066 && code <= 0x009A) {
		return 10 * ((float)(10 * ((float)code - 0x0066)) / 0x0034 + 20.0f);
	} else if (code > 0x009A && code <= 0x00CE) {
		return 10 * ((float)(10 * ((float)code - 0x009A)) / 0x0034 + 30.0f);
	} else if (code > 0x00CE && code <= 0x0103) {
		return 10 * ((float)(10 * ((float)code - 0x00CE)) / 0x0035 + 40.0f);
	} else if (code > 0x0103 && code <= 0x0138) {
		return 10 * ((float)(10 * ((float)code - 0x0103)) / 0x0035 + 50.0f);
	} else if (code > 0x0138 && code <= 0x01A2) {
		return 10 * ((float)(20 * ((float)code - 0x0138)) / 0x006A + 60.0f);
	} else if (code > 0x01A2 && code <= 0x020C) {
		return 10 * ((float)(20 * ((float)code - 0x01A2)) / 0x006A + 80.0f);
	} else if (code > 0x020C && code <= 0x02DE) {
		return 10 * ((float)(40 * ((float)code - 0x020C)) / 0x00D2 + 100.0f);
	} else if (code > 0x02DE && code <= 0x03AC) {
		return 10 * ((float)(40 * ((float)code - 0x02DE)) / 0x00CE + 140.0f);
	} else if (code > 0x03AC && code <= 0x0478) {
		return 10 * ((float)(40 * ((float)code - 0x03AB)) / 0x00CD + 180.0f);
	} else if (code > 0x0478 && code <= 0x0548) {
		return 10 * ((float)(40 * ((float)code - 0x0478)) / 0x00D0 + 220.0f);
	} else if (code > 0x0548 && code <= 0x061B) {
		return 10 * ((float)(40 * ((float)code - 0x0548)) / 0x00D3 + 260.0f);
	} else if (code > 0x061B && code <= 0x06F2) {
		return 10 * ((float)(40 * ((float)code - 0x061B)) / 0x00D7 + 300.0f);
	} else if (code > 0x06F2 && code <= 0x07C7) {
		return 10 * ((float)(40 * ((float)code - 0x06F2)) / 0x00D5 + 340.0f);
	} else if (code > 0x07C7 && code <= 0x089F) {
		return 10 * ((float)(40 * ((float)code - 0x07C7)) / 0x00D8 + 380.0f);
	} else if (code > 0x089F && code <= 0x0978) {
		return 10 * ((float)(40 * ((float)code - 0x089F)) / 0x00D9 + 420.0f);
	} else if (code > 0x0978 && code <= 0x0A52) {
		return 10 * ((float)(40 * ((float)code - 0x0978)) / 0x00DA + 460.0f);
	}
	
	return 10 * 0xA5A5;
}

int32_t ADS1118::writeAds(uint32_t value) {
	SPI.setDataMode(ADS_DATA_MODE);
	value |= 0x8000;

	ADS_CS_LOW;
	
	int msb = SPI.transfer(value >> 8);
    msb = (msb << 8) | SPI.transfer(value & 0xFF);
    SPI.transfer(value >> 8);
    SPI.transfer(value & 0xFF);
	
	ADS_CS_HIGH;

	return msb;
}

int32_t ADS1118::readAds(uint8_t readInternal) {
	return writeAds(ADSCON_CH0 + (readInternal ? 0 : ADS1118_TS));
}
