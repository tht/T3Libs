/// RF12_t3.h
// Header file for RFM12B library for Teensy 3.0
// 2012: tht https://github.com/tht

/*
In an attempt to make it easier to change the irq pin;
created #define IRQPIN and used IRQPIN where the irq pin was referenced 
renamed irqLine4 to irqLineT3
renamed _handleIrq4 to _handleIrqT3
*/

#ifndef RF12_T3_h
#define RF12_T3_h

#include "Arduino.h";
#include <stdlib.h> // for malloc and free

#define IRQPIN 4

/**
 * Known requency bands.
 */
#define RF12_BAND433MHZ 1
#define RF12_BAND868MHZ 2
#define RF12_BAND915MHZ 3


/**
 * Some additional definitions to use CRC unit
 */
#define CRC_CRC8    *(volatile uint8_t  *)0x40032000 // for adding a single byte to CRC
#define CRC_CRC16   *(volatile uint16_t *)0x40032000 // for writing seed and reading result
#define CRC_GPOLY16 *(volatile uint16_t *)0x40032004 // for writing polynomial


class RF12_T3 {
    
public:
    
    // =====================================================
    // Init
    
    // returns instance for IRQPIN
    static RF12_T3* irqLineT3() {
        if (!_instance) {
            _instance = (RF12_T3*)malloc(sizeof(RF12_T3));
            _instance->setIrq(IRQPIN);
        }
        return _instance;
    }
    
    // Inits RFM12b
    int reinit(uint8_t id, uint8_t band, uint8_t group, uint8_t rate);
    int reinit(uint8_t id, uint8_t band, uint8_t group) {
        return reinit(id, band, group, 0x06); // JeeNode datarate
    }
    
    
    // =====================================================
    // Query network infos from last reinit
    
    uint8_t geNodetId(){ return nodeId; }
    uint8_t getGroup() { return groupId; }
    uint8_t getBand()  { return bandId; }
    
    
    // =====================================================
    // Receiving data
    
    // Variables containing received data (TODO: Should be private)
    volatile uint8_t  _index;
    volatile char buffer[70];  // also used for sending
    volatile uint16_t rf12_crc;    // also used for sending
    
    // check if there is a message waiting
    boolean recvDone() {
        if (state == RF_IDLE) {
            resetReceiveBuffer();
            enableReceiver();
            state=RF_RECV;
            _recvDone = 0;
        } else if (_recvDone) {
            state = RF_IDLE;
            if ( (!reportBroken && rf12_crc!=0x0000)  // report broken packets?
                | (buffer[0] & (1<<6) && (buffer[0] & 0x1F)!=nodeId) ) // not for us
                _recvDone = 0;  // ignore packet, because of invalid checksum
        }
        return _recvDone;
    }
    
    boolean getReportBroken() {
        return reportBroken;
    }
    
    // enable reporting of packages with invalid CRC
    void setReportBroken(boolean report) {
        reportBroken = report;
    }
    
    // return signal strength calculated out of DRSSI bit
    int8_t getDRSSI() {
        if (! drssi & B1000)
            return 0;
        
        const int8_t table[] = {-106, -100, -94, -88, -82, -76, -70};
        return table[drssi & B111];
    }
    
    
    int8_t getAFCOffset() {
        return afc_offset;
    }
    
    
    // =====================================================
    // Transmitting data
    
    // Check if we can send now - required before doing sendStart(*)
    boolean canSend();
    
    // Send empty packet with specific header
    void sendStart(uint8_t hdr);
    
    // Send packet with specific header and data
    void sendStart(uint8_t hdr, const void *ptr, uint8_t len);
    
    // wait until data was sent (TODO: implement!)
    void sendWait (uint8_t mode);
    
    
    // =====================================================
    // Wakeup-Timer
    
    // set wakeup-timer
    inline void rf12_sleep (unsigned long millis);
    
    // check if we got wakeup call
    inline boolean gotWakeup();
    
    
    // =====================================================
    // IRQ handling stuff (internal use only)
    void handleIrq();
    
    
private:
    
    // =====================================================
    // Internal driver state
    enum {
        RF_TXCRC1 = 1, // sending, first CRC byte follows => 1
        RF_TXCRC2, // sending, second CRC byte follows    => 2
        RF_TXTAIL1,// sending, tail follows               => 3
        RF_TXTAIL2,// sending, switchoff follows          => 4
        RF_TXDONE, // sending finished, have to switch off sender, falling to RF_IDLE => 5
        RF_TXPRE,  // sending prefix tact sync            => 6
        RF_TXSYN1, // sending, first SYNC byte            => 7
        RF_TXSYN2, // sending, secondfirst SYNC byte, buffer follows => 8
        
        RF_RECV,   // in receive state (may already have received some bytes)
        RF_IDLE    // full packet received, receiving disabled, will switch to RF_RECV on next call to recvDone()
    };
    
    volatile int8_t state;    // see enum above
    volatile uint16_t rfMode; // current mode of RFM module (0x82xx)
    uint8_t irqLine;          // where irq from RFM12b is connected to
    volatile uint8_t _toSend; // next byte to send while transmitting (RF_TX*)
    
    boolean reportBroken;     // report packets with invalid checksum to Sketch?
    
    
    // =====================================================
    // Network infos
    uint8_t nodeId;
    uint8_t groupId;
    uint8_t bandId;
    
    
    // =====================================================
    // RFM12b infos
    boolean available;  // did module respond to reset command
    boolean wakeup;     // did we receive a wakeup?
    uint8_t datarate;
    volatile boolean _recvDone; // a message is waiting in "buffer"
 
    volatile uint8_t drssi; // received signal strength (see dssi table)
    volatile int8_t afc_offset; // received signal AFC offset

    
    // =====================================================
    // Enable/Disable components / communicate to RFM12b module
    void enableReceiver();
    void disableReceiver();
    void enableTransmitter();
    void disableTransmitter();
    
    inline uint16_t rf12_xfer(uint16_t data);
    
    static void _handleIrqT3() {
        _instance->handleIrq();
    }
    
    
    // =====================================================
    // Receive/send buffer handling
    void resetReceiveBuffer() {
        _index = 0;
        buffer[0] = 0; // reset header
        buffer[1] = 0; // reset len
        rf12_crc = 0xffff;
    }
    
    
    // =====================================================
    // CRC
    void initCRC() {
        SIM_SCGC6 |= SIM_SCGC6_CRC;                  // enable crc clock
        CRC_CTRL = 0x00000000 | (1 <<30) | (1 <<28); // 16bit mode with some translation
        CRC_GPOLY16 = 0x8005;                        // polynom
        bitSet(CRC_CTRL,25);                         // prepare to write seed
        CRC_CRC16 = 0xffff;                          // this is the seed
        bitClear(CRC_CTRL,25);                       // prepare to write data
    }
    
    
    // =====================================================
    // Singleton handling
    static RF12_T3* _instance;
    RF12_T3(); // private -> singleton
    void setIrq(uint8_t irq){
        irqLine = irq;
    }
};

#endif


