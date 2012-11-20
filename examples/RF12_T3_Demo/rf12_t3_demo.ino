/// RF12_T3_Demo.ino
// Example Sketch to show how to use RFM12B library
// 2012: tht https://github.com/tht

#include "RF12_T3.h"

// for debug output to serial port
HardwareSerial Uart = HardwareSerial();

// RFM12b Module on irqLine4, nodeid 21, 868MHz, group 212
RF12_T3 *RF12 = RF12_T3::irqLineT3();


int drssi[32];


/**
 * setup()
 * Prepare Uart for output and send some data
 */
void setup() {
  // init debug output
  Uart.begin(115200);
  Uart.println("Starting...");
  
  // Very slow non JeeNode datarate! Remove 0x40 to get JeeNode defaults.
  RF12->reinit(21, RF12_BAND868MHZ, 212, 0x40);

  // Wait until we're allowed to send, send an empty packet
  while(! RF12->canSend()) ;
  Uart.println("Sending an empty packet...");
  RF12->sendStart(0);

  // Wait until we're allowed to send, send an 4 byte packet
  while(! RF12->canSend()) ;
  Uart.println("Sending a packet...");
  uint32_t data = 0x12345678;
  RF12->sendStart(0, &data, 4);
  
  Uart.flush();
}


/**
 * loop()
 * Check if we received something and output it to serial console.
 */
void loop() {
  if (RF12->recvDone()) {
    int nodeId = RF12->buffer[0] & 0x1F;
    int d = RF12->getDRSSI();

    // check CRC
    if (RF12->rf12_crc == 0)
      Uart.print("OK ");
    else
      Uart.print(" ? ");
      
    Uart.print(RF12->buffer[0], DEC);

    // print content of packet
    for(int i=0; i<RF12->buffer[1]; i++) {
      Uart.print(" ");
      Uart.print(RF12->buffer[i+2], DEC);
    }

    Uart.print(" (");
    Uart.print(d, DEC);
    Uart.print("dB, AFC: ");
    Uart.print(RF12->getAFCOffset(), DEC);
    Uart.println(")");
    Uart.flush();
    
    // Update drssi table
    drssi[nodeId]=d;
      
    // output rssi list
    Uart.print("DRSSI: ");
    for (int i=0; i<32; i++) {
      if (drssi[i]!=0) {
        Uart.print(i, DEC);
        Uart.print(":");
        Uart.print(drssi[i], DEC);
        Uart.print("dB "); 
      }
    }
    Uart.println();
  }
}
