/*
	Reflow oven controller based on the MSP430G2553 and ADS1118 BoosterPack.
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

#include "Energia.h"

#include <QueueList.h>
#include <SPI.h>
#include <PID_v1.h>

#include "ADS1118.h"
#include <SpiLcd.h>

#define DISPLAY_REFRESH_DELAY 50

// This software records a series of measurements to look back at the temperature history of the oven.
// Using more, longer frames results in more stable output, at the cost of increased response time.
// More than 12 measurements and it will run out of memory.
#define WINDOW_SIZE 10
#define FRAME_DURATION 250

#define STATE_IDLE 		0
#define STATE_START 	1
#define STATE_PREHEAT 	2
#define STATE_RAMP 		3
#define STATE_REFLOW 	4
#define STATE_COOLDOWN 	5

#define BUTTON_PIN 9
#define DEBOUNCE_DELAY 20

#define RELAY_PIN 18
#define FAN_PIN 19
#define RELAY_ON digitalWrite(RELAY_PIN, HIGH)
#define RELAY_OFF digitalWrite(RELAY_PIN, LOW)

// These values are best guess, and might be no better than random. But it works. Temperatures are tenths of a degree C.
// During startup we want to heat up fast, and stop fast. A small error isn't a big deal. Low ki.
#define STARTUP_KP 1.000
#define STARTUP_KI 0.001
#define STARTUP_KD 1.000
#define STARTUP_TARGET_TEMP 1350

// During preheat, we want a very stable temperature rise, with a very low constant error. Stopping fast not as important. Higher ki, lower kd.
#define PREHEAT_KP 1.000
#define PREHEAT_KI 0.100
#define PREHEAT_KD 0.500
#define PREHEAT_TARGET_RISE 4
#define PREHEAT_TARGET_TEMP 1550

// During ramp, we want to heat up fast and stop fast. More concerned with small errors than during startup.
#define RAMP_KP 1.000
#define RAMP_KI 0.100
#define RAMP_KD 2.000
#define RAMP_TARGET_TEMP 2050

// During reflow, we want to hold temperature stable. Small errors not very important.
#define REFLOW_KP 1.000
#define REFLOW_KI 0.001
#define REFLOW_KD 0.500
#define REFLOW_TIME 30000

#define COOLDOWN_TARGET_TEMP 1350

// Some macros to aid readability in the state machine
#define CONTROLLING_TEMP controllingTemp = 1
#define NOT_CONTROLLING_TEMP controllingTemp = 0

uint8_t controllingTemp;

void setup();
void loop();
void updateDisplay();
void controlHeater();
int32_t getRateOfRise();

ADS1118 ads;
SpiLcd lcd;

double input, output, setpoint;
PID pid(&input, &output, &setpoint, STARTUP_KP, STARTUP_KI, STARTUP_KD, DIRECT);

uint8_t state = STATE_IDLE;

int32_t celsius;
uint32_t now;

uint32_t lastDisplay = 0;

uint32_t buttonDebounceStart = 0;
uint8_t suppressButton = 0;

uint32_t reflowEnd;

uint8_t currentWindow;
uint8_t currentFrame;

// TODO: This uses a ton of memory, try a geometric moving average instead?
QueueList<uint32_t> readingTimes;
QueueList<int32_t> readings;

uint32_t lastReadingAt = 0;
uint32_t oldestReadingAt = 0;

int32_t lastReading = 0;
int32_t oldestReading = 0;

uint32_t displayUpdatedAt = 0;

void setup() {
	pinMode(RELAY_PIN, OUTPUT);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	
	lcd.init();
	lcd.clear();
	ads.begin();
	
	pid.SetMode(AUTOMATIC);
	pid.SetOutputLimits(0, WINDOW_SIZE);
	pid.SetSampleTime(50);
	
	currentWindow = 0;
	currentFrame = 0;
}

void loop() {
	celsius = ads.readCelsius();
	now = millis();

	switch (state) {
		case STATE_IDLE:
			NOT_CONTROLLING_TEMP;
			
			if (isButtonPressed()) {
				state = STATE_START;
				pid.SetTunings(STARTUP_KP, STARTUP_KI, STARTUP_KD);
				setpoint = STARTUP_TARGET_TEMP;
			}
			
			break;
		
		case STATE_START:
            CONTROLLING_TEMP;

			input = celsius;
			
			if (celsius >= STARTUP_TARGET_TEMP) {
				state = STATE_PREHEAT;
				pid.SetTunings(PREHEAT_KP, PREHEAT_KI, PREHEAT_KD);
				setpoint = PREHEAT_TARGET_RISE;
			}
			
			break;
		
		case STATE_PREHEAT:
            CONTROLLING_TEMP;

			input = getRateOfRise() / 10;
			
			if (celsius >= PREHEAT_TARGET_TEMP) {
				state = STATE_RAMP;
				pid.SetTunings(RAMP_KP, RAMP_KI, RAMP_KD);
				setpoint = RAMP_TARGET_TEMP;
			}
			
			break;
		
		case STATE_RAMP:
            CONTROLLING_TEMP;

			input = celsius;
			
			if (celsius >= RAMP_TARGET_TEMP) {
				state = STATE_REFLOW;
				pid.SetTunings(REFLOW_KP, REFLOW_KI, REFLOW_KD);
				reflowEnd = now + REFLOW_TIME;
			}
			
			break;
		
		case STATE_REFLOW:
            CONTROLLING_TEMP;

			input = celsius;
			
			if (now >= reflowEnd) {
				state = STATE_COOLDOWN;
			}
			
			break;
		
		case STATE_COOLDOWN:
            NOT_CONTROLLING_TEMP;

			if (celsius <= COOLDOWN_TARGET_TEMP) {
				state = STATE_IDLE;
			}
			break;
			
		default:
			state = STATE_IDLE;
	}
	
	pid.Compute();
	controlHeater();
	
	if (now < lastDisplay + FRAME_DURATION) {
		return;
	}

	recordTemp();
	updateDisplay();
	
	lastDisplay = now;
}

void controlHeater() {
	if (!controllingTemp) {
		RELAY_OFF;
		return;
	}
	
	currentWindow = (uint8_t)output;
	if (currentWindow > currentFrame) {
		RELAY_ON;
	} else {
		RELAY_OFF;
	}
	
	currentFrame++;
	if (currentFrame >= WINDOW_SIZE) {
		currentFrame = 0;
	}
}

uint8_t isButtonPressed() {
	int button = digitalRead(BUTTON_PIN);
	if (button == LOW) {
		if (buttonDebounceStart == 0) {
			buttonDebounceStart = now;
		} else if (!suppressButton && now > buttonDebounceStart + DEBOUNCE_DELAY) {
			suppressButton = 1;
			buttonDebounceStart = 0;
			return 1;
		}
	} else {
		suppressButton = 0;
	}
	
	return 0;
}

void updateDisplay() {
	lcd.clear();
    lcd.setPointer(0, 0);
	lcd.displayNumber(celsius, 10);
	lcd.displayString("C");

	int32_t rate = getRateOfRise();
	lcd.setPointer(0, 7);

	if (rate > 0) lcd.displayString("+");
	lcd.displayNumber(rate, 100);
	lcd.displayString("C/s");

	lcd.setPointer(1, 0);
	switch (state) {
		case STATE_IDLE:
			lcd.displayString("  Press start");
			break;

		case STATE_START:
			lcd.displayString("Start => ");
			lcd.displayNumber(STARTUP_TARGET_TEMP, 10);
			lcd.displayString("C");
			break;

		case STATE_PREHEAT:
			lcd.displayString("P.heat  +");
			lcd.displayNumber(PREHEAT_TARGET_RISE, 10);
			lcd.displayString("C/s");
			break;

		case STATE_RAMP:
			lcd.displayString("Ramp  => ");
			lcd.displayNumber(RAMP_TARGET_TEMP, 10);
			lcd.displayString("C");
			break;

		case STATE_REFLOW:
			lcd.displayString("Reflow: ");
			lcd.displayNumber((reflowEnd - now) / 1000, 100);
			break;

		case STATE_COOLDOWN:
			lcd.displayString("!Open the Door!");
			break;
	}
}

void recordTemp() {
	readingTimes.push(now);
	readings.push(celsius);
	
	lastReadingAt = now;
	lastReading = celsius;
	
	if (readingTimes.count() == 1) {
		oldestReadingAt = lastReadingAt;
		oldestReading = lastReading;
	} else if (readingTimes.count() > WINDOW_SIZE) {
		oldestReadingAt = readingTimes.pop();
		oldestReading = readings.pop();
	}
}

// Returns hundredths of a degree per second
int32_t getRateOfRise() {
	if (readings.count() <= 1) {
		return 0.0;
	}

	int32_t tempDiff = (lastReading - oldestReading) * 10 * 1000;
	int32_t timeDiff = lastReadingAt - oldestReadingAt;

	if (timeDiff == 0) {
		return 0;
	}

	return tempDiff / timeDiff;
}

