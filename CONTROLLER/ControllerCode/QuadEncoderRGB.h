#include <Arduino.h>
using namespace std;

class QuadEncoderRGB{
  public:
    QuadEncoderRGB(){};
    QuadEncoderRGB(byte _redPin, byte _grnPin, byte _bluPin, byte _swtBtn, byte _encA, byte _encB, byte _swtDB, byte _maxEnc){
      
      this->redLED = _redPin;
      this->grnLED = _grnPin;
      this->bluLED = _bluPin;
      this->redVal = 0;
      this->grnVal = 0;
      this->bluVal = 0;
      pinMode(redLED, OUTPUT);
      pinMode(grnLED, OUTPUT);
      pinMode(bluLED, OUTPUT);
      analogWrite(redLED, 255-this->redVal);
      analogWrite(grnLED, 255-this->grnVal);
      analogWrite(bluLED, 255-this->bluVal);

      this->swtBTN = _swtBtn;
      this->swtDB = _swtDB;
      pinMode(swtBTN, INPUT);
      digitalWrite(swtBTN, LOW);
      curSwitch = false;
      lstSwitch = curSwitch;

      this->encA = _encA;
      this->encB = _encB;
      pinMode(encA, INPUT);
      pinMode(encB, INPUT);
      digitalWrite(encA, HIGH);
      digitalWrite(encB, HIGH);
      this->maxPos = _maxEnc;
      this->encPos = 0;
      this->curPos = 0;
      
    }
    void update(){
      
      analogWrite(redLED, 255-(redVal*2));
      analogWrite(grnLED, 255-(grnVal*2));
      analogWrite(bluLED, 255-(bluVal*2));

      if(this->maxPos==0){
        this->encPos = 0;
        this->curPos = 0;
      }
      while (this->encPos>this->maxPos){
        this->encPos = this->encPos - (this->maxPos + 1);
      }
      while (this->encPos<0){
        this->encPos = this->encPos + (this->maxPos + 1);
      }
      this->curPos = this->encPos;

      curSwitch = digitalRead(swtBTN);
      if(this->lstSwitch == this->curSwitch){
        if(millis()>this->swtDBTimeout){
          this->pressedState = this->curSwitch;
        }
      }
      else{
        this->lstSwitch = this->curSwitch;
        this->swtDBTimeout = millis() + this->swtDB;
      }
      
    }

    byte getEncPos(){this->update();return this->curPos;}
    void setEncPos(byte _pos){this->encPos = _pos;this->update();}
    void setMaxEncPos(byte _maxPos){this->maxPos = _maxPos;this->update();}
    byte getMaxEncPos(){return this->maxPos;}
    bool encStateEqual(){return digitalRead(this->encA)==digitalRead(this->encB);}

    bool getSwitchState(){this->update();return this->pressedState;}
    void resetSwitchState(){this->pressedState = false; this->swtDBTimeout = (millis() + 250);this->update();}
    
    void setColor(byte _red, byte _grn, byte _blu){
      this->redVal = _red;
      this->grnVal = _grn;
      this->bluVal = _blu;
      this->update();
    }
    
    byte getRed(){return this->redVal;}
    byte getGrn(){return this->grnVal;}
    byte getBlu(){return this->bluVal;}

    void encAdd(){this->encPos++;this->update();}
    void encSub(){this->encPos--;this->update();}
    
  private:
  
    byte redLED;
    byte grnLED;
    byte bluLED;
    byte redVal;
    byte grnVal;
    byte bluVal;
    
    byte swtBTN;
    bool lstSwitch;
    bool curSwitch;
    bool pressedState;
    unsigned long swtDBTimeout;
    byte swtDB;
    
    byte encA;
    byte encB;
    short encPos;
    short curPos;
    byte maxPos;
};

