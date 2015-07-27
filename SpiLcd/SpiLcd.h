/*

	Stripped down, simplified, driver for the LCD on an ADS1118 BoosterPack.
	Might work with other SPI LCD displays as well, I haven't tested.
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

#ifndef __SPI_LCD
#define __SPI_LCD

#ifdef __cplusplus
extern "C"
{
#endif

class SpiLcd
{
	public:
        void init(void);
        void clear(void);
        
        void setPointer(unsigned char line, unsigned char position);
        
        void displayString( char *str);
        void displayNumber(int32_t number, int32_t scaleFactor);

	private:
        void writeByte(unsigned char byte);
        void writeCommand(unsigned char command);
		
        void delay(unsigned char millis);
};

#ifdef __cplusplus
}
#endif

#endif /* __SPI_LCD */
