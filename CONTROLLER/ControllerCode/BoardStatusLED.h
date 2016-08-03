#include <Arduino.h>

class BoardStatusLED{
	public:
		BoardStatusLED(){};
		BoardStatusLED(byte _pin, bool _defaultState){
			this->LEDpin = _pin;
			this->defaultState = _defaultState;
			pinMode(this->LEDpin, OUTPUT);
			digitalWrite(this->LEDpin, _defaultState);
			this->timerA = millis();
			this->timerB = millis();
		}
		void update(){
			if(this->timerA<millis()&&this->timerB>millis()){
				digitalWrite(this->LEDpin, !(this->defaultState));
			}
			else{
				digitalWrite(this->LEDpin, this->defaultState);
			}
		}
		void blink(unsigned short _time){
			this->timerA = millis();
			this->timerB = this->timerA + _time;
			this->update();
		}
    void turnOn(){digitalWrite(this->LEDpin, !(this->defaultState));}
	private:
		unsigned long timerA;
		unsigned long timerB;
		bool defaultState;
		byte LEDpin;
};
