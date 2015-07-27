/*

	Stripped down, simplified, driver for the LCD for the MSP430G2553 + ADS1118 BoosterPack.
	Should work with other hardware as well, if you update the pins and commands.
	Credit to Wayne Xu's LCD driver for the C2000, which served as a guide
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

#include <SPI.h>
#include <msp430g2553.h>
#include "SpiLcd.h"

// Update these pins as necessary for your LCD
#define LCD_CS_PIN      13
#define LCD_RS_PIN      12
#define LCD_RST_PIN     11
#define LCD_SPI_DATAMODE 0

// http://www.newhavendisplay.com/app_notes/ST7032.pdf
#define LCD_COMMAND_CLEAR 				B00000001
#define LCD_COMMAND_CURSOR_HOME			B00000010
#define LCD_COMMAND_ENTRY_MODE 			B00000110
#define LCD_COMMAND_DISPLAY_ON			B00001100
#define LCD_COMMAND_SET_SHIFT 			B00010100
#define LCD_COMMAND_FUNCTION_SET		B00111001

// CGRAM set commands
#define LCD_COMMAND_POWER_CONTROL 		B01010110
#define LCD_COMMAND_FOLLOWER_CONTROL 	B01101101
#define LCD_COMMAND_CONTRAST 			B01110000

#define digittoa(digit) 0x30 + digit

// TODO: Use busy flag instead of delays and clean this up.
void SpiLcd::init(void) {
	SPI.begin();
	SPI.setDataMode(LCD_SPI_DATAMODE);
	
	pinMode (LCD_CS_PIN, OUTPUT);
	pinMode (LCD_RS_PIN, OUTPUT);
	pinMode (LCD_RST_PIN, OUTPUT);

	// Startup sequence from Wayne Xu's C2000 LCD driver.
	delay(4);		    // waiting LCD to power on.
	digitalWrite(LCD_CS_PIN, HIGH);;		    //set CS high
	digitalWrite(LCD_RS_PIN, HIGH);		//set RS high
	digitalWrite(LCD_RST_PIN, LOW);     	//RESET
	delay(2);
	digitalWrite(LCD_RST_PIN, HIGH);    	//end reset
	delay(1);

    // Initialize LCD
	writeCommand(LCD_COMMAND_FUNCTION_SET);
	writeCommand(LCD_COMMAND_POWER_CONTROL);
	writeCommand(LCD_COMMAND_FOLLOWER_CONTROL);
	writeCommand(LCD_COMMAND_CONTRAST);
	writeCommand(LCD_COMMAND_DISPLAY_ON);
	writeCommand(LCD_COMMAND_ENTRY_MODE);
}

void SpiLcd::clear(void) {
	writeCommand(LCD_COMMAND_CLEAR);
	writeCommand(LCD_COMMAND_CURSOR_HOME);
}

void SpiLcd::setPointer(uint8_t line, uint8_t position) {
	uint8_t pointer = line == 0 ? 0x80 : 0xC0;
	writeCommand(pointer + position);
	delay(1);
}

void SpiLcd::displayString( char *str) {
	while (*str) {
	    writeByte(*str++);
	}
}

// Displays a number that has been scaled by some factor.
// displayNumber(123, 0)    => "123"
// displayNumber(123, 1)    => "123"
// displayNumber(123, 10)   => "12.3"
// displayNumber(123, 100)  => "1.23"
// displayNumber(123, 1000) => "0.123"
// displayNumber(123, 10000) => "0.0123"
void SpiLcd::displayNumber(int32_t number, int32_t scaleFactor) {
    if (number == 0) {
        displayString("0.0");
        return;
    } else if (number < 0) {
        writeByte('-');
        number *= -1;
    }

    if (number < scaleFactor) {
        // Number is less than 1, add a leading 0
        displayString("0.");

        scaleFactor /= 10;

        while (number < scaleFactor) {
            writeByte('0');
            scaleFactor /= 10;
        }

        // Scaling taken care of, zero out scale factor.
        scaleFactor = 0;
    }

    // Zero out scaleFactor if unscaled, to avoid needing to test this in a loop later.
    if (scaleFactor == 1) {
        scaleFactor = 0;
    }

    // Find the number of digits to display
    uint8_t digits = 0;
    uint32_t power = 1;

    while (power <= number) {
        power *= 10;
        digits++;
    }

    // Above loop overshoots by one. Could just make the loop smarter, but this works fine and is probably faster.
    power /= 10;

    while (power >= 1) {
        writeByte(digittoa((number / power) % 10));

        if (power == scaleFactor) {
            writeByte('.');
        }

        power /= 10;
    }
}

void SpiLcd::writeByte(uint8_t byte) {
    SPI.setDataMode(LCD_SPI_DATAMODE);
	
	digitalWrite(LCD_CS_PIN, LOW);
	digitalWrite(LCD_RS_PIN, HIGH);
	SPI.transfer(byte);
	digitalWrite(LCD_CS_PIN, HIGH);
	
	delay(1);
}

void SpiLcd::writeCommand(uint8_t command) {
	SPI.setDataMode(LCD_SPI_DATAMODE);
	
	digitalWrite(LCD_CS_PIN, LOW);
	digitalWrite(LCD_RS_PIN, LOW);
	SPI.transfer(command);
	digitalWrite(LCD_CS_PIN, HIGH);
	
	delay(1);
}

void SpiLcd::delay(uint8_t duration) {
	uint32_t now = millis();
	while (millis() < now + duration);
}
