#include <Wire.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "ByteBreaker.h"
#include "PayloadHandler.h"
#include "LightNode.h"

#define  MIN_RGB_VALUE  0   // no smaller than 0.
#define  MAX_RGB_VALUE  100  // no bigger than 255.
#define  MAX_WHT_VALUE 200
#define  MAX_GLOBAL_LED 500
#define  MAX_INC_COLOR 63.0
#define  MAX_INC_WHITE 255.0
#define  TRANSITION_DELAY  20   // in milliseconds, between individual light changes

#define EXTENDED_PAYLOADS 0
#define NODE_OPERATIONS 1
#define CHAIN_RESET 0
#define TIMING_INFO 1
#define ADD_WHITE 2
#define ADD_COLOR 3
#define SYSTEM_PAYLOADS 1
#define NETWORKING_OPERATIONS 0
#define GLOBAL_OPERATIONS 1

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
const uint64_t pipes[2] = { 0xE8E8F0F0E1LL, 0xF0F0F0F0D2LL };
// Payload container
unsigned long radio_payload = 0;
// Network Address
byte STEM_ADDRESS = (byte)15;

//
// Light Chain
//
LightNode nodeAry[32];
byte numNodes = 0;
byte nodeIndex = 0;
unsigned long lastTransition = 0;
LightNode* currentNode = NULL;
LightNode* nextNode = NULL;

ByteBreaker btBreaker;
PayloadHandler plHandler;
//
// Topology
//

unsigned long timeAry[] = { 0, 0, 0, 0};
byte rgbAry[] = { 0, 0, 0, 0};
//byte curRGB[]={ 0, 0, 0, 0};
//byte tmpRGB[]={ 0, 0, 0, 0};

void setup(void)
{
  for (int i = 0; i < 16; i++) {
    nodeAry[i] = LightNode(0, 0, 0, 0, 0, 0, 0, 0);
  }
  pinMode (redLEDport, OUTPUT);
  analogWrite(redLEDport, 0);
  pinMode (grnLEDport, OUTPUT);
  analogWrite(grnLEDport, 0);
  pinMode (bluLEDport, OUTPUT);
  analogWrite(bluLEDport, 0);
  pinMode (whtLEDport, OUTPUT);
  analogWrite(whtLEDport, 0);
  // Setup and configure rf radio
  //

  radio.begin();
  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15, 15);
  // optionally, reduce the payload size.  seems to
  // improve reliability
  radio.setPayloadSize(8);

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  if (true)
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
  }
  else
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1, pipes[0]);
  }

  radio.startListening();

}

unsigned long currentTime = 0;
unsigned long zeroTime = 0;

void loop(void) {

  delay(50);

  if (zeroTime == 0) {
    zeroTime = millis();
  }
  currentTime = millis() - zeroTime;

  if ( radio.available() ) {
    // Dump the payloads until we've gotten everything
    bool done = false;
    radio_payload = 0;
    while (!done) {
      // Fetch the payload, and see if this was the last one.
      done = radio.read( &radio_payload, sizeof(unsigned long) );
    }
    processPayload();
  }

  updateStem();

}



void processPayload() {

  plHandler.unlPL = radio_payload;

  if (plHandler.bytPL.bytD == STEM_ADDRESS || plHandler.bytPL.bytD == (byte)255) {

    btBreaker.nybBB.nybA = plHandler.nybPL.nybA1;
    switch (btBreaker.duoBB.duoA1) {
      case EXTENDED_PAYLOADS:
        //TODO for EXTENDED PAYLOADS
        switch (btBreaker.duoBB.duoA2) {
          case (byte)0:
            //TODO for RESET NODE CHAIN
            break;
          case NODE_OPERATIONS:
            //TODO for NODE OPERATIONS
            processNodeOp();
            break;
          case (byte)2:
            //TODO for RESERVED PAYLOAD TYPE
            break;
          case (byte)3:
            //TODO for RESERVED PAYLOAD TYPE
            break;
        }
        break;
      case SYSTEM_PAYLOADS:
        //TODO for SYSTEM PAYLOADS
        switch (btBreaker.duoBB.duoA2) {
          case NETWORKING_OPERATIONS:
            //TODO for NODE OPERATIONS
            //processNetworkOp();
            break;
          case GLOBAL_OPERATIONS:
            //TODO for NODE OPERATIONS
            //processPowerTimingOp();
            break;
          case (byte)2:
            //TODO for RESERVED PAYLOAD TYPE
            break;
          case (byte)3:
            //TODO for RESERVED PAYLOAD TYPE
            break;
        }
        break;
      case (byte)2:
        //TODO for RESERVED PAYLOAD TYPE
        break;
      case (byte)3:
        //TODO for RESERVED PAYLOAD TYPE
        break;
    }
  }
  radio_payload = 0;
}

void processNodeOp() {
  plHandler.unlPL = radio_payload;
  btBreaker.nybBB.nybA = plHandler.nybPL.nybA2;
  switch (btBreaker.duoBB.duoA1) {
    case CHAIN_RESET:
      numNodes = 0;
      nodeIndex = 0;
      break;
    case TIMING_INFO:
      timeAry[btBreaker.duoBB.duoA2] = plHandler.unsPL.unsS;
      if (btBreaker.duoBB.duoA2 == 3) {
        // LightNode(                 _redNew,   _grnNew,   _bluNew,   _whtNew,   holdTime,   _timeA,     _timeB,     _transition)
        nodeAry[numNodes] = LightNode(rgbAry[0], rgbAry[1], rgbAry[2], rgbAry[3], timeAry[0], timeAry[1], timeAry[2], timeAry[3]);
        numNodes++;
      }
      break;
    case ADD_WHITE:
      break;
    case ADD_COLOR:
      btBreaker.duoBB.duoA1 = 0;
      btBreaker.nybBB.nybB = plHandler.nybPL.nybB2;
      rgbAry[0] = (btBreaker.bytBB / MAX_INC_COLOR) * MAX_RGB_VALUE;
      btBreaker.nybBB.nybB = plHandler.nybPL.nybC1;
      btBreaker.nybBB.nybA = plHandler.nybPL.nybB1;
      btBreaker.duoBB.duoA2 = btBreaker.duoBB.duoA1;
      btBreaker.duoBB.duoA1 = (byte)0;
      rgbAry[1] = (btBreaker.bytBB / MAX_INC_COLOR) * MAX_RGB_VALUE;
      btBreaker.nybBB.nybA = plHandler.nybPL.nybB1;
      btBreaker.duoBB.duoA1 = (byte)0;
      btBreaker.nybBB.nybB = plHandler.nybPL.nybC2;
      rgbAry[2] = (btBreaker.bytBB / MAX_INC_COLOR) * MAX_RGB_VALUE;
      break;
  }
}

void toggleLED(int led_pin, int toggle_times) {
  for (int i = 0; i < toggle_times; i++) {
    if (digitalRead(led_pin)) {
      digitalWrite(led_pin, LOW);
      delay(100);
      digitalWrite(led_pin, HIGH);
      delay(100);
    }
    else {
      digitalWrite(led_pin, HIGH);
      delay(100);
      digitalWrite(led_pin, LOW);
      delay(100);
    }
  }
}

void updateStem() {
  if (numNodes == 0) {
    if ((currentTime / 1000) % 2) {
      analogWrite(redLEDport, 100);
    }
    else {
      analogWrite(redLEDport, 0);
    }
    nodeIndex = 0;
    lastTransition = currentTime;
  }
  else {

    byte redValue = 0;
    byte grnValue = 0;
    byte bluValue = 0;
    byte whtValue = 0;

    if (currentTime > (   lastTransition + (*currentNode).getHoldTime() + (*currentNode).getTransA() + (*currentNode).getTransB()   )) {

      lastTransition = currentTime;
      nodeIndex++;
      
      if (nodeIndex == numNodes) {
        nodeIndex = 0;
      }
      
      *currentNode = nodeAry[nodeIndex];

      if (numNodes == 1) {
        nextNode = currentNode;
      }
      else if ((nodeIndex + 1) == numNodes) {
        *nextNode = nodeAry[0];
      }
      else {
        *nextNode = nodeAry[nodeIndex + 1];
      }

    }

    if ( currentTime < (  lastTransition + (*currentNode).getHoldTime() ) ) {
      redValue = (*currentNode).getRed();
      grnValue = (*currentNode).getGrn();
      bluValue = (*currentNode).getBlu();
      whtValue = (*currentNode).getWht();
    }
    
    else if ( currentTime < (  lastTransition + (*currentNode).getHoldTime() + (*currentNode).getTransA() ) ) {
      float fraction = ( currentTime - ( lastTransition  + (*currentNode).getHoldTime() ) ) / (*currentNode).getTransA();
      switch ((*currentNode).getTransType()) {
        case 0:
          redValue = (*currentNode).getRed();
          grnValue = (*currentNode).getGrn();
          bluValue = (*currentNode).getBlu();
          whtValue = (*currentNode).getWht();
          break;
        case 1:
          redValue = ( (*currentNode).getRed()/(1+fraction) )+( (*nextNode).getRed()*(fraction/2) );
          grnValue = ( (*currentNode).getGrn()/(1+fraction) )+( (*nextNode).getGrn()*(fraction/2) );
          bluValue = ( (*currentNode).getBlu()/(1+fraction) )+( (*nextNode).getBlu()*(fraction/2) );
          whtValue = ( (*currentNode).getWht()/(1+fraction) )+( (*nextNode).getWht()*(fraction/2) );
          break;
        case 2:
          redValue = ( (*currentNode).getRed() * (1-fraction) );
          grnValue = ( (*currentNode).getGrn() * (1-fraction) );
          bluValue = ( (*currentNode).getBlu() * (1-fraction) );
          whtValue = ( 0.66 * MAX_WHT_VALUE * (fraction) );
          break;
        case 3:
          redValue = ( (*currentNode).getRed() * (1-fraction) );
          grnValue = ( (*currentNode).getGrn() * (1-fraction) );
          bluValue = ( (*currentNode).getBlu() * (1-fraction) );
          whtValue = ( (*currentNode).getWht() * (1-fraction) );
          break;
      }
    }
    
    else if ( currentTime < (  lastTransition + (*currentNode).getHoldTime() + (*currentNode).getTransA() + (*currentNode).getTransB()) ) {
      float fraction = ( currentTime - ( lastTransition  + (*currentNode).getHoldTime() + (*currentNode).getTransA() ) ) / (*currentNode).getTransB();
      switch ((*currentNode).getTransType()) {
        case 0:
          redValue = (*nextNode).getRed();
          grnValue = (*nextNode).getGrn();
          bluValue = (*nextNode).getBlu();
          whtValue = (*nextNode).getWht();
          break;
        case 1:
          redValue = ( ((*currentNode).getRed()/2)*(1-fraction) )+( (*nextNode).getRed()/(2-fraction) );
          redValue = ( ((*currentNode).getGrn()/2)*(1-fraction) )+( (*nextNode).getGrn()/(2-fraction) );
          redValue = ( ((*currentNode).getBlu()/2)*(1-fraction) )+( (*nextNode).getBlu()/(2-fraction) );
          redValue = ( ((*currentNode).getWht()/2)*(1-fraction) )+( (*nextNode).getWht()/(2-fraction) );
          break;
        case 2:
          redValue = ( (*nextNode).getRed() * fraction );
          grnValue = ( (*nextNode).getGrn() * fraction );
          bluValue = ( (*nextNode).getBlu() * fraction );
          whtValue = ( 0.66 * MAX_WHT_VALUE * (1 - fraction) ) + ( ( (*nextNode).getWht() * fraction ));
          break;
        case 3:
          redValue = ( (*nextNode).getRed() * (fraction) );
          grnValue = ( (*nextNode).getGrn() * (fraction) );
          bluValue = ( (*nextNode).getBlu() * (fraction) );
          whtValue = ( (*nextNode).getWht() * (fraction) );
          break;
      }
    }
    analogWrite(redLEDport, redValue);
    analogWrite(grnLEDport, grnValue);
    analogWrite(bluLEDport, bluValue);
    analogWrite(whtLEDport, whtValue);
  }
}

