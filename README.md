T3Libs
======

While looking
around for a more powerful alternative I found the new [Teensy 3.0](http://www.kickstarter.com/projects/paulstoffregen/teensy-30-32-bit-arm-cortex-m4-usable-in-arduino-a) on Kickstarter.

I've already some experience with ATmega and ATtiny in the Arduino IDE. To use the Teensy in my existing environment I started to write a RFM12B driver. It's actually my first Arduino library.

I'm not yet sure how this will evolve. I do have plans for my Teensy 3.0 but time is limited...

Thanks for your interest and please send feedback.

Thomas

========
##RFM12_T3##

### Overview ###
A Teensy 3.0 library to communicate wirelessly using a [HopeRF RFM12B](<http://www.hoperf.com>) module. The on-air protocol is compatible with the one used on
[JeeNodes](<http://www.jeelabs.org>). I actually developed it to exchange some messages
with JeeNode clones.

### Required Files ###
Just clone this repository to your `Arduino Sketches/libraries` folder or copy the following two files in your own Sketch folder to make it work:

* RF12_T3.h
* RF12_T3.cpp

### Current State ###
The driver is work-in-progress, a 0.something version. I'm able to send and receive data from other wireless nodes but there is much room for improvements. The external interface will most likely change a bit.

### Wiring ###

	._______.          .------.
	|Teensy |          |RFM12b|
	|     13o---SCK----oSCK   |
	|     12o---MISO---oSDO   |
	|     11o---MOSI---oSDI   |
	|     10o---CS-----oSEL   |
	|      4o---IRQ----oIRQ   |
	'.......'          '......'
Don't forget power and ground :-)

### Addition Informations ###
Most of the information about the RFM12B SPI interface I got from the german site [www.mikrocontroller.net](http://www.mikrocontroller.net/articles/RFM12).

The internal working of the driver is heavily based on jcw's work on [jeelib](https://github.com/jcw/jeelib).