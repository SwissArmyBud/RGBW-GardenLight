#include <Arduino.h>

union ByteBreaker
{	
  byte bytBB;
  struct
  {
    byte duoB2: 2;
    byte duoB1: 2;
    byte duoA2: 2;
    byte duoA1: 2;
  } duoBB;
  struct
  {
    byte nybB: 4;
    byte nybA: 4;
  } nybBB;
};
