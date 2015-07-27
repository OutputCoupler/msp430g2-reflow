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

#ifndef __ADS1118
#define __ADS1118

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

class ADS1118
{
	public:
		void begin();
		
		int32_t readCelsius();
		int32_t readFahrenheit();
		
	private:
		int32_t getRawTemp();
		int32_t getTempFromCode(int32_t);
		int32_t getLocalCompensation(int32_t);
		
		int32_t writeAds(uint32_t);
		int32_t readAds(uint8_t);
};

#ifdef __cplusplus
}
#endif

#endif /* __ADS1118 */