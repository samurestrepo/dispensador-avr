// =====================================================================
// config.h — Constantes del dispensador y selector de MODO
// Arduino Mega 2560 (ATmega2560, 16 MHz)
// Todo lo que se puede ajustar sin tocar la lógica está aquí.
// =====================================================================
#ifndef CONFIG_H
#define CONFIG_H

// ------------------------- Selector de modo -------------------------
// Se cambia aquí (o con -DMODO=N al compilar) y se recompila.
#define MODO_NORMAL          0
#define MODO_PRUEBA_LCD      1
#define MODO_PRUEBA_TECLADO  2
#define MODO_PRUEBA_MONEDAS  3   // muestra también el estado crudo de PG0–PG2 (para confirmar el nivel activo)
#define MODO_PRUEBA_RELES    4
#define MODO_PRUEBA_RFID     5   // UID, tipo de tarjeta y saldo del bloque 4. SOLO LECTURA.
#ifndef MODO
#define MODO MODO_NORMAL
#endif

// ------------------------- Mapa de pines -------------------------
// (tabla completa en diseño/04_pines_y_montaje.md)
//   LCD D0–D7 ........ PA0–PA7  (22–29)  salida      -> lcd.cpp
//   LCD RS / EN ...... PC0 / PC1 (37/36) salida      -> lcd.cpp
//   Teclado filas .... PL0–PL3  (49–46)  salida      -> teclado.cpp
//   Teclado columnas . PL4–PL7  (45–42)  entrada + pull-up interna
//   Moneda $100 ...... PG0 (41)          entrada     -> monedas.cpp
//   Moneda $200 ...... PG1 (40)          entrada
//   Moneda $500 ...... PG2 (39)          entrada
//   LED/relé agua .... PC2 (35)          salida      -> reles.cpp
//   LED/relé gaseosa . PC3 (34)          salida
//   RC522 SS/SCK/MOSI/MISO = PB0/PB1/PB2/PB3 (53/52/51/50), RST = PE3 (5) -> tarjeta.cpp (fase 3)
// PORTC es compartido (LCD en PC0–PC1, LEDs en PC2–PC3): solo |= y &= ~.

// Máscaras de los pines que manejan los módulos nuevos.
// (lcd.cpp y teclado.cpp escriben sus máscaras a mano, igual que la referencia.)
#define MONEDA_100_MASCARA   0b00000001   // PG0
#define MONEDA_200_MASCARA   0b00000010   // PG1
#define MONEDA_500_MASCARA   0b00000100   // PG2
#define MONEDAS_MASCARA      0b00000111   // PG0–PG2

#define RELE_AGUA_MASCARA     0b00000100  // PC2
#define RELE_GASEOSA_MASCARA  0b00001000  // PC3
#define RELES_MASCARA         0b00001100  // PC2–PC3

// ------------------------- Productos -------------------------
#define PRODUCTO_AGUA      0
#define PRODUCTO_GASEOSA   1
#define PRECIO_AGUA        500
#define PRECIO_GASEOSA     700
#define MAX_UNIDADES       9      // D11: una cifra en el LCD

// ------------------------- Monedas -------------------------
#define MONEDA_1_VALOR     100    // switch en PG0
#define MONEDA_2_VALOR     200    // switch en PG1
#define MONEDA_3_VALOR     500    // switch en PG2
// D15 [SUP]: 1 = el switch "arriba" da 5 V en el pin; 0 = da 0 V.
// Se confirma con MODO_PRUEBA_MONEDAS (columna "crudo").
#define MONEDA_ACTIVA_EN_ALTO  1

// ------------------------- Tiempos (ms) -------------------------
#define ANTIRREBOTE_MONEDA_MS    30     // D3
#define ANTIRREBOTE_TECLADO_MS   20     // el mismo de la referencia (_delay_ms(20) del loop)
#define TIEMPO_LED_ON_MS         2000   // D12
#define TIEMPO_LED_OFF_MS        500    // D12
#define TIEMPO_MENSAJE_MS        2000   // mensajes temporales (devuelta, saldo insuf., gracias)
#define TIEMPO_ALTERNAR_MS       2000   // CONFIRMAR alterna sus dos pantallas
#define TIEMPO_TARJETA_MS        20000  // D14

// ------------------------- Tarjeta y recarga -------------------------
#define CLAVE_ADMIN            "1234"   // D9
#define RECARGA_MAX_DIGITOS    6        // D10
#define SALDO_MAXIMO           999900UL // D10
#define BLOQUE_SALDO           4        // D6

#endif
