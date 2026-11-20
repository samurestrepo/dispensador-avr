// =====================================================================
// teclado.cpp — Teclado 4x4 por registros
// =====================================================================
#include <Arduino.h>        // solo por millis()
#include <avr/io.h>
#include <util/delay.h>     // solo el _delay_us(10) del barrido
#include "config.h"
#include "teclado.h"

// ======================= De la referencia (sin cambios) =======================
const char teclas[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
const uint8_t fila_activa[4] = {0b00000001, 0b00000010, 0b00000100, 0b00001000};
char tecla_anterior = 0;

void teclado_iniciar(void) {
  DDRL  = 0b00000000;       // todo entrada
  PORTL = 0b11110000;       // pull-ups internas SOLO en las columnas (PL4..PL7); filas en 0
}

// Revisa las 4 filas UNA vez. Devuelve la tecla presionada o 0 si no hay ninguna.
char teclado_escanear(void) {
  for (uint8_t fila = 0; fila < 4; fila++) {
    DDRL = fila_activa[fila];            // solo esta fila es salida, en 0
    _delay_us(10);
    uint8_t col = PINL & 0b11110000;
    DDRL = 0b00000000;
    if ((col & 0b00010000) == 0) return teclas[fila][0];
    if ((col & 0b00100000) == 0) return teclas[fila][1];
    if ((col & 0b01000000) == 0) return teclas[fila][2];
    if ((col & 0b10000000) == 0) return teclas[fila][3];
  }
  return 0;
}

// Devuelve la tecla SOLO en el momento en que se presiona (una vez por pulsación); si no, 0.
// No se queda esperando: así el programa sigue atendiendo lo demás.
char teclado_nueva(void) {
  char actual = teclado_escanear();
  char nueva = 0;
  if (actual != 0 && tecla_anterior == 0) {
    nueva = actual;
  }
  tecla_anterior = actual;
  return nueva;
}

// ======================= Nueva =======================
// Antirrebote por tiempo: entre dos lecturas pasan al menos 20 ms, así un rebote
// (que dura unos pocos ms) no se ve como "soltó y volvió a presionar".
static unsigned long ultima_lectura = 0;

char teclado_leer(void) {
  unsigned long ahora = millis();
  if (ahora - ultima_lectura < ANTIRREBOTE_TECLADO_MS) {
    return 0;               // todavía no toca leer
  }
  ultima_lectura = ahora;
  return teclado_nueva();
}
