// Arduino.h mínimo para compilar MFRC522.cpp en el PC contra el emulador
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper*)(s))
#define SS 53
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
unsigned long millis(void);
void delay(unsigned long);
void delayMicroseconds(unsigned int);
inline void yield(void) {}
#define DEC 10
#define HEX 16
struct StubSerial {
  template<class T> void print(T) {}
  template<class T> void print(T, int) {}
  template<class T> void println(T) {}
  template<class T> void println(T, int) {}
  void println() {}
};
extern StubSerial Serial;
