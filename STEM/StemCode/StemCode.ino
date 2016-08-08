
/*-----( Import needed libraries )-----*/
#include <Wire.h>
#include <SPI.h>
#include "RF24.h"
#include "printf.h"
#include "ByteBreaker.h"
#include "PayloadHandler.h"


/*-----( Declare Constants )-----*/

// Setup the H/P LED PWM ports
#define grnLEDport 3
#define redLEDport 5
#define bluLEDport 6
#define whtLEDport 9

/*-----( Declare Global Objects )-----*/

// Set up nRF24L01 radio on SPI bus plus pins 14 & 10
RF24 radio(14, 10);
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0A0F0A5AALL, 0xF0A0F0A566LL }; // radio pipe addresses for the 2 nodes to communicate.
// Payload container
unsigned long radio_payload = 0;

// Byte manipulators
ByteBreaker btBreaker;
PayloadHandler plHandler;

// Incoming RGB/W container and a second one to hold the current RGBW color
byte rgbAry[] = { 0, 0, 0};
byte curColor[] = { 0, 0, 0, 0};

// Boolean to keep track of the type of incoming bytes (RGB/W)
bool isColor = false;

/*----( SETUP: RUNS ONCE )----*/
void setup(void)
{

  // ------- Setup pin modes for H/P LEDs  -------------
  pinMode (redLEDport, OUTPUT);
  analogWrite(redLEDport, 0);
  pinMode (grnLEDport, OUTPUT);
  analogWrite(grnLEDport, 0);
  pinMode (bluLEDport, OUTPUT);
  analogWrite(bluLEDport, 0);
  pinMode (whtLEDport, OUTPUT);
  analogWrite(whtLEDport, 0);

  // ------- Initialize and setup the NRF radio module -------------
  radio.begin();
  radio.setChannel(100);
  radio.setRetries(10,10);
  radio.setPayloadSize(8);
  
  if (true) { // Set the pipe direction for listening/writing
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
  } else {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1, pipes[0]);
  }

  radio.startListening(); // Start the NRF radio and have it listen for traffic

}/*--( end setup )---*/

void loop() { /*----( LOOP: RUNS CONSTANTLY )----*/

  delay(20); // don't abuse the radio, give it a little break between loops
  
  if ( radio.available() ) { // if the radio has a payload available
    radio.read( &radio_payload, sizeof(radio_payload) ); // read the payload
    processPayload(); // process the payload according to protocol
  }

}/* --(end main loop )-- */

// Sub-routine to process the incoming payloads
void processPayload() {

  plHandler.unlPL = radio_payload; // copy the payload into the byte manipulator for payloads
  if (plHandler.bytPL.bytD == (byte)255) { // check for address match (NOT IMPLEMENTED)
    if(plHandler.nybPL.nybA1 == 1){  // protocol
      switch(plHandler.nybPL.nybA2){
        case(1):
          rgbAry[0] = plHandler.bytPL.bytB; // load byte 1 into the R position
          rgbAry[1] = plHandler.bytPL.bytC; // load byte 2 into the G position
          isColor = true; // activate the color flag
          break;
         case(2):
          rgbAry[2] = plHandler.bytPL.bytB; // load byte 3 into the B position
          if( isColor && (plHandler.bytPL.bytC == (20))){ // protocol
            updateStem(); // update the stem color
            isColor = false; // reset the color flag
          }
          break;
        case(3):
          rgbAry[0] = plHandler.bytPL.bytB; // load byte 1 into the W position
          rgbAry[1] = 0; // leave others empty
          rgbAry[2] = 0; // leave others empty
          if (plHandler.bytPL.bytC == 20){ // protocol
            isColor = false; // deactivate color flag
            updateStem(); // update stem color
          }
          break;
      }
    }
    else{ // if there is a problem with the protocol
      rgbAry[0] = 0; // reset
      rgbAry[1] = 0; // all
      rgbAry[2] = 0; // values
      isColor = true; // including the color flag
      updateStem(); // update the stem color
    }
  }
  
  radio_payload = 0; // reset the radio payload
}

// Sub-routine to update the stem color
void updateStem() {

    if(isColor){ // if the node is color use the RGB array to fill in the RGB values
      curColor[0] = rgbAry[0];
      curColor[1] = rgbAry[1];
      curColor[2] = rgbAry[2];
      curColor[3] = 0;
    }
    else{ // if the node is white, use the R slot to fill in W and set other colors to 0
      curColor[0] = 0;
      curColor[1] = 0;
      curColor[2] = 0;
      curColor[3] = rgbAry[0];
    }

    // Set the PWM values for the new color
    analogWrite(redLEDport, curColor[0]);
    analogWrite(grnLEDport, curColor[1]);
    analogWrite(bluLEDport, curColor[2]);
    analogWrite(whtLEDport, curColor[3]);
}

