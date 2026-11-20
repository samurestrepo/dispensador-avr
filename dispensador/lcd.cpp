// =====================================================================
// lcd.cpp — LCD 16x2 por registros
// Único módulo con esperas de <util/delay.h>: son las que exige el HD44780.
// =====================================================================
#include <avr/io.h>
#include <util/delay.h>
#include "lcd.h"

// ======================= De la referencia (sin cambios) =======================
void pulsoE(void) {
  PORTC |= 0b00000010;      // E = 1
  _delay_us(1);
  PORTC &= ~0b00000010;     // E = 0 -> el LCD lee el bus
}
void instruc(uint8_t comando) {
  PORTA = comando;
  PORTC &= ~0b00000001;     // RS = 0: instrucción
  pulsoE();
  _delay_ms(2);
}
void escrib(uint8_t letra) {
  PORTA = letra;
  PORTC |= 0b00000001;      // RS = 1: dato
  pulsoE();
  _delay_us(50);
}
void texto(const char t[]) {
  uint8_t i = 0;
  while (t[i] != 0) {
    escrib(t[i]);
    i++;
  }
}
void lcd_iniciar(void) {
  DDRA = 0b11111111;
  DDRC |= 0b00000011;
  PORTC &= ~0b00000011;
  _delay_ms(50);
  instruc(0x38);            // 8 bits, 2 líneas
  instruc(0x0C);            // display ON, sin cursor
  instruc(0x06);            // incrementa
  instruc(0x01);            // borrar
}

// ======================= Nuevas =======================

// Mueve el cursor. Instrucción "Set DDRAM address" = 0b1xxxxxxx:
// la línea 1 empieza en la dirección 0x00 y la línea 2 en 0x40.
// (Es lo mismo que instruc(0x80) e instruc(0xC0) de la referencia.)
void lcd_ir(uint8_t fila, uint8_t col) {
  uint8_t direccion = col;
  if (fila == 1) {
    direccion = 0b01000000 + col;     // 0x40: inicio de la línea 2
  }
  instruc(0b10000000 | direccion);    // bit 7 = 1: "fijar dirección DDRAM"
}

// Escribe "$" y el valor en decimal, sin ceros a la izquierda: 1700 -> "$1700".
// Las cifras salen al revés (unidades primero), por eso se guardan y se escriben al final.
void lcd_dinero(uint32_t valor) {
  char cifras[10];            // uint32_t tiene a lo sumo 10 cifras
  uint8_t n = 0;
  escrib('$');
  do {
    cifras[n] = '0' + (valor % 10);
    valor = valor / 10;
    n++;
  } while (valor > 0);
  while (n > 0) {
    n--;
    escrib(cifras[n]);
  }
}

// Escribe una línea completa: el texto (máx. 16 caracteres) y espacios hasta la columna 16.
// Así se borra lo que hubiera antes sin usar instruc(0x01), que hace parpadear el LCD.
static void lcd_linea(const char t[]) {
  uint8_t i = 0;
  while (t[i] != 0 && i < 16) {
    escrib(t[i]);
    i++;
  }
  while (i < 16) {
    escrib(' ');
    i++;
  }
}

void lcd_pantalla(const char linea1[], const char linea2[]) {
  lcd_ir(0, 0);
  lcd_linea(linea1);
  lcd_ir(1, 0);
  lcd_linea(linea2);
}
