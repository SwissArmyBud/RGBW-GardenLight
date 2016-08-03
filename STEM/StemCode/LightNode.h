#include <Arduino.h>

using namespace std;

class LightNode{
	public:
		LightNode(){};
		LightNode(byte _redNew, byte _greenNew, byte _blueNew, byte _whiteNew, unsigned short _holdTime, unsigned short _timeA, unsigned short _timeB, byte _transition){
      this->FUCKOFF = 0;
      this->nodeR = _redNew;
			this->nodeG = _greenNew;
			this->nodeB = _blueNew;
      this->nodeW = _whiteNew;
      this->holdTime = _holdTime;
			this->transTimeA = _timeA;
			this->transTimeB = _timeB;
			this->transType = _transition;
		}
    byte getRed(){return this->nodeR;}
    byte getGrn(){return this->nodeG;}
    byte getBlu(){return this->nodeB;}
    byte getWht(){return this->nodeW;}
    unsigned short getHoldTime(){return this->holdTime;}
    unsigned short getTransA(){return this->transTimeA;}
    unsigned short getTransB(){return this->transTimeB;}
    byte getTransType(){return this-> transType;}
	private:
    byte FUCKOFF;
    byte nodeR;
		byte nodeG;
		byte nodeB;
    byte nodeW;
    unsigned short holdTime;
		unsigned short transTimeA;
		unsigned short transTimeB;
		byte transType;
};
