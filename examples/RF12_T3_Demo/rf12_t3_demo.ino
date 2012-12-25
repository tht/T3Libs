/// RF12_T3_Demo.ino
// Example Sketch to show how to use RFM12B library
// 2012: tht https://github.com/tht

#include "RF12_T3.h"

// comment out following line to get JeeNode default datarate
#define SPEED 0x40  // 5k305

// RFM12b Module on irqLine4, nodeid 21, 868MHz, group 212
RF12_T3 *RF12 = RF12_T3::irqLine4();

/**
 * setup()
 * Prepare Uart for output and send some data
 */
void setup() {
  // init debug output

  // Serial needs a delay after power-on
  delay(1000);
  Serial.begin(57600);
  Serial.println("Starting...");

  // initialize RFM module  
#ifdef SPEED
  RF12->reinit(21, RF12_BAND868MHZ, 212, 0x40);
#else
  RF12->reinit(21, RF12_BAND868MHZ, 212);
#endif

  // Wait until we're allowed to send, send an empty packet
  while(! RF12->canSend()) ;
  Serial.println("Sending an empty packet...");
  RF12->sendStart(0);

  // Wait until we're allowed to send, send an 4 byte packet
  while(! RF12->canSend()) ;
  Serial.println("Sending a packet...");
  uint32_t data = 0x12345678;
  RF12->sendStart(0, &data, 4);
  
  Serial.flush();
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
      Serial.print("OK ");
    else
      Serial.print(" ? ");
      
    Serial.print(RF12->buffer[0], DEC);

    // print content of packet
    for(int i=0; i<RF12->buffer[1]; i++) {
      Serial.print(" ");
      Serial.print(RF12->buffer[i+2], DEC);
    }

    Serial.print(" (");
    Serial.print(d, DEC);
    Serial.print(", AFC: ");
    Serial.print(RF12->getAFCOffset(), DEC);
    Serial.println(")");
    Serial.flush();
  }
}
