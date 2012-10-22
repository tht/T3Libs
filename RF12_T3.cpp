/// RF12_t3.cpp
// Implementation file for RFM12B library for Teensy 3.0
// 2012: tht https://github.com/tht

#include "RF12_T3.h"

// Init singleton cariable
RF12_T3* RF12_T3::_instance = 0; // init singleton


/**
 * reinit(uint8_t id, uint8_t band, uint8_t group, uint8_t rate)
 * (Re)initializes the RFM12 module with supplied arguments. It
 * actually requests a reset from the RFM12 module and waits until
 * it responds back. Use isAvailable() to check if init was successful.
 * Mode will be RF_IDLE after successful initialization.
 * Params: id:   NodeId of this node
 *         band: Frequency band (see constants in this file)
 *         group: RFM12 group (second sync byte)
 *         rate: Datarate to use
 * Return: void
 */
int RF12_T3::reinit(uint8_t id, uint8_t band, uint8_t group, uint8_t rate) {
  nodeId = id;
  groupId= group;
  bandId = band;
  datarate = rate;
  available = 0;
  reportBroken = 0;

  // configure SPI pins
  digitalWrite(SCK, LOW);
  digitalWrite(MOSI, LOW);
  digitalWrite(SS, HIGH);
  pinMode(SCK, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SS, OUTPUT); // is pin 10

  // enables and configures SPI module
  SIM_SCGC6 |= SIM_SCGC6_SPI0;  // enable crc clock
  SPI0_MCR = 0x80004000;
  SPCR |= _BV(MSTR);
  SPCR |= _BV(SPE);
  SPCR &= ~(_BV(DORD));   // MSBFIRST SPI

  // 12MHz 16bit transfers on CTAR0
  SPI0_CTAR0 = 0xF8010000;

  // 2MHz 8bit transfers on CTAR1 (for reading FIFO)
  SPI0_CTAR1 = 0x38010002;

  // init some values
  _index = 0;
  buffer[0] = 0;  // reset header
  buffer[1] = 0;  // reset len

  // register irq
  pinMode(irqLine, INPUT);
  attachInterrupt(irqLine, RF12_T3::_handleIrq4, LOW);

  // requesting RFM12b reset
  rf12_xfer(0xCA82); // enable software reset
  rf12_xfer(0xFE00); // do software reset
}


/**
 * rf12_xfer(uint16_t data)
 * Transmits one 16bit command to the RFM12 module.
 * Params: data: Command to send
 * Return: From RFM12 module received data
 */
inline uint16_t RF12_T3::rf12_xfer(uint16_t data) {
  digitalWrite(10, LOW);
  SPI0_PUSHR = (1<<26) | data;    // send data (clear transfer counter)
  while (! SPI0_TCR) ; // loop until transfer is complete
  digitalWrite(10, HIGH);
  return SPI0_POPR;
}


/**
 * handleIrq()
 * Handles an IRQ request from the RFM12b module.
 * Params: none (called from interrupt)
 * Return: void
 */
void RF12_T3::handleIrq() {
  // check if we really have to do something
  if (digitalRead(4) == HIGH)
    return;

  // reading state
  digitalWrite(10, LOW);  // select RFM12b module
  SPI0_PUSHR = (1<<26) | 0x0000;    // send data (clear transfer counter)
  while (! SPI0_TCR) ; // loop until transfer is complete
  uint16_t res = SPI0_POPR;

  // fifo full or buffer empty
  if (res & 0x8000) {
    
    // =====================================================
    // FIFO has a byte to read
    if (state == RF_RECV) {
      SPI0_PUSHR = (1<<28) | (1<<26); // CTAR1 transfer (slow 8bit), clear transfer counter
      while (! SPI0_TCR) ; // loop until transfer is complete
      uint8_t data = (uint8_t) SPI0_POPR;
      digitalWrite(10, HIGH);

      // save data to internal buffer
      buffer[_index++] = data;

      if (_index==1) {          // first packet!
        rf12_crc = crc16_update(0xffff, groupId); // network group id
        rf12_crc = crc16_update(rf12_crc, data);

      } else if (_index==2) {   // second packet (with length)
        rf12_crc = crc16_update(rf12_crc, data);

      } else {                  // data (or crc)
        rf12_crc = crc16_update(rf12_crc, data);

        // abort reception if we got a full packet
        if (_index > buffer[1] + 3) { // +1 for header, +2 for checksum
          disableReceiver();
          _recvDone = 1;
        }
      }

    // =====================================================
    // Buffer needs neyt byte to send
    } else {
      digitalWrite(10, HIGH);

      rf12_xfer(0xB800 | _toSend);
      
      // prepare next byte to send
      if (state < 0) {
        _toSend = buffer[2 + buffer[1] + state++];
        rf12_crc = crc16_update(rf12_crc, _toSend);
      } else {
        switch (++state) {
          case RF_TXSYN1: _toSend = 0x2D; break;
          case RF_TXSYN2: _toSend = groupId;
                          rf12_crc = crc16_update(0xffff, groupId);
                          state = - (2 + buffer[1]);
                          break;
          case RF_TXCRC1: _toSend = 0xff & rf12_crc; break;
          case RF_TXCRC2: _toSend = 0xff & (rf12_crc>>8);   break;
          case RF_TXTAIL1: _toSend = 0xAA; break; // dummy
          case RF_TXTAIL2: break; // dummy
          case RF_TXDONE: disableTransmitter(); // switch off transmitter, fall through
          default:        _toSend = 0x99;
        }
      }
    }
  } else
    digitalWrite(10, HIGH);  // don't forget to disable CS to RFM module

  // =====================================================
  // Power-On reset complete, do init now
  if (res & 0x4000) {
    rf12_xfer(0x80C7 | (bandId << 4)); // configuration settings
    rf12_xfer(0x82D9); 
    rfMode = 0x82D9; rfMode = 0x82D9; // rx enabled, wakeup disabled, lowbat disabled
    rf12_xfer(0xA640);
    rf12_xfer(0xC600 | datarate);
    rf12_xfer(0x94A2);
    rf12_xfer(0xC2AC);
    rf12_xfer(0xCA83);
    rf12_xfer(0xCE00 | groupId); // sync Byte (group id)
    rf12_xfer(0xC483);
    rf12_xfer(0x9850);
    rf12_xfer(0xCC57);
    //rf12_xfer(0xE000); // no wakeup timer
    rf12_xfer(0xC800);
    //rf12_xfer(0xC049); // no lowbat detection
    available = 1;
    state = RF_IDLE;
  }

  // =====================================================
  // Wakeup-call from RFM12b module
  if (res & 0x1000) { // wakeup-call, mark as received and switch off wakeup-timer
    wakeup = 1;
    rf12_sleep(0);
  }

  // =====================================================
  // FIFO overflow or buffer underrun
  if (res & 0x2000) {
    if (bitRead(rfMode, 7)) {  // we are receiving
      resetReceiveBuffer();
      disableReceiver();
      enableReceiver();
      
    } else {                  // we are sending
      // Resending could jam the air... so just abort sending
      // TODO: recheck when sendWait() is implemented to not hang here
      disableTransmitter();
    }
  }
}


/**
 * enableReceiver()
 * Enables the receiver circuit. Disables transmitter if enabled.
 * Also switches internal state to RF_RECV.
 * Params: none
 * Return: void
 */
void RF12_T3::enableReceiver() {
  bitSet(rfMode,  7);   // enable receiver
  bitSet(rfMode,  6);   // enable recv. baseband
  bitClear(rfMode,5);   // disable transmitter
  rf12_xfer(rfMode);
  state = RF_RECV;
}


/**
 * disableReceiver()
 * Disables the receiver circuit. Does not touch transmiter.
 * Params: none
 * Return: void
 */
void RF12_T3::disableReceiver() {
  bitClear(rfMode,7);
  rf12_xfer(rfMode);
}


/**
 * enableTransmitter()
 * Enables the transmitter and completely disables receiver. We send
 * a dummy 0xCC for clock syncronisation first. RFM module will use an
 * interrupt ro request first (real) byte to send.
 * Params: none
 * Return: void
 */
void RF12_T3::enableTransmitter() {
  bitSet(rfMode, 5);   // enable transmitter
  bitClear(rfMode, 7); // disable receiver
  bitClear(rfMode, 6); // disable recv. baseband
  rf12_xfer(rfMode);
  
  // prepare first byte (sync) to send and switch to correct state
  _toSend = 0xCC;
  state = RF_TXPRE; 
}


/**
 * disableTransmitter()
 * Disables the transmitter, does not touch receiver. State is reset to
 * RF_IDLE so the next call to recvDone() will enable reception.
 * Params: none
 * Return: void
 */
void RF12_T3::disableTransmitter() {
  bitClear(rfMode, 5);  // disable transmitter
  rf12_xfer(rfMode);
  state = RF_IDLE;
}


/**
 * canSend()
 * Check if we can send now. It switches off receiver, so a call to
 * recvDone() is needed to enable receiving again.
 * Params: none
 * Return: true if sending is possible/allowed
 */
boolean RF12_T3::canSend() {
  if (available & (state == RF_IDLE || state == RF_RECV)) {
    disableReceiver();
    state = RF_IDLE;
    return true;
  } else {
    return false;
  }
}


/**
 * sendStart(int hdr)
 * Sends an empty packet (only header).
 * Params: hdr: Header to send
 * Return: void
 */
void RF12_T3::sendStart(uint8_t hdr) {
  sendStart(hdr, 0, 0);
}


/**
 * sendStart(int hdr, const void *ptr, int len)
 * Sends an empty packet (only header).
 * Params: hdr: Header to send
 *         ptr: Pointer to data which is copied to internal memory
 *         len: Len of data where ptr points to
 * Return: void
 */
void RF12_T3::sendStart(uint8_t hdr, const void *ptr, uint8_t len) {
  state = RF_IDLE;
  buffer[0] = hdr;
  buffer[1] = len;
  memcpy((void*) &buffer[2], ptr, len);
  rf12_crc = crc16_update(0xffff, groupId);
  enableTransmitter();
}


/**
 * crc16_update(int crc, int a)
 * Appends a new byte to the existing checksum in crc. Uses same format as in JeeLib.
 * Params: crc: Old crc value or seed on initial run
 *         a;   New data to feed into crc
 * Return: new crc value
 */
inline uint16_t RF12_T3::crc16_update(uint16_t crc, uint8_t a) {
  crc ^= a;
  for (uint8_t i = 0; i < 8; ++i) {
    if (crc & 1)  crc = (crc >> 1) ^ 0xA001;
    else          crc = (crc >> 1);
  }
  return crc;
}


/**
 * rf12_sleep(unsigned long m)
 * Requests an interrupt from the RFM12 module after this many ms. The interrupts do not
 * repeat itself (non periodic). Requesting a new one if another one is still wating clears
 * the old one.
 * Set to 0 ms to disable RFM12 wakeup.
 * Params: m: Time to wait in ms or 0 to disable
 * Return: void
 */
inline void RF12_T3::rf12_sleep(unsigned long m) {
  // calculate parameters for RFM12 module
  // T_wakeup[ms] = m * 2^r
  char r=0;
  while (m > 255) {
    r  += 1;
    m >>= 1;
  }

  // Disable old one if present
  if (bitRead(rfMode,1)) {  
    bitClear(rfMode,1);
    rf12_xfer(rfMode);
  }
  
  // enable wakeup call if we have to
  if (m>0) {
    bitSet(rfMode,1);
    rf12_xfer(rfMode);
  
    // write time to wakeup-register
    rf12_xfer(0xE000 | (r<<8) | m);
  }
}


/**
 * gotWakeup()
 * Returns true if wakeup-call was received from RFM12 module and resets itself.
 * Params: none
 * Return: true if wakeup-call was received
 */
inline boolean RF12_T3::gotWakeup() {
  if (wakeup) {
    wakeup = 0;
    return 1;
  }
  return 0;
}
