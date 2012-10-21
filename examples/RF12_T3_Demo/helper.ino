/// helper.ino
// Some helpful methods not directly related to a particular library.
// 2012: tht https://github.com/tht


/**
 * ppBin4(HardwareSerial Uart, uint8_t v)
 * Helps outputing nice formated binary numbers
 * like 0010
 */
inline void ppBin4(HardwareSerial Uart, uint8_t v) {
  char buf[5] = "0000";
  for (int i=0; i<4; i++) {
    if (v & 0x01)
      buf[3-i] = '1';
    v >>= 1;
  }
  Uart.print(buf);
}


/**
 * ppBin8(HardwareSerial Uart, uint8_t v)
 * Helps outputing nice formated binary numbers
 * like 1001.0010
 */
inline void ppBin8(HardwareSerial Uart, uint8_t v) {
  ppBin4(Uart, 0x0f & (v >> 4));
  Uart.print('.');
  ppBin4(Uart, 0x0f & (v));
}


/**
 * ppBin16(HardwareSerial Uart, uint16_t v)
 * Helps outputing nice formated binary numbers
 * like 0010.1001:1001.0010
 */
inline void ppBin16(HardwareSerial Uart, uint16_t v) {
  ppBin8(Uart, 0x00ff & (v >> 8));
  Uart.print(':');
  ppBin8(Uart, 0x00ff & (v));
}


/**
 * ppBin32(HardwareSerial Uart, uint32_t v)
 * Helps outputing nice formated binary numbers
 * like 0010.1001:1001.0010|0010.1001:1001.0010
 */
inline void ppBin32(HardwareSerial Uart, uint32_t v) {
  ppBin16(Uart, 0x0000ffff & (v >> 16));
  Uart.print('|');
  ppBin16(Uart, 0x0000ffff & (v));
}




/**
 * ppHex4(HardwareSerial Uart, uint8_t v)
 * Helps outputing nice formated hex numbers
 * like A
 */
inline void ppHex4(HardwareSerial Uart, uint8_t v) {
  Uart.print(v,HEX);
}


/**
 * ppHex8(HardwareSerial Uart, uint8_t v)
 * Helps outputing nice formated hex numbers
 * like 0A
 */
inline void ppHex8(HardwareSerial Uart, uint8_t v) {
  ppHex4(Uart, 0x0f & (v >> 4));
  ppHex4(Uart, 0x0f & (v));
}


/**
 * ppHex16(HardwareSerial Uart, uint16_t v)
 * Helps outputing nice formated hex numbers
 * like 0ACC
 */
inline void ppHex16(HardwareSerial Uart, uint16_t v) {
  ppHex8(Uart, 0x00ff & (v >> 8));
  ppHex8(Uart, 0x00ff & (v));
}


/**
 * ppHex32(HardwareSerial Uart, uint32_t v)
 * Helps outputing nice formated hex numbers
 * like 0ACC.ABBA
 */
inline void ppHex32(HardwareSerial Uart, uint32_t v) {
  ppHex16(Uart, 0x0000ffff & (v >> 16));
  Uart.print('.');
  ppHex16(Uart, 0x0000ffff & (v));
}


/**
 * ppHex64(HardwareSerial Uart, uint64_t v)
 * Helps outputing nice formated hex numbers
 * like 0ACC.ABBA:0123.4567
 */
inline void ppHex64(HardwareSerial Uart, uint64_t v) {
  ppHex32(Uart, 0x0000ffffffff & (v >> 32));
  Uart.print(':');
  ppHex32(Uart, 0x0000ffffffff & (v));
}
