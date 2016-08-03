/*-----( Import needed libraries )-----*/

#include <Wire.h>
#include <U8glib.h>
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

// U8G Library used for HD LCD control.
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NO_ACK); //Creating the HD LCD object for the sketch.
String u8gLine1 = String("");
String u8gLine2 = String("");
String u8gLine3 = String("");
String u8gLine4 = String("");


PayloadHandler plHandler;
ByteBreaker btBreaker;
// Setting up the NRF radio on pins 14 and 10
RF24 radio(14, 10);
const uint64_t pipes[2] = { 0xE8E8F0F0E1LL, 0xF0F0F0F0D2LL }; // radio pipe addresses for the 2 nodes to communicate.
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

unsigned long blinkTimer = 0;
unsigned long displayTimer = 0;

char nextColor = 'r';
bool isColor = false;
byte rgbAry[] = {(byte)0, (byte)0, (byte)0};

ControllerState currentState = IN_MENU;

char timeDiv = 'h';
unsigned short timeAry[] = {0, 0, 0, 0};

byte stemAddress = 0;
bool confirmSelection = false;
byte numNodes = 0;
byte curPower = 255;

const Option goHome = {"Go Back", 0};
const Option mainAry[] = {{"Basic Menu", 1}, {"Adv. Menu", 2}};
const Menu mainMenu = {"Main", mainAry, 2};
const Option bascAry[] = {{"Set White", 3}, {"Set Color", 4}, {"Set On/Off", 5}, goHome};
const Menu bascMenu = {"Basic", bascAry, 4};
const Option advAry[] = {{"Add Wht Node", 3}, {"Add Clr Node", 4}, {"Set Bright %", 5}, {"Reset Chain", 6}, goHome};
const Menu advnMenu = {"Advanced", advAry, 5};

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

  for (int i = 0; i < 4; i++) {
    lcdLine1 = F("Please Wait!");
    lcdLine2 = F("Loading...");
    updateLCD();
    delay(500);
  }

  for (int i = 0; i < 3; i++) { //Write and delete some loading information, eventually can key into fragmented loading process.
    if (i % 2 == 0) {
      lcdLine2 = F("Loading...");
      updateLCD();
    }
    else {
      lcdLine2 = F("");
      updateLCD();
    }
    delay(500);
  }

  // ------- Setup The U8G LCD with a loading screen  -------------
  u8g.setFont(u8g_font_courB12); // set the font for the HD LCD
  for (int i = 0; i < 4; i++) {
    if (i < 1)u8gLine1 = F("**SYS: LOAD**"); // write text, U/L == 0,0
    if (i < 2)u8gLine2 = F("CLOCK: NO!");
    if (i < 3)u8gLine3 = F("RADIO: NO!");
    if (i < 4)u8gLine4 = F("*PLEASE WAIT*");
    updateU8G();
    delay(500);
  }

  // ------- Initialize and setup the NRF radio module -------------
  radio.begin(); // Initialize the NRF radio module.
  radio.setRetries(15, 15); // sets retry attempts and delay
  radio.setPayloadSize(8); // sets payload size

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

  u8gLine1 = (F("**SYSTEM: ON**")); // write text, U/L == 0,0
  u8gLine2 = F("CLOCK: NO!");
  u8gLine3 = F("RADIO: YES!");
  u8gLine4 = F("SYS/LOAD: OK");
  updateU8G();
  delay(500);
}/*--(end setup )---*/

void loop() { /*----( LOOP: RUNS CONSTANTLY )----*/

  unsigned long currentTime = millis();
  encoder.update();

  grnStatus.update(); 
  yelStatus.update();
  redStatus.update();

  if (blinkTimer == 0) {

    currentMenu = &mainMenu;

    yelStatus.blink(200);
    delay(100);
    yelStatus.update();
    encoder.setEncPos(0);
  }

  switch (currentState) {
    default:
      u8gLine1 = (F("SYSTEM: ON "));
      if ((currentTime / 1000) % 2) {
        u8gLine1 += F("*");
      }
      else {
        u8gLine1 += F(" ");
      }
      if (!lastTransmit) {
        u8gLine1 += F("!");
      }
      u8gLine2 = F("Nodes: "); u8gLine2 += numNodes;
      u8gLine3 = F("Power: "); u8gLine3 += (byte)(100 * curPower / 255.0); u8gLine3 += F("%");
      u8gLine4 = F("Up Time: ");
      if ( ((currentTime / 1000) > (60 * 60 * 24 * 7))) {
        u8gLine4 += (byte)((currentTime / 1000) / (60 * 60 * 24 * 7)); u8gLine4 += F("w");
      }
      else if ( ((currentTime / 1000) > (60 * 60 * 24))) {
        u8gLine4 += (byte)((currentTime / 1000) / (60 * 60 * 24)); u8gLine4 += F("d");
      }
      else if ( ((currentTime / 1000) > (60 * 60))) {
        u8gLine4 += (byte)((currentTime / 1000) / (60 * 60)); u8gLine4 += F("h");
      }
      else if ( ((currentTime / 1000) > (60))) {
        u8gLine4 += (byte)((currentTime / 1000) / (60)); u8gLine4 += F("m");
      }
      else {
        u8gLine4 += (byte)(currentTime / (1000)); u8gLine4 += F("s");
      }

  }

  lcdLine1 = F("");
  lcdLine2 = F("");
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
            case 6:
              sendReset(255);
              currentMenu = &mainMenu;
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
            rgbAry[0] = encoder.getEncPos()*3;
            encoder.resetSwitchState();
            encoder.setEncPos(0);
            timeAry[0] = 5;
            timeAry[1] = 0;
            timeAry[2] = 0;
            timeAry[3] = 0;
            if (currentOpType == BASIC_OPERATION) {
              sendNode(255);
              currentState = IN_MENU;
              currentMenu = &mainMenu;
            }
            else {
              nextColor = 'h';
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
            rgbAry[0] = encoder.getEncPos();
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
            rgbAry[1] = encoder.getEncPos();
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
            rgbAry[2] = encoder.getEncPos();
            encoder.setEncPos(0);
            encoder.resetSwitchState();
            timeAry[0] = 5;
            timeAry[1] = 0;
            timeAry[2] = 0;
            timeAry[3] = 0;
            if (currentOpType == BASIC_OPERATION) {
              sendNode(255);
              currentState = IN_MENU;
              currentMenu = &mainMenu;
            }
            else {
              timeDiv = 'h';
              nextColor = 'h';
            }
          }
          break;
        case 'h':
          lcdLine1 += F("Node Hold Time:");
          lcdLine2 += F("Time: ");
          switch (timeDiv) {
            case 'h':
              encoder.setMaxEncPos(12);
              lcdLine2 += String(encoder.getEncPos(), DEC);
              lcdLine2 += F("h");
              if (encoder.getSwitchState()) {
                timeAry[0] += encoder.getEncPos() * 60 * 60;
                timeDiv = 'm';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
            case 'm':
              encoder.setMaxEncPos(59);
              lcdLine2 += String(timeAry[0] / (60 * 60), DEC) + F("h");
              lcdLine2 += String(encoder.getEncPos(), DEC) + F("m");
              if (encoder.getSwitchState()) {
                timeAry[0] += encoder.getEncPos() * 60;
                timeDiv = 's';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
            case 's':
              encoder.setMaxEncPos(59);
              lcdLine2 += String(timeAry[0] / (60 * 60), DEC) + F("h");
              lcdLine2 += String((timeAry[0] % (60 * 60)) / 60, DEC) + F("m");
              lcdLine2 += String(encoder.getEncPos(), DEC) + F("s");
              if (encoder.getSwitchState()) {
                timeAry[0] += encoder.getEncPos();
                nextColor = 't';
                timeDiv = 'h';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
          }
          break;
        case 't':
          encoder.setMaxEncPos(3);
          lcdLine1 += F("Transition Type:");
          lcdLine2 += String(encoder.getEncPos(), DEC);
          if (encoder.getSwitchState()) {
            timeAry[3] = encoder.getEncPos();
            nextColor = 'x';
            encoder.setEncPos(0);
            encoder.resetSwitchState();
          }
          break;
        case 'x':
          lcdLine1 += F("Trnstn Tm, In:");
          lcdLine2 += F("Time: ");
          switch (timeDiv) {
            case 'h':
              encoder.setMaxEncPos(12);
              lcdLine2 += String(encoder.getEncPos(), DEC);
              lcdLine2 += F("h");
              if (encoder.getSwitchState()) {
                timeAry[1] += encoder.getEncPos() * 60 * 60;
                timeDiv = 'm';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
            case 'm':
              encoder.setMaxEncPos(59);
              lcdLine2 += String(timeAry[1] / (60 * 60), DEC) + F("h");
              lcdLine2 += String(encoder.getEncPos(), DEC) + F("m");
              if (encoder.getSwitchState()) {
                timeAry[1] += encoder.getEncPos() * 60;
                timeDiv = 's';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
            case 's':
              encoder.setMaxEncPos(59);
              lcdLine2 += String(timeAry[1] / (60 * 60), DEC) + F("h");
              lcdLine2 += String((timeAry[1] % (60 * 60)) / 60, DEC) + F("m");
              lcdLine2 += String(encoder.getEncPos(), DEC) + F("s");
              if (encoder.getSwitchState()) {
                timeAry[1] += encoder.getEncPos();
                nextColor = 'y';
                timeDiv = 'h';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
          }
          break;
        case 'y':
          lcdLine1 += F("Trnstn Tm, Out:");
          lcdLine2 += F("Time: ");
          switch (timeDiv) {
            case 'h':
              encoder.setMaxEncPos(12);
              lcdLine2 += String(encoder.getEncPos(), DEC);
              lcdLine2 += F("h");
              if (encoder.getSwitchState()) {
                timeAry[2] += encoder.getEncPos() * 60 * 60;
                timeDiv = 'm';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
            case 'm':
              encoder.setMaxEncPos(59);
              lcdLine2 += String(timeAry[2] / (60 * 60), DEC) + F("h");
              lcdLine2 += String(encoder.getEncPos(), DEC) + F("m");
              if (encoder.getSwitchState()) {
                timeAry[2] += encoder.getEncPos() * 60;
                timeDiv = 's';
                encoder.setEncPos(0);
                encoder.resetSwitchState();
              }
              break;
            case 's':
              encoder.setMaxEncPos(59);
              lcdLine2 += String(timeAry[2] / (60 * 60), DEC) + F("h");
              lcdLine2 += String((timeAry[2] % (60 * 60)) / 60, DEC) + F("m");
              lcdLine2 += String(encoder.getEncPos(), DEC) + F("s");
              if (encoder.getSwitchState()) {
                timeAry[2] += encoder.getEncPos();
                nextColor = 'a';
                timeDiv = 'h';
                encoder.setMaxEncPos(15);
                encoder.setEncPos(15);
                encoder.resetSwitchState();
              }
              break;
          }
          break;
        case 'a':
          lcdLine1 += F("Send to Stem:");
          lcdLine2 += F("--> ");
          if (encoder.getEncPos()==15) {
            lcdLine2 += F("ALL");
          }
          else {
            lcdLine2 += encoder.getEncPos();
          }
          if (encoder.getSwitchState()) {
            stemAddress = encoder.getEncPos();
            nextColor = 'c';
            encoder.setMaxEncPos(1);
            encoder.setEncPos(0);
            encoder.resetSwitchState();
          }
          break;          
        case 'c':
          lcdLine1 += F("Confirm?");
          lcdLine2 += F("--> ");
          if (encoder.getEncPos()) {
            lcdLine2 += F("YES");
          }
          else {
            lcdLine2 += F("NO");
          }
          if (encoder.getSwitchState()) {
            encoder.resetSwitchState();
            if (encoder.getEncPos()) {
              sendNode(stemAddress);
            }
            currentMenu = &mainMenu;
            currentState = IN_MENU;
            encoder.setEncPos(0);
          }
          break;
      }
      break;
  }

  //---------- Handle Feedback/Updates/Timeouts for System --------------------
  if (blinkTimer < currentTime) {
    grnStatus.blink(100);
    blinkTimer = currentTime + 3000;
  }

  if (displayTimer < currentTime) {
    updateLCD();
    updateU8G();

    displayTimer = currentTime + 300;
  }

}/* --(end main loop )-- */



void sendNode(byte _address) {

  plHandler.bytPL.bytD = _address;

  if (currentOpType == BASIC_OPERATION) {
    sendReset(_address);
  }
  
  delay(20);
  plHandler.nybPL.nybA1 = 1;
  btBreaker.bytBB = rgbAry[0];
  btBreaker.duoBB.duoA1 = 2 + isColor;
  plHandler.nybPL.nybA2 = btBreaker.nybBB.nybA;
  plHandler.nybPL.nybB2 = btBreaker.nybBB.nybB;
  btBreaker.bytBB = rgbAry[1];
  plHandler.bytPL.bytD = btBreaker.duoBB.duoA2;
  plHandler.nybPL.nybC1 = btBreaker.nybBB.nybB;
  btBreaker.bytBB = rgbAry[2];
  plHandler.nybPL.nybC2 = btBreaker.nybBB.nybB;
  btBreaker.duoBB.duoA1 = plHandler.bytPL.bytD;
  plHandler.nybPL.nybB1 = btBreaker.nybBB.nybA;
  plHandler.bytPL.bytD = _address;
  radio_payload = plHandler.unlPL;
  transmitPacket();   // send color packet
  delay(20);
  btBreaker.duoBB.duoA1 = 1; //set payload as timing
  for (int i = 0; i < 4; i++) {
    if (lastTransmit) {
      btBreaker.duoBB.duoA2 = i;
      plHandler.nybPL.nybA2 = btBreaker.nybBB.nybA;
      plHandler.unsPL.unsS = timeAry[i];
      radio_payload = plHandler.unlPL;
      transmitPacket();
      delay(50);
    }
  }
  if (lastTransmit) {
    numNodes++;
  }

}

void transmitPacket() {

  radio.stopListening();
  delay(15);
  lastTransmit = radio.write(&radio_payload, sizeof(unsigned long)); // write the payload to the radio and check for success
  delay(15);
  radio.startListening(); // reset the radio to start listening for an ack

  if (lastTransmit) {
    yelStatus.blink(200);
    last_payload = radio_payload;
  }
  else
    redStatus.blink(200);

  delay(50);
}

void updateU8G() {
  u8g.firstPage(); //begin the screen writing process
  do {
    u8g.setPrintPos(0, 14); // different method of writing text, U/L == 0,0
    u8g.print((u8gLine1)); // write text, U/L == 0,0
    u8g.setPrintPos(0, 30); // different method of writing text, U/L == 0,0
    u8g.print((u8gLine2));
    u8g.setPrintPos(0, 46); // different method of writing text, U/L == 0,0
    u8g.print((u8gLine3));
    u8g.setPrintPos(0, 62); // different method of writing text, U/L == 0,0
    u8g.print((u8gLine4));
  } while (u8g.nextPage()); // end the screen writing process
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

void sendReset(byte _address) {
  plHandler.nybPL.nybA1 = 1;
  plHandler.nybPL.nybA2 = 0;
  plHandler.nybPL.nybB1 = 0;
  plHandler.nybPL.nybB2 = 0;
  plHandler.nybPL.nybC1 = 0;
  plHandler.nybPL.nybC2 = 0;
  plHandler.bytPL.bytD = _address;
  radio_payload = plHandler.unlPL;
  transmitPacket();
  numNodes = 0;
}





/* ( THE END ) */
