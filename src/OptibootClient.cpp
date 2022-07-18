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


#include "OptibootClient.h"
//#include "LoraCrypt.h"
#include "mbedtls/md.h"

static HardwareSerial &SerTar = Serial1;
static HardwareSerial &SerCon = Serial;
static int avr_rst_p = -1;

// taking this out to put the buffer in the target sketch
// that way it can be statically initialized with a firmware image if desired
// uint8_t avr64[AVR_FLASH_SZ];
static uint8_t reset_pin;

void obc_init(HardwareSerial &uart_tar, HardwareSerial &uart_con, int reset_pin,
              bool erase) {
  SerTar = uart_tar;
  SerCon = uart_con;
  avr_rst_p = reset_pin;
  if (erase) {
    for (uint32_t i = 0; i < AVR_FLASH_SZ; i++) {
      avr64[i] = 0xFF;
    }
  }
}

uint8_t bgetch(uint16_t to) {
  uint32_t sm = millis();
  while ((!SerTar.available()) && ((millis() - sm < to)))
    ;
  return (SerTar.read());
}

void bputch(uint8_t ch) { SerTar.write(ch); }

void bflush() {
  SerTar.flush();
  while (SerTar.available())
    SerTar.read();
}

void bspace() {
  bputch(CRC_EOP);
  uint8_t sync;
  uint32_t sm = millis();
  do {
    sync = bgetch(250);
  } while (sync != STK_INSYNC && millis() - sm < 1000);
}

// pre: in bootloader
uint32_t get_sig() {
  bflush();
  bputch(STK_READ_SIGN);
  bspace();
  uint8_t s1 = bgetch(250);
  uint8_t s2 = bgetch(250);
  uint8_t s3 = bgetch(250);
  if (STK_OK == bgetch())
    ;
  uint32_t ret;
  ret = s1;
  ret <<= 8;
  ret |= s2;
  ret <<= 8;
  ret |= s3;
  return (ret);
}

void load_address(uint16_t addr) {
  bflush();
  bputch(STK_LOAD_ADDRESS);
  bputch(addr & 0xFF);
  bputch(addr >> 8);
  bspace();
  if (STK_OK == bgetch())
    ;
  //  SerCon.print("Address Loaded: ");
  //  SerCon.println(addr,16);
}

void read_page(uint8_t *out) {
  bflush();
  // SerCon.print("read_page...");
  bputch(STK_READ_PAGE);
  uint16_t page_len = 512;
  bputch(page_len >> 8);
  bputch(page_len & 0xFF);
  bputch('F');
  bspace();
  for (int i = 0; i < 512; i++) {
    out[i] = bgetch(250);
  }
  if (STK_OK == bgetch())
    ;
  // SerCon.println("[ok]");
}

void write_page(uint8_t *in, uint16_t len) {
  bflush();
  // SerCon.print("read_page...");
  bputch(STK_PROG_PAGE);
  bputch(len >> 8);
  bputch(len & 0xFF);
  bputch('F');
  for (int i = 0; i < len; i++) {
    bputch(in[i]);
  }
  bspace();
  if (STK_OK == bgetch())
    ;
  // SerCon.println("[ok]");
}

void ph(uint8_t v) {
  if (v < 16)
    SerCon.print('0');
  SerCon.print(v, 16);
}

uint8_t getch() {
  while (!SerCon.available())
    ;
  return (SerCon.read());
}

uint8_t hdn(uint8_t n) {
  if (n >= '0' && n <= '9')
    return n - '0';
  if (n >= 'a' && n <= 'f')
    return (n - 'a') + 0xA;
  if (n >= 'A' && n <= 'F')
    return (n - 'A') + 0xA;
  return 0xF;
}

uint8_t hd(uint8_t h, uint8_t l) { return hdn(h) << 4 | hdn(l); }

char hen(uint8_t n) {
  if (n <= 9)
    return n + '0';
  return ((n - 10) + 'A');
}

// out must be at least 2*len+1 bytes.
// writes the hex encoded string of in to out
void hes(char *out, const uint8_t *in, uint8_t len) {
  for (int i = 0; i < len; i++) {
    *(out++) = hen(in[i] >> 4);
    *(out++) = hen(in[i] & 0xF);
  }
  *(out++) = '\0';
}

uint8_t gethex() {
  uint8_t h = getch();
  uint8_t l = getch();
  return (hd(h, l));
}

void load_hexfile() {
  SerCon.println("input hex:");
  uint8_t line[16];
  uint8_t sof;
  while ((sof = getch()) == ':') {
    uint8_t cs = 0;
    uint8_t len = gethex();
    if (len > 16) {
      SerCon.print("MAX LINE LEN 16");
      while (SerCon.available())
        SerCon.read();
      SerCon.flush();
      return;
    }
    cs = len;
    uint16_t addr = gethex();
    cs += addr;
    addr <<= 8;
    addr |= gethex();
    if ((addr + len) > 0xFFFF) {
      SerCon.print("ERROR Write beyond max valid address 0xFFFF");
      while (SerCon.available())
        SerCon.read();
      SerCon.flush();
      return;
    }
    cs += (addr & 0xFF);
    uint8_t type = gethex();
    cs += type;
    for (int i = 0; i < len; i++) {
      line[i] = gethex();
      cs += line[i];
    }
    cs = ~cs;
    cs++;
    uint8_t sum = gethex();
    if (sum != cs) {
      SerCon.print("CHECKSUM FAIL, got:");
      SerCon.print(sum, 16);
      SerCon.print(" expected ");
      SerCon.println(cs, 16);
      while (SerCon.available())
        SerCon.read();
      SerCon.flush();
      return;
    }
    if (type == 0) {
      for (int i = 0; i < len; i++) {
        avr64[addr + i] = line[i];
      }
    }
    uint8_t term;
    term = getch();
    if (term == '\r')
      term = getch();
  }
  SerCon.println(sof, 16);
}

void avr_rst() {
  SerCon.print("Resetting...");
  pinMode(avr_rst_p, INPUT_PULLUP);
  digitalWrite(avr_rst_p, LOW);
  pinMode(avr_rst_p, OUTPUT);
  digitalWrite(avr_rst_p, LOW);
  delay(10);
  pinMode(avr_rst_p, INPUT_PULLUP);
  delay(10);
  SerCon.println("[ok]");
}

uint32_t avr_cn_get_sig() {
  avr_rst();
  bflush();
  bputch(STK_GET_PARAMETER);
  bputch(STK_SW_MAJOR);
  bspace();
  SerCon.print("Major:");
  SerCon.println((int)bgetch());
  if (STK_OK != bgetch())
    return 0xFFFFFFFF;

  bflush();
  bputch(STK_GET_PARAMETER);
  bputch(STK_SW_MINOR);
  bspace();
  SerCon.print("Minor:");
  SerCon.println(bgetch());
  if (STK_OK != bgetch())
    return 0xFFFFFFFF;

  SerCon.print("Signature:");
  uint32_t sig = get_sig();
  SerCon.println(sig, 16);
  return sig;
}

uint8_t avr_read_flash() {
  uint32_t sig = avr_cn_get_sig();
  uint8_t pr = 0;
  if (sig == TARGET_SIGNATURE) {
    SerCon.println("Got AVR64DB32");
    SerCon.print("Reading Flash");
    load_address(0);
    uint8_t page[512];
    uint32_t ms_st = millis();
    for (int i = 0; i < 128; i++) {
      load_address(i * 512);
      read_page(page);
      for (int j = 0; j < 512; j++) {
        avr64[i * 512 + j] = page[j];
        if (pr) {
          if (!(j & 0x1F)) {
            SerCon.println();
            SerCon.print(i * 512 + j, 16);
            SerCon.print(":");
          }
          if (!(j & 0x3)) {
            SerCon.print(" ");
          }
          if (page[j] < 16)
            SerCon.print('0');
          SerCon.print(page[j], 16);
        }
      }
      if (pr)
        SerCon.println("");
      else if (!(i & 0xF)) {
        SerCon.print(".");
      }
    }
    SerCon.println("[ok]");
    // SerCon.print("Flash read in ");
    // SerCon.println(millis()-ms_st);
    ms_st = millis();
    print_sha256_avr();
    SerCon.println("");
  }
  return 0;
}

// bring this in from loracrypt to allow this to be used independantly
static void sha256(uint8_t *in, uint32_t len, uint8_t *out) {
  mbedtls_md_context_t ctx;
  // mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)in, len);
  mbedtls_md_finish(&ctx, out);
  mbedtls_md_free(&ctx);
}

void print_sha256_avr() {
  uint8_t sha2[32];
  sha256(avr64, AVR_FLASH_SZ, sha2);
  SerCon.print("SHA256:");
  for (int i = 0; i < 32; i++) {
    if (sha2[i] < 16)
      SerCon.print('0');
    SerCon.print(sha2[i], 16);
  }
  SerCon.println("");
}

uint8_t avr_prog_flash() {
  uint32_t sig = avr_cn_get_sig();
  uint8_t pr = 0;
  uint8_t verify_err = PROG_ERR_NO_RESET;
  if (sig == TARGET_SIGNATURE) {
    uint8_t retries = 5;
    do {
      verify_err = 0;
      SerCon.println("Got AVR64DB32");
      SerCon.print("Programming Flash");
      uint8_t page[512];
      uint32_t ms_st = millis();
      for (int i = 1; i < 128; i++) { // dont try to replace optiboot
        load_address(i * 512);
        for (int j = 0; j < 512; j++) {
          page[j] = avr64[i * 512 + j];
        }
        write_page(page, 512);
        if (!(i & 0xF)) {
          SerCon.print(".");
        }
      }
      SerCon.println("");
      SerCon.print("Verifying");
      for (int i = 1; i < 128; i++) {
        load_address(i * 512);
        read_page(page);
        for (int j = 0; j < 512; j++) {
          if (page[j] != avr64[i * 512 + j]) {
            verify_err = PROG_VERIFY_FAIL;
            SerCon.print("ERROR at");
            SerCon.println(i * 512 + j, 16);
          }
        }
        if (!(i & 0xF)) {
          SerCon.print(".");
        }
      }
      if (verify_err && retries) {
        uint8_t rst_retry = 5;
        do {
          sig = avr_cn_get_sig();
        } while (sig != TARGET_SIGNATURE && rst_retry--);
      }
      SerCon.println(verify_err ? "[FAIL]" : "[ok]");
    } while (retries-- && verify_err && sig == TARGET_SIGNATURE);
  } else {
    Serial.print("CN fail, flash unmodified");
    return PROG_ERR_NO_RESET;
  }
  return verify_err;
}

void hexprint_avr_buf() {
  uint8_t cs;
  for (int i = 0; i < 128; i++) {
    uint8_t skip = 1;
    for (int j = 0; j < 512; j++) {
      if (!(j & 0xF)) { // check line for all 0xFF
        skip = 1;
        for (int k = 0; k < 16; k++) {
          if (avr64[i * 512 + j + k] != 0xFF)
            skip = 0;
        }
        if (!skip) {
          SerCon.println("");
          SerCon.print(":10");
          uint16_t addr = i * 512 + j;
          cs = 16 + (addr >> 8) + (addr & 0xFF);
          ph(addr >> 8);
          ph(addr & 0xFF);
          SerCon.print("00");
        }
      }
      if (!skip) {
        ph(avr64[i * 512 + j]);
        cs += avr64[i * 512 + j];
        if (0xF == (j & 0xF)) {
          cs = ~cs;
          cs++;
          ph(cs & 0xFF);
        }
      }
    }
  }
  SerCon.println("");
  print_sha256_avr();
}
