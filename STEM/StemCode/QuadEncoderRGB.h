#include <Arduino.h>

class QuadEncoderRGB{
	public:
    void begin(){
      pinMode(redLED, OUTPUT);
      pinMode(grnLED, OUTPUT);
      pinMode(bluLED, OUTPUT);
      analogWrite(redLED, 0);
      analogWrite(grnLED, 0);
      analogWrite(bluLED, 0);
      this->position = 0;
    }
		void update(){
		}
    void setMaxEncoder(byte _maxPos){this->maxPosition = _maxPos;}
		void setColor(byte _red, byte _grn, byte _blu){
			this->redVal = _red;
			this->grnVal = _grn;
			this->bluVal = _blu;
		}
	private:
	
		byte redLED;
		byte grnLED;
		byte bluLED;
		byte swtCON;
		byte encSWA;
		byte encSWB;
		
		bool switchStatus;
    byte curPosition;
		byte maxPosition = 255;
    byte debounceTime = 35;
		byte redVal;
		byte grnVal;
		byte bluVal;
};

attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2

void doEncoder() {  
  if(millis()>bounceTimeout){
    if ((PIND & (1<<PD2)) == (PIND & (1<<PD4))) {
      encoder0Pos++;
    } else{
      encoder0Pos--;
    }
    bounceTimeout =millis() + debounceTime;
  }
}
