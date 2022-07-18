#include "OptibootClient.h"

uint8_t avr64[AVR_FLASH_SZ];

void setup() {
  // put your setup code here, to run once:
  obc_init(Serial1,Serial,33,true);  
}

void loop() {
   if(Serial.available()){
    byte choice = Serial.read();
    if (choice == 'r')
    {
      avr_read_flash();
    }
    else if (choice == 'p')
    {
      avr_prog_flash();
    }
    else if(choice == 'd'){
      hexprint_avr_buf();
    }
    else if(choice=='l'){
      load_hexfile();
      print_sha256_avr();
    }
    else if(choice == 'e'){
      Serial.println("erasing buffer");
      for(uint32_t i=0; i<AVR_FLASH_SZ; i++){
        avr64[i]=0xFF;
      }
    }
    else if(choice == 'c'){
      Serial.println("corrupting buffer");
      for(uint32_t i=0; i<AVR_FLASH_SZ; i++){
        avr64[i]=i&0xFF;
      }
    }
    else{
      Serial.println("OptibootClientExample" __TIMESTAMP__);
      Serial.println("r)ead avr64 flash into buffer");
      Serial.println("l)oad hex into buffer");
      Serial.println("d)ump buffer as intel hex");
      Serial.println("e)rase buffer");
      Serial.println("p)rogram avr64");
    }
   }
}
