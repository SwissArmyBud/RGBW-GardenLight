// 26,988/1,082
// 14,794/766
// 14,626/765
// 14,534/763
/*-----( Import needed libraries )-----*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"
#include "PayloadHandler.h"
#include "ByteBreaker.h"
#include "QuadEncoderRGB.h"
#include "BoardStatusLED.h"

/*-----( Declare Constants )-----*/
typedef struct {
  const String name;
  const byte code;
} Option ;
typedef struct {
  const String name;
  const Option * list;
  const byte size;
} Menu;

/*-----( Declare objects )-----*/

// Use I2C scanner to find address of I2C controller.
// Set I2C pins :     addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Creating the LCD object for the sketch.
String lcdLine1 = String("");
String lcdLine2 = String("");

// Byte level manipulators, see source files for details
PayloadHandler plHandler;
ByteBreaker btBreaker;

// Setting up the NRF radio on pins 14 and 10
RF24 radio(14, 10);
const uint64_t pipes[2] = { 0xF0A0F0A5AALL, 0xF0A0F0A566LL }; // radio pipe addresses for the 2 nodes to communicate.
unsigned long radio_payload = 0; // default, empty payload for the radio module
unsigned long last_payload = 0;

// Setup QuadEncoder with RGB/Switch -  RD GR BL SW eA eB sT  mE
QuadEncoderRGB encoder = QuadEncoderRGB(3, 5, 9, 7, 2, 4, 50, 10);

BoardStatusLED redStatus(15, LOW);
BoardStatusLED yelStatus(16, LOW);
BoardStatusLED grnStatus(17, LOW);

enum ControllerState {
  IN_MENU,
  CREATING_NODE
};

enum OperationType {
  BASIC_OPERATION,
  ADVANCED_OPERATION
};

OperationType currentOpType = BASIC_OPERATION;
ControllerState currentState = IN_MENU;

unsigned long blinkTimer = 0;
unsigned long displayTimer = 0;

char nextColor = 'r';
bool isColor = false;
byte rgbAry[] = {(byte)0, (byte)0, (byte)0};
byte tmpRGB[] = {(byte)0, (byte)0, (byte)0};

byte stemAddress = 0;
bool confirmSelection = false;
byte numNodes = 0;
byte curPower = 255;

const Option goHome = {"Go Back", 0};
const Option mainAry[] = {{"Basic Menu", 1}, {"Adv. Menu", 2}};
const Menu mainMenu = {"Main", mainAry, 2};
const Option bascAry[] = {{"Set White", 3}, {"Set Color", 4}, {"Set On/Off", 5}, goHome};
const Menu bascMenu = {"Basic", bascAry, 4};
const Option advAry[] = {{"Set Bright %", 5}, goHome};
const Menu advnMenu = {"Advanced", advAry, 2};

const Menu* currentMenu = &bascMenu;

bool lastTransmit = true;
/*----( SETUP: RUNS ONCE )----*/
void setup() {

  attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2

  lcd.begin(16, 2);
  // ------- Quick blinks of 16x2 LCD backlight  -------------
  for (int i = 0; i < 2; i++) {
    lcd.backlight();
    delay(250);
    lcd.noBacklight();
    delay(250);
  }
  lcd.backlight(); // finish with 16x2 LCD backlight on

  //-------- Write characters on the display ------------------


      lcdLine1 = F("PLEASE WAIT - ");
      lcdLine2 = F("Loading...");
      updateLCD();


  // ------- Initialize and setup the NRF radio module -------------
  radio.begin();
  radio.setChannel(100);
  radio.setRetries(10,10);                 // Smallest time between retries, max no. of retries
  radio.setPayloadSize(8);                // Here we are sending 1-byte payloads to test the call-response speed

  if (false) { // Set the pipe direction for listening/writing
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
  } else {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1, pipes[0]);
  }

  radio.startListening(); // Start the NRF radio and have it listen for traffic

  grnStatus.update();
  yelStatus.update();
  redStatus.update();

  encoder.setColor(100, 0, 0);
  delay(300);
  encoder.setColor(0, 100, 0);
  delay(300);
  encoder.setColor(0, 0, 100);
  delay(300);
  encoder.setColor(0, 0, 0);

  delay(500);
}/*--(end setup )---*/

void loop() { /*----( LOOP: RUNS CONSTANTLY )----*/

  unsigned long currentTime = millis();

  if (blinkTimer == 0) {

    currentMenu = &mainMenu;

    yelStatus.blink(200);
    delay(100);
    yelStatus.update();
    encoder.setEncPos(0);
  }


  //---------- Handle Feedback/Updates/Timeouts for System --------------------
  if (blinkTimer < currentTime) {
    grnStatus.blink(100);
    blinkTimer = currentTime + 3000;
    sendNode(255);
  }
  if (displayTimer < currentTime) {
    updateLCD();
    displayTimer = currentTime + 300;
  }
  
  lcdLine1 = F("");
  lcdLine2 = F("");
  encoder.update();
  grnStatus.update(); 
  yelStatus.update();
  redStatus.update();
  
  switch (currentState) {
    case IN_MENU:
      encoder.setColor(0, 0, 0);
      lcdLine1 += F("M: ");
      lcdLine1 += (*currentMenu).name;

      if ((*currentMenu).size == 0) {
        encoder.setMaxEncPos(0);
        encoder.setEncPos(0);
        lcdLine2 += F("No Options");
        if (encoder.getSwitchState()) {
          encoder.resetSwitchState();
          currentMenu = &mainMenu;
        }
      }

      else {
        encoder.setMaxEncPos((*currentMenu).size - 1);
        Option oPnt = ((*currentMenu).list)[encoder.getEncPos()];
        lcdLine1 += " " + String(encoder.getEncPos() + 1, DEC);
        lcdLine1 += F("/");
        lcdLine1 += String((*currentMenu).size, DEC);
        lcdLine2 += F("O: ");
        lcdLine2 += oPnt.name;
        if (encoder.getSwitchState()) {
          encoder.setEncPos(0);
          encoder.resetSwitchState();
          switch (oPnt.code) {
            case 0:
              currentMenu = &mainMenu;
              break;
            case 1:
              currentMenu = &bascMenu;
              currentOpType = BASIC_OPERATION;
              break;
            case 2:
              currentMenu = &advnMenu;
              currentOpType = ADVANCED_OPERATION;
              break;
            case 3:
              currentState = CREATING_NODE;
              isColor = 0;
              nextColor = 'w';
              break;
            case 4:
              currentState = CREATING_NODE;
              isColor = 1;
              nextColor = 'r';
              break;
            case 5:
              //masterSet(0);
              break;
          }
        }
      }

      break;
    case CREATING_NODE:
      switch (nextColor) {
        
        case 'w':
          encoder.setMaxEncPos(20);
          encoder.setColor(encoder.getEncPos()*3, encoder.getEncPos()*3, encoder.getEncPos()*3);
          lcdLine1 += F("Set Wht, Max ");
          lcdLine1 += String(encoder.getMaxEncPos(), DEC);
          lcdLine2 += F("Value: ");
          lcdLine2 += String(encoder.getEncPos());
          if (encoder.getSwitchState()) {
            rgbAry[0] = encoder.getEncPos()*10;
            rgbAry[1] = 0;
            rgbAry[2] = 0;        
            encoder.resetSwitchState();
            encoder.setEncPos(0);
            if (currentOpType == BASIC_OPERATION) {
              sendNode(255);
              currentState = IN_MENU;
              currentMenu = &mainMenu;
            }
          }
          break;
          
        case 'r':
          encoder.setMaxEncPos(20);
          encoder.setColor(encoder.getEncPos()*3, encoder.getGrn(), encoder.getBlu());
          lcdLine1 += F("Set Red, Max ");
          lcdLine1 += String(encoder.getMaxEncPos(), DEC);
          lcdLine2 += F("Value: ");
          lcdLine2 += String(encoder.getEncPos());
          if (encoder.getSwitchState()) {
            tmpRGB[0] = encoder.getEncPos()*7;
            nextColor = 'g';
            encoder.resetSwitchState();
            encoder.setEncPos(0);
          }
          break;
          
        case 'g':
          encoder.setColor(encoder.getRed(), encoder.getEncPos()*3, encoder.getBlu());
          lcdLine1 += F("Set Grn, Max ");
          lcdLine1 += String(encoder.getMaxEncPos(), DEC);
          lcdLine2 += F("Value: ");
          lcdLine2 += String(encoder.getEncPos());
          if (encoder.getSwitchState()) {
            tmpRGB[1] = encoder.getEncPos()*7;
            nextColor = 'b';
            encoder.resetSwitchState();
            encoder.setEncPos(0);
          }
          break;
          
        case 'b':
          encoder.setColor(encoder.getRed(), encoder.getGrn(), encoder.getEncPos()*3);
          lcdLine1 += F("Set Blu, Max ");
          lcdLine1 += String(encoder.getMaxEncPos(), DEC);
          lcdLine2 += F("Value: ");
          lcdLine2 += String(encoder.getEncPos());
          if (encoder.getSwitchState()) {
            tmpRGB[2] = encoder.getEncPos()*7;
            encoder.setEncPos(0);
            encoder.resetSwitchState();
            if (currentOpType == BASIC_OPERATION) {
              rgbAry[0] = tmpRGB[0];
              rgbAry[1] = tmpRGB[1];
              rgbAry[2] = tmpRGB[2];
              sendNode(255);
              currentState = IN_MENU;
              currentMenu = &mainMenu;
            }
          }
          break;
          
      }
      break;
  }

}/* --(end main loop )-- */



void sendNode(byte _address) {

  plHandler.bytPL.bytD = _address;
  plHandler.nybPL.nybA1 = 1;
  
  if(isColor){
      plHandler.nybPL.nybA2 = 1;
      plHandler.bytPL.bytB = rgbAry[0];
      plHandler.bytPL.bytC = rgbAry[1];
      radio_payload = plHandler.unlPL;
      transmitPacket(); // send
      delay(200);
      plHandler.nybPL.nybA2 = 2;
      plHandler.bytPL.bytB = rgbAry[2];
      plHandler.bytPL.bytC = (byte)(20);
      radio_payload = plHandler.unlPL;
      transmitPacket(); // send
      delay(200);
  }
  else{
    plHandler.nybPL.nybA2 = 3;
    plHandler.bytPL.bytB = rgbAry[0];
    plHandler.bytPL.bytC = 20;
    radio_payload = plHandler.unlPL;
    transmitPacket(); // send
  }
  
}

void transmitPacket() {

  radio.stopListening();
  delay(20);
  lastTransmit = radio.write(&radio_payload, 4); // write the payload to the radio and check for success
  delay(20);
  radio.startListening(); // reset the radio to start listening for an ack

  if (lastTransmit) {
    yelStatus.blink(200);
    yelStatus.update();
    last_payload = radio_payload;
  }
  else
    redStatus.blink(200);
    redStatus.update();
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print((lcdLine1));
  lcd.setCursor(0, 1);
  lcd.print((lcdLine2));
}

unsigned long encDBTimeout = 0;
void doEncoder() {
  //unsigned long temp = encoder.getEncDBTimeout();
  if (millis() > encDBTimeout) {
    if ((PIND & (1 << PD2)) == (PIND & (1 << PD4))) {
      encoder.encAdd();
    } else {
      encoder.encSub();
    }
    encDBTimeout = millis() + 50;
  }
}

/* ( THE END ) */
