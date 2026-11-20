// =====================================================================
// lcd.h — LCD 16x2 (HD44780) por registros, bus de 8 bits
// PA0–PA7 = D0–D7 · PC0 = RS · PC1 = E · RW a GND
// pulsoE, instruc, escrib, texto y lcd_iniciar vienen de
// referencia/esclavo_usart1.ino (probados en el entrenador).
// =====================================================================
#ifndef LCD_H
#define LCD_H

#include <stdint.h>

// --- De la referencia ---
void pulsoE(void);                    // pulso en E: el LCD lee el bus
void instruc(uint8_t comando);        // RS = 0: instrucción
void escrib(uint8_t letra);           // RS = 1: dato (un carácter)
void texto(const char t[]);           // escribe una cadena desde la posición actual
void lcd_iniciar(void);               // 8 bits, 2 líneas, sin cursor, borra

// --- Nuevas ---
void lcd_ir(uint8_t fila, uint8_t col);              // fila 0–1, columna 0–15
void lcd_dinero(uint32_t valor);                     // escribe "$1700" desde la posición actual
void lcd_hex(uint8_t valor);                         // escribe un byte en hexadecimal: 0xA4 -> "A4"
void lcd_pantalla(const char linea1[], const char linea2[]); // las dos líneas, rellenas con espacios

#endif
