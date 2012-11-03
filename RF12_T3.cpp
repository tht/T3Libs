/// RF12_t3.cpp
// Implementation file for RFM12B library for Teensy 3.0
// 2012: tht https://github.com/tht

#include "RF12_T3.h"

// Init singleton cariable
RF12_T3* RF12_T3::_instance = 0; // init singleton


/*
 * drssi_dec_tree[] is an array used to do a binary
 * search to get the most accurate dssi value out
 * of the RFM12b module. drssi holds the actual value.
 * if drssi <=5 then we are still searching in this array
 * drssi & B1000 indicate terminated states where getDSSIdB()
 * allows to querty the correct values.
 */
struct drssi_dec_t {
    uint8_t up;
    uint8_t down;
    uint8_t threshold;
};

const drssi_dec_t drssi_dec_tree[] = {
    /*  up    down  thres*/
    /* 0 */ { B1001, B1000, B000 },
    /* 1 */ { B0010, B0000, B001 },
    /* 2 */ { B1011, B1010, B010 },
    /* 3 */ { B0101, B0001, B011 },
    /* 4 */ { B1101, B1100, B100 },
    /* 5 */ { B1110, B0100, B101 }
};


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
    digitalWriteFast(SCK, LOW);
    digitalWriteFast(MOSI, LOW);
    digitalWriteFast(SS, HIGH);
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(SS, OUTPUT); // is pin 10
    
    // enables and configures SPI module
    SIM_SCGC6 |= SIM_SCGC6_SPI0;  // enable SPI clock
    SPI0_MCR = 0x80004000;
    SPCR |= _BV(MSTR);
    SPCR |= _BV(SPE);
    SPCR &= ~(_BV(DORD));   // MSBFIRST SPI
    
    // 16MHz 16bit transfers on CTAR0
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
    digitalWriteFast(10, LOW);
    SPI0_PUSHR = (1<<26) | data;    // send data (clear transfer counter)
    while (! SPI0_TCR) ; // loop until transfer is complete
    digitalWriteFast(10, HIGH);
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
    digitalWriteFast(10, LOW);  // select RFM12b module
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
            digitalWriteFast(10, HIGH);
            
            // do drssi binary-tree search
            if ( drssi < 6 ) {       // not yet final value
                if ( bitRead(res,8) )  // rssi over threashold?
                    drssi = drssi_dec_tree[drssi].up;
                else
                    drssi = drssi_dec_tree[drssi].down;
                if ( drssi < 6 ) {     // not yet final destination
                    rf12_xfer(0x94A0 | drssi_dec_tree[drssi].threshold);
                }
            }
            
            // save data to internal buffer
            buffer[_index++] = data;
            
            if (_index==1) {          // first packet!
                SIM_SCGC6 |= SIM_SCGC6_CRC;                  // enable crc clock
                CRC_CTRL = 0x00000000 | (1 <<30) | (1 <<28); // 16bit mode with some translation
                CRC_GPOLY16 = 0x8005;                        // polynom
                bitSet(CRC_CTRL,25);                         // prepare to write seed
                CRC_CRC16 = 0xffff;                          // this is the seed
                bitClear(CRC_CTRL,25);                       // prepare to write data

                CRC_CRC8 = groupId;
                CRC_CRC8 = data;
                
            } else if (_index==2) {   // second packet (with length)
                CRC_CRC8 = data;
                
            } else {                  // data (or crc)
                CRC_CRC8 = data;
                
                afc_offset = ((state & 0x0010)?-1:1) * state&0x000F;
                
                // abort reception if we got a full packet
                if (_index > buffer[1] + 3) { // +1 for header, +2 for checksum
                    disableReceiver();
                    _recvDone = 1;
                    rf12_crc = CRC_CRC16;
                }
            }
            
            // =====================================================
            // Buffer needs neyt byte to send
        } else {
            digitalWriteFast(10, HIGH);
            
            rf12_xfer(0xB800 | _toSend);
            
            // prepare next byte to send
            if (state < 0) {
                _toSend = buffer[2 + buffer[1] + state++];
                CRC_CRC8 = _toSend;
            } else {
                switch (++state) {
                    case RF_TXSYN1: _toSend = 0x2D; break;
                    case RF_TXSYN2: _toSend = groupId;
                        CRC_CRC8 = _toSend;
                        state = - (2 + buffer[1]);
                        break;
                    case RF_TXCRC1: _toSend = 0xff & (CRC_CRC16>>8); break;
                    case RF_TXCRC2: _toSend = 0xff & (CRC_CRC16);   break;
                    case RF_TXTAIL1: _toSend = 0xAA; break; // dummy
                    case RF_TXTAIL2: break; // dummy
                    case RF_TXDONE: _toSend = 0x99; // dummy, fall through
                    default:        disableTransmitter(); state = RF_IDLE;  // make sure we're back on track
                }
            }
        }
    } else
        digitalWriteFast(10, HIGH);  // don't forget to disable CS to RFM module
    
    // =====================================================
    // Power-On reset complete, do init now
    if (res & 0x4000) {
        rf12_xfer(0x80C7 | (bandId << 4)); // configuration settings
        rf12_xfer(0x82D9);
        rfMode = 0x82D9; rfMode = 0x82D9; // rx enabled, wakeup disabled, lowbat disabled
        rf12_xfer(0xA640);
        rf12_xfer(0xC600 | datarate);
        rf12_xfer(0x94A3);
        rf12_xfer(0xC2AC);
        rf12_xfer(0xCA83);
        rf12_xfer(0xCE00 | groupId); // sync Byte (group id)
        rf12_xfer(0xC493);
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
    // init drssi detection tree
    drssi = 3;
    rf12_xfer(0x94A0 | drssi_dec_tree[drssi].threshold);
    
    // switch RFM12b state
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
    
    // preparing CRC unit
    SIM_SCGC6 |= SIM_SCGC6_CRC;                  // enable crc clock
    CRC_CTRL = 0x00000000 | (1 <<30) | (1 <<28); // 16bit mode with some translation
    CRC_GPOLY16 = 0x8005;                        // polynom
    bitSet(CRC_CTRL,25);                         // prepare to write seed
    CRC_CRC16 = 0xffff;                          // this is the seed
    bitClear(CRC_CTRL,25);                       // prepare to write data
    
    enableTransmitter();
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
