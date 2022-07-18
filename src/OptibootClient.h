/*

 MIT License

 Copyright (c) 2022 ObviousInRetrospect

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 */

/* WARNING: publishing this by request. It is sparsely commented and has been
 * tested only running on an ESP32 to program an AVR64DB32. Future versions may
 * change the interface. Recommend making your own copy if you want to use it.
 * No promises of future maintainence or support are implied by making this
 * available. */

#ifndef __OptibootClient_H__
#define __OptibootClient_H__
#include "HardwareSerial.h"

// flash size in bytes. 0x10000 is an avr64db32
#define AVR_FLASH_SZ 0x10000
#define AVR_FLASH_PGSZ 512
#define AVR_FLASH_PAGES 128
#define TARGET_SIGNATURE 0x1E9618

// stick "uint8_t avr64[AVR_FLASH_SZ];" in your code
// or uncomment it in OptibootClient.cpp
extern uint8_t avr64[];

#define STK_SET_PARAMETER 0x40 // '@'
#define STK_GET_PARAMETER 0x41 // 'A'

#define STK_SW_MAJOR 0x81 // ' '
#define STK_SW_MINOR 0x82 // ' '

#define STK_READ_SIGN 0x75    // 'u'
#define STK_READ_PAGE 0x74    // 't'
#define STK_PROG_PAGE 0x64    // 'd'
#define STK_LOAD_ADDRESS 0x55 // 'U'

#define CRC_EOP 0x20 // 'SPACE'
#define STK_OK 0x10
#define STK_INSYNC 0x14 // ' '

#define PROG_OK 0
#define PROG_ERR_NO_RESET 1
#define PROG_VERIFY_FAIL 2

// initialize library to program a device connected to uart_tar
// information will be printed to uart_con
// reset_pin is used to control reset on the device
// erase will cause obc_init to write 0xff to all of the avr64 buffer
void obc_init(HardwareSerial &uart_tar, HardwareSerial &uart_con, int reset_pin,
              bool erase = true);

// get a byte from uart_tar with timeout
uint8_t bgetch(uint16_t to = 1000);

// put a character to uart_tar
void bputch(uint8_t ch);

// flush the buffer for uart_tar
void bflush();

// send CRC_EOP and wait for STK_INSYNC
void bspace();

// get the target signature
// must be in the bootloader
uint32_t get_sig();

// sends a load_address to the bootloader
void load_address(uint16_t addr);

// reads a page. out must point to a buffer that can hold AVR_FLASH_PGSZ bytes
void read_page(uint8_t *out);

// writes len bytes from in to the current address.
void write_page(uint8_t *in, uint16_t len);

// prints v to uart_con in hex
void ph(uint8_t v);

// reads a character from uart_con
uint8_t getch();

// hex decode nibble
// input: 0-9A-Fa-f
// output: hex value (0-15)
uint8_t hdn(uint8_t n);

// hex decode high and low nibble
uint8_t hd(uint8_t h, uint8_t l);

// hex encode nibble
// input (0-15)
// output [0-9A-F]
char hen(uint8_t n);

// hex encode string
// out must be at least 2*len+1 bytes.
// writes the hex encoded string of in to out
// len specifies the length of in
void hes(char *out, const uint8_t *in, uint8_t len);

// read a hex encoded byte from ser_con
uint8_t gethex();

// prompt for and parse intel hex formatted input into avr64
void load_hexfile();

// reset the target (to activate the bootloader)
void avr_rst();

// connect and get the signature of the attached target
uint32_t avr_cn_get_sig();

// all-in-one, resets the target and reads out its contents into avr64
uint8_t avr_read_flash();

// prints the hash of the buffer
void print_sha256_avr();

// all-in-one, resets the target and reads out its contents into avr64
uint8_t avr_prog_flash();

// prints the avr64 buffer in intel hex format
void hexprint_avr_buf();

#endif
