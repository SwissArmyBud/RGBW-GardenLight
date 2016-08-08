
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

/*-----( Declare Global Objects )-----*/

// Use I2C scanner to find address of I2C controller.
// Set I2C pins :     addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Creating the LCD object for the sketch.
String lcdLine1 = String(""); // Global
String lcdLine2 = String("");

// Byte level manipulators, see source files for details
PayloadHandler plHandler;
ByteBreaker btBreaker;

// Setting up the NRF radio on pins 14 and 10 (LIBRARY INCLUDED, SEE SUPPORT DOCS FOR REFERENCE)
RF24 radio(14, 10);
const uint64_t pipes[2] = { 0xF0A0F0A5AALL, 0xF0A0F0A566LL }; // radio pipe addresses for the 2 nodes to communicate.
unsigned long radio_payload = 0; // default, empty payload for the radio module
bool lastTransmit = true; // boolean for tracking transimission success/failure

// Setup QuadEncoder with RGB/Switch -  RD GR BL SW eA eB sT
QuadEncoderRGB encoder = QuadEncoderRGB(3, 5, 9, 7, 2, 4, 50);

// Setup the custom board status LEDs, (PER INCLUDED DESIGN, PINS ARE ALWAYS 15, 16, 17)
BoardStatusLED redStatus(15, LOW);
BoardStatusLED yelStatus(16, LOW);
BoardStatusLED grnStatus(17, LOW);

// Setup a state machine to keep track of the current task
enum ControllerState {
  IN_MENU,
  CREATING_NODE
};
ControllerState currentState = IN_MENU;

// Another (sub) state machine controller, this cycles through available settings inside each ControllerState
char nextColor = 'r';

// Setup timer for an LED heartbeat
unsigned long blinkTimer = 0;

// Setup timer for refreshing screen contents
unsigned long displayTimer = 0;

// Setup timer for encoder de-bounce
unsigned long encDBTimeout = 0;

// Boolean for keeping track of what color space we are working in (b/w or color)
bool isColor = false;

// Two 3xBYTE(RGB/Wxx) arrays for keeping track of an upcoming color and the current color chosen
byte rgbAry[] = {(byte)0, (byte)0, (byte)0};
byte tmpRGB[] = {(byte)0, (byte)0, (byte)0};

// Menu System
//
// Menu contains (Title, Option Array, Number of Options)
// Options contain (Title, Option Case) where Option Case is unique and trips switch in main loop
const Option goHome = {"Go Back", 0};
const Option mainAry[] = {{"Basic Menu", 1}, {"Adv. Menu", 2}};
const Menu mainMenu = {"Main", mainAry, 2};
const Option bascAry[] = {{"Set White", 3}, {"Set Color", 4}, {"Set On/Off", 5}, goHome};
const Menu bascMenu = {"Basic", bascAry, 4};
const Option advAry[] = {{"Set Bright %", 5}, goHome};
const Menu advnMenu = {"Advanced", advAry, 2};

const Menu* currentMenu = &bascMenu;

/*----( SETUP: RUNS ONCE )----*/
void setup() {

  attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2

  // ------- Quick blinks of 16x2 LCD backlight  -------------
  lcd.begin(16, 2);
  lcd.clear();
  for (int i = 0; i < 2; i++) {
    lcd.backlight();
    delay(250);
    lcd.noBacklight();
    delay(250);
  }
  lcd.backlight(); // finish with 16x2 LCD backlight on

  // ------- Write characters on the display ------------------
  lcdLine1 = F("PLEASE WAIT - ");
  lcdLine2 = F("Loading...");
  updateLCD();


  // ------- Initialize and setup the NRF radio module -------------
  radio.begin();
  radio.setChannel(100);
  radio.setRetries(10,10);
  radio.setPayloadSize(8);
  
  if (false) { // Set the pipe direction for listening/writing
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
  } else {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1, pipes[0]);
  }
  
  radio.startListening(); // Start the NRF radio and have it listen for traffic

  // Cycle through the RGB lights on the encoder to signal a finished setup section
  encoder.setColor(100, 0, 0);
  delay(300);
  encoder.setColor(0, 100, 0);
  delay(300);
  encoder.setColor(0, 0, 100);
  delay(300);
  encoder.setColor(0, 0, 0);
  
}/*--( end setup )---*/

void loop() { /*----( LOOP: RUNS CONSTANTLY )----*/

  //---------- Handle Feedback/Updates/Timeouts for System --------------------
  unsigned long currentTime = millis(); // update the current time

  if (blinkTimer == 0) { // if this is the first loop, make sure to , , 
    encoder.setEncPos(0); // reset the encoder
    currentMenu = &mainMenu; // use the main menu
    yelStatus.blink(200); // and blink a yellow status LED to confirm loop has started
    yelStatus.update();
  }
  if (blinkTimer < currentTime) { // run a heartbeat system every 3 seconds, using the green status LED
    grnStatus.blink(100);
    sendNode(255); // also resend node at the same time, to ensure any "busy" lights get updated eventually
    blinkTimer = currentTime + 3000;
  }
  if (displayTimer < currentTime) { // update the LCD screen, 3 times per second
    updateLCD();
    displayTimer = currentTime + 300;
  }
  
  lcdLine1 = F(""); // reset LCD text
  lcdLine2 = F("");
  
  encoder.update(); // update encoder and status LEDs
  grnStatus.update(); 
  yelStatus.update();
  redStatus.update();

  //---------- Handle State Machine Movements for currentState --------------------
  switch (currentState) {
    
    case IN_MENU:
    
      encoder.setColor(0, 0, 0); // encoder is dark when in menu
      lcdLine1 += F("M: "); // screen gives menu prompt when in menu
      lcdLine1 += (*currentMenu).name; // screen gives current menu name while in menu

      if ((*currentMenu).size == 0) { // if the menu is "empty" 
        encoder.setMaxEncPos(0); // reset the encoder
        encoder.setEncPos(0);
        lcdLine2 += F("No Options"); // tell the user
        if (encoder.getSwitchState()) { // wait for button press, then go home
          encoder.resetSwitchState();
          currentMenu = &mainMenu;
        }
      }

      else { // if the menu is NOT empty
        encoder.setMaxEncPos((*currentMenu).size - 1); // set the encoder to allow for movement through menu
        Option oPnt = ((*currentMenu).list)[encoder.getEncPos()]; // grab the current option from the encoder position
        lcdLine1 += " " + String(encoder.getEncPos() + 1, DEC); // display position inside the menu on screen line 1
        lcdLine1 += F("/");
        lcdLine1 += String((*currentMenu).size, DEC); // and then show the full menu size
        lcdLine2 += F("O: "); // on screen line 2 show the current option
        lcdLine2 += oPnt.name; // by using the name from the option
        
        if (encoder.getSwitchState()) { // if the button is pushed, activate whatever the current option code is
          encoder.setEncPos(0);
          encoder.resetSwitchState();
          switch (oPnt.code) {
            case 0: // (GO HOME)
              currentState = IN_MENU;
              currentMenu = &mainMenu;
              break;
            case 1: // (GO BASIC MENU)
              currentState = IN_MENU;
              currentMenu = &bascMenu;
              break;
            case 2: // (GO ADVANCED MENU)
              currentState = IN_MENU;
              currentMenu = &advnMenu;
              break;
            case 3: // (CREATE NODE WHITE)
              currentState = CREATING_NODE;
              nextColor = 'w';
              break;
            case 4: // (CREATE NODE COLOR)
              currentState = CREATING_NODE;
              nextColor = 'r';
              break;
            case 5: // (SET ON/OFF)
              //masterSet(0);
              break;
          }
        }
        
      }
      break;
      
    case CREATING_NODE:
    
      switch (nextColor) {
        
        case 'w': // sets WHITE node intensity and then goes back to main menu
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
            isColor = false;
            sendNode(255);
            currentState = IN_MENU;
            currentMenu = &mainMenu;
          }
          break;
          
        case 'r': // sets RED node intensity and then moves to menu to pick GREEN
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
          
        case 'g': // sets WHITE node intensity and then moves to menu to pick BLUE
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
          
        case 'b': // sets BLUE node intensity and then goes back to main menu
          encoder.setColor(encoder.getRed(), encoder.getGrn(), encoder.getEncPos()*3);
          lcdLine1 += F("Set Blu, Max ");
          lcdLine1 += String(encoder.getMaxEncPos(), DEC);
          lcdLine2 += F("Value: ");
          lcdLine2 += String(encoder.getEncPos());
          if (encoder.getSwitchState()) {
            tmpRGB[2] = encoder.getEncPos()*7;
            encoder.setEncPos(0);
            encoder.resetSwitchState();
            rgbAry[0] = tmpRGB[0];
            rgbAry[1] = tmpRGB[1];
            rgbAry[2] = tmpRGB[2];
            isColor = true;
            sendNode(255);
            currentState = IN_MENU;
            currentMenu = &mainMenu;
          }
          break;
          
      }
      break;
  }

}/* --(end main loop )-- */


// Sub-routine to send the selected color to any stems that are listening
void sendNode(byte _address) {

  plHandler.bytPL.bytD = _address; // last byte is always the address
  plHandler.nybPL.nybA1 = 1; // A1 is a protocol value
  
  if(isColor){ // if rgbAry represents RGB
    plHandler.nybPL.nybA2 = 1; // A2 is packet type for color, 1 = RG
    plHandler.bytPL.bytB = rgbAry[0]; // Red
    plHandler.bytPL.bytC = rgbAry[1]; // Green
    radio_payload = plHandler.unlPL; // load payload into place from handler
    transmitPacket(); // send
    delay(200);
    plHandler.nybPL.nybA2 = 2; // A2 is packet type for color, 2 = B+Hash
    plHandler.bytPL.bytB = rgbAry[2]; // Blue
    plHandler.bytPL.bytC = 20; // Hash
    radio_payload = plHandler.unlPL; // load payload into place from handler
    transmitPacket(); // send
    delay(200);
  }
  else{ // if rgbAry represents W
    plHandler.nybPL.nybA2 = 3; // A2 is packet type for color, 3 = W+Hash
    plHandler.bytPL.bytB = rgbAry[0]; // White
    plHandler.bytPL.bytC = 20; // Hash
    radio_payload = plHandler.unlPL; // load payload into place from handler
    transmitPacket(); // send
  }
  
}

// Complete sub-routine for sending the variable (radio_payload) across the radio
void transmitPacket() {

  radio.stopListening(); // stop listening in preparation for outgoing transmission
  delay(20); // allow radio to transition
  lastTransmit = radio.write(&radio_payload, sizeof(radio_payload)); // write the payload to the radio and check for success
  delay(20); // allow radio to transition
  radio.startListening(); // reset the radio to start listening again

  if (lastTransmit) { // if transmission succeeded, blink yellow status LED
    yelStatus.blink(200);
    yelStatus.update();
  }
  else // if transmission failed, blink red status LED
    redStatus.blink(200);
    redStatus.update();
}

// Sub-routine for keeping the LCD screen updated, code should be self-explainatory
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print((lcdLine1));
  lcd.setCursor(0, 1);
  lcd.print((lcdLine2));
}

// Sub-routine for keeping track of encoder position
void doEncoder() {
  if (millis() > encDBTimeout) { // check the current time vs the de-bounce timer
    if ((PIND & (1 << PD2)) == (PIND & (1 << PD4))) { // fancy FAST check of pins, pins equal is CW, pins unequal is CCW
      encoder.encAdd(); // call for position increase with CW motion
    } else {
      encoder.encSub(); // call for position increase with CCW motion
    }
    encDBTimeout = millis() + 50; // renew de-bounce timer
  }
}

/* ( THE END ) */
