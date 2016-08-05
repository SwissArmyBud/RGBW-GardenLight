// 8,816/845
// 6,568/437

#include <Wire.h>
#include <SPI.h>
#include "RF24.h"
#include "printf.h"
#include "ByteBreaker.h"
#include "PayloadHandler.h"

#define  MIN_RGB_VALUE  0   // no smaller than 0.
#define  MAX_RGB_VALUE  150  // no bigger than 255.
#define  MAX_WHT_VALUE 200
#define  MAX_GLOBAL_LED 255
#define  TRANSITION_DELAY  20   // in milliseconds, between individual light changes

#define grnLEDport 3
#define redLEDport 5
#define bluLEDport 6
#define whtLEDport 9

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 14 & 10
RF24 radio(14, 10);
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0A0F0A5AALL, 0xF0A0F0A566LL }; // radio pipe addresses for the 2 nodes to communicate.
// Payload container
unsigned long radio_payload = 0;
// Network Address
byte STEM_ADDRESS = (byte)15;

ByteBreaker btBreaker;
PayloadHandler plHandler;

byte rgbAry[] = { 0, 0, 0};
byte curColor[] = { 0, 0, 0, 0};

bool isColor = false;

void setup(void)
{
  pinMode (redLEDport, OUTPUT);
  analogWrite(redLEDport, 0);
  pinMode (grnLEDport, OUTPUT);
  analogWrite(grnLEDport, 0);
  pinMode (bluLEDport, OUTPUT);
  analogWrite(bluLEDport, 0);
  pinMode (whtLEDport, OUTPUT);
  analogWrite(whtLEDport, 0);
  
  radio.begin();
  radio.setChannel(100);
  radio.setRetries(10,10);                 // Smallest time between retries, max no. of retries
  radio.setPayloadSize(8);                // Here we are sending 1-byte payloads to test the call-response speed
  
  if (true) { // Set the pipe direction for listening/writing
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
  } else {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1, pipes[0]);
  }

  radio.startListening(); // Start the NRF radio and have it listen for traffic

}

unsigned long currentTime = 0;
unsigned long zeroTime = 0;

void loop(void) {

  delay(30);
  currentTime = millis();
  
  if ( radio.available() ) {
    radio.read( &radio_payload, 4 );
    processPayload();
  }

}

void processPayload() {

  plHandler.unlPL = radio_payload;
  if (plHandler.bytPL.bytD == STEM_ADDRESS || plHandler.bytPL.bytD == (byte)255) {
    if(plHandler.nybPL.nybA1 == 1){
      switch(plHandler.nybPL.nybA2){
        case(1):
          rgbAry[0] = plHandler.bytPL.bytB;
          rgbAry[1] = plHandler.bytPL.bytC;
          isColor = true;
          break;
         case(2):
          rgbAry[2] = plHandler.bytPL.bytB;
          if( isColor && (plHandler.bytPL.bytC == (20))){
            updateStem();
            isColor = false;
          }
          break;
        case(3):
          rgbAry[0] = plHandler.bytPL.bytB;
          rgbAry[1] = 0;
          rgbAry[2] = 0;
          if (plHandler.bytPL.bytC == 20){
            isColor = false;
            updateStem();
          }
          break;
      }
    }
    else{
      rgbAry[0] = 0;
      rgbAry[1] = 0;
      rgbAry[2] = 0;
      isColor = true;
      updateStem();
    }
  }
  
  radio_payload = 0;
}


void updateStem() {
//  
//    byte colValue[] = {curColor[0],curColor[1],curColor[2],curColor[3]};
//    
//    // check max global LED power
//    short sum = rgbAry[0] + rgbAry[1] + rgbAry[2];
//    if((sum>MAX_GLOBAL_LED )&& color){
//      for(byte i=0;i<=3;i++){
//        rgbAry[i] = rgbAry[i] * (MAX_GLOBAL_LED / sum);
//      }
//    }
//
//    // check individual bulb power
//    byte max = 0;
//    for(byte i=0;i<3;i++){
//      if(rgbAry[i]>max){
//        max = rgbAry[i];
//      }
//    }
//    if((max>MAX_WHT_VALUE)&&!color){
//      rgbAry[0] = MAX_WHT_VALUE;
//    }
//    if((max>MAX_RGB_VALUE)&&color){
//      rgbAry[0] = rgbAry[0] * (MAX_RGB_VALUE / max);
//      rgbAry[1] = rgbAry[1] * (MAX_RGB_VALUE / max);
//      rgbAry[2] = rgbAry[2] * (MAX_RGB_VALUE / max);
//    }
//
//    byte fraction = 100;
//    for(byte i=0; i<=fraction; i++){
//      
//      if(color){
//        colValue[0] = ((100-i)*(curColor[0]/100)) + ((rgbAry[0]*i)/100);
//        colValue[1] = ((100-i)*(curColor[1]/100)) + ((rgbAry[1]*i)/100);
//        colValue[2] = ((100-i)*(curColor[1]/100)) + ((rgbAry[2]*i)/100);
//        colValue[3] = ((100-i)*(curColor[3]/100));
//      }
//      else{
//        colValue[0] = ((100-i)*(curColor[0]/100));
//        colValue[1] = ((100-i)*(curColor[1]/100));
//        colValue[2] = ((100-i)*(curColor[1]/100));
//        colValue[3] = ((100-i)*(curColor[3]/100)) + ((rgbAry[0]*i)/100);
//      }
//
//      short sum = colValue[0] + colValue[1] + colValue[2] + colValue[3];
//      if(sum>MAX_GLOBAL_LED){
//        for(byte i=0;i<=3;i++){
//          colValue[i] = colValue[i] * (MAX_GLOBAL_LED / sum);
//        }
//      }
//      analogWrite(redLEDport, colValue[0]);
//      analogWrite(grnLEDport, colValue[1]);
//      analogWrite(bluLEDport, colValue[2]);
//      analogWrite(whtLEDport, colValue[3]);
//    }

    if(isColor){
      curColor[0] = rgbAry[0];
      curColor[1] = rgbAry[1];
      curColor[2] = rgbAry[2];
      curColor[3] = 0;
    }
    else{
      curColor[0] = 0;
      curColor[1] = 0;
      curColor[2] = 0;
      curColor[3] = rgbAry[0];
    }
  
    analogWrite(redLEDport, curColor[0]);
    analogWrite(grnLEDport, curColor[1]);
    analogWrite(bluLEDport, curColor[2]);
    analogWrite(whtLEDport, curColor[3]);
}

