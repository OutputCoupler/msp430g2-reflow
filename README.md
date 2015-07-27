# msp430g2-reflow
Reflow oven controller for the MSP430G2553 and ADS1118 BoosterPack

The relay is connected to pin 18 (XOUT). Start the oven by pressing
the bottom button on the BoosterPack. The top button does nothing, 
currently.

The solder profile is currently set for leaded solder, but can be
adjusted via a series of #defines that set the desired temperatures,
rate of rise and PID controller parameters.

Requires the Arduino PID and QueueList lirbaries.

