#include <Arduino.h>

union PayloadHandler
{	
  unsigned long unlPL;
  struct
  {
    byte bytD: 8;
    byte bytC: 8;
    byte bytB: 8;
    byte bytA: 8;
  } bytPL;
  struct
  {
    byte nybD2: 4;
    byte nybD1: 4;
    byte nybC2: 4;
    byte nybC1: 4;
    byte nybB2: 4;
    byte nybB1: 4;
    byte nybA2: 4;
    byte nybA1: 4;
  } nybPL;
};
