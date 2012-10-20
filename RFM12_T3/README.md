RFM12_T3
========

## Overview ##
A Teensy 3.0 library to communicate wirelessly using a RFM12B module
(<http://www.hoperf.com>). The on-air protocol is compatible with the one used on
JeeNodes (<http://www.jeelabs.org>). I actually developed it to exchange some messages
with JeeNode clones.

## Current State ##
The driver is work-in-progress, a 0.something version. I'm able to send and receive data
from other wireless nodes but there is much room for improvements. The external interface
will most likely change a bit.

## Wiring ##

	._______.          .------.
	|Teensy |          |RFM12b|
	|     13o---SCK----oSCK   |
	|     12o---MISO---oSDO   |
	|     11o---MOSI---oSDI   |
	|     10o---CS-----oSEL   |
	|      4o---IRQ----oIRQ   |
	'.......'          '......'
Don't forget power and ground :-)

## Addition Informations ##
Most of the information about the RFM12B SPI interface I got from this (german) site:
<http://www.mikrocontroller.net/articles/RFM12>.