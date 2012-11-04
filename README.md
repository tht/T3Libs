T3Libs
======

While looking
around for a more powerful alternative to an Arduino I found the new [Teensy 3.0](http://www.kickstarter.com/projects/paulstoffregen/teensy-30-32-bit-arm-cortex-m4-usable-in-arduino-a) on Kickstarter. I've already some experience with ATmega and ATtiny in the Arduino IDE. To use the Teensy in my existing environment I started to write a RFM12B driver. It's actually my first Arduino library.

I'm not yet sure how this will evolve. I do have plans for my Teensy 3.0 but time is limited... Thanks for your interest and please send feedback.

Thomas

========
##RFM12_T3##

### Overview ###
A Teensy 3.0 library to communicate wirelessly using a [HopeRF RFM12B](<http://www.hoperf.com>) module. The on-air protocol is compatible with the one used on
[JeeNodes](<http://www.jeelabs.org>). I actually developed it to exchange some messages
with JeeNode clones.

### Features ###
* Completely compatible with [JeeNodes](<http://www.jeelabs.org>)
* Mature interrupt handling based on RFM12B state
* Very fast interrupt handling
* Uses hardware CRC unit to calculate and verify CRCs
* *Creative* use of the DRSSI bit allows a rough estimate of the signal strength (`getDRSSI()`)
* Reports AFC offset while receiving data (`getAFCOffset()`)

### Disadvantages ###
* Driver *assumes* it's the only one doing SPI. Using anything else with SPI will most likely break stuff.
* Driver *assumes* no one uses hardware CRC unit. Use it and stuff will break.

Must likely there are some more problems hiding. See [github issues](<https://github.com/tht/T3Libs/issues>).


### Required Files ###
Just clone this repository to your `Arduino Sketches/libraries` folder or copy the following two files in your own Sketch folder to make it work:

* RF12_T3.h
* RF12_T3.cpp

### Current State ###
The driver is work-in-progress, a 0.something version. I'm able to send and receive data from other wireless nodes and internal error handling is improving fast. There is still room for improvements but the driver is working stable. The external interface will most likely change a bit soon.

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

### Where To Get The Parts? ###
* [Teensy 3.0](http://www.pjrc.com/store/teensy3.html) (available soon)
* RFM12b module (I highly recommend a breakout board as it has non-standard pin spacing)
	* [868MHz at JeeLabs.com](http://jeelabs.com/products/rfm12b-board) (only legal in Europe as far as I know)
	* [915MHz at ModernDevice.com](http://shop.moderndevice.com/products/rfm12b-board) (only legal in U.S. and Australia as far as I know)
	* There are many more shops and also a 443MHz
 * I do recommend a solderless breadboard and some wires to connect the stuff.

### Addition Informations ###
Most of the information about the RFM12B SPI interface I got from the german site [www.mikrocontroller.net](http://www.mikrocontroller.net/articles/RFM12).

The internal working of the driver is heavily based on jcw's work on [jeelib](https://github.com/jcw/jeelib).