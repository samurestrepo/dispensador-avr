// =====================================================================
// teclado.cpp — Teclado 4x4 por registros, leído por interrupción (D17)
// =====================================================================
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>     // solo el _delay_us(10) de teclado_escanear (referencia)
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

// Revisa las 4 filas UNA vez. Devuelve la tecla presionada o 0 si no hay ninguna.
// (Ya no la usa el programa: la reemplaza la interrupción de abajo. Se deja
// porque es la versión probada y sirve para comparar.)
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
char teclado_nueva(void) {
  char actual = teclado_escanear();
  char nueva = 0;
  if (actual != 0 && tecla_anterior == 0) {
    nueva = actual;
  }
  tecla_anterior = actual;
  return nueva;
}

// ======================= D17: lectura por interrupción =======================
// Cada 1 ms el Timer2 interrumpe. La ISR:
//   1) lee las columnas de la fila que activó en la interrupción ANTERIOR
//      (ya pasó 1 ms: la línea está asentada, no hace falta _delay_us),
//   2) activa la fila siguiente.
// Cada 4 interrupciones (4 ms) completa una pasada por las 4 filas.
// Antirrebote: una tecla se acepta cuando la pasada da el MISMO resultado
// TECLADO_PASADAS_ESTABLES veces seguidas (5 × 4 ms = 20 ms = ANTIRREBOTE_TECLADO_MS).
// La ISR no llama funciones ni espera: solo lee, compara y guarda.
#define TECLADO_PASADAS_ESTABLES  (ANTIRREBOTE_TECLADO_MS / 4)

// "volatile": la cambia la ISR y la lee el programa principal; sin volatile el
// compilador podría guardarla en un registro y no ver el cambio.
static volatile char tecla_lista = 0;     // tecla nueva que espera a teclado_leer()
static uint8_t fila_isr = 0;              // fila activa en la ISR (0..3)
static char en_pasada = 0;                // tecla vista en la pasada en curso
static char candidata = 0;                // resultado de las últimas pasadas
static uint8_t pasadas_iguales = 0;       // cuántas pasadas seguidas dieron "candidata"
static char estable = 0;                  // última tecla aceptada (0 = ninguna)

void teclado_iniciar(void) {
  DDRL  = 0b00000000;       // DDRL: todo entrada
  PORTL = 0b11110000;       // PORTL: pull-ups internas SOLO en las columnas (PL4..PL7); filas en 0
  DDRL  = fila_activa[0];   // DDRL: activa la fila A (PL0 salida en 0) para la primera interrupción

  // Timer2 en CTC, 1 ms. tiempo = N · (OCR2A + 1) / f_clk = 64 · 250 / 16 MHz = 1 ms
  TCCR2A = 0b00000010;      // TCCR2A: COM2A/COM2B = 00 (no toca pines) · WGM21:20 = 10 -> CTC (junto con WGM22 = 0)
  TCCR2B = 0b00000100;      // TCCR2B: WGM22 = 0 · CS22:20 = 100 -> reloj / 64 (en el Timer2, 100 es /64)
  OCR2A  = 249;             // OCR2A: tope del conteo -> 250 tics de 4 µs = 1 ms
  TCNT2  = 0;               // TCNT2: conteo desde 0
  TIFR2  = 0b00000010;      // TIFR2: borra OCF2A (bit 1) escribiendo 1, por si quedó levantada
  TIMSK2 = 0b00000010;      // TIMSK2: OCIE2A (bit 1) = 1 -> interrupción por comparación A
  // La I de SREG ya la puso el núcleo de Arduino (sei) antes de setup().
}

ISR(TIMER2_COMPA_vect) {
  // 1) Columnas de la fila activa: una columna en 0 = tecla presionada
  uint8_t col = PINL & 0b11110000;                 // PINL: solo PL4..PL7
  if (en_pasada == 0) {
    if      ((col & 0b00010000) == 0) en_pasada = teclas[fila_isr][0];
    else if ((col & 0b00100000) == 0) en_pasada = teclas[fila_isr][1];
    else if ((col & 0b01000000) == 0) en_pasada = teclas[fila_isr][2];
    else if ((col & 0b10000000) == 0) en_pasada = teclas[fila_isr][3];
  }

  // 2) ¿Terminó la pasada por las 4 filas? -> antirrebote
  fila_isr++;
  if (fila_isr == 4) {
    fila_isr = 0;
    if (en_pasada == candidata) {
      if (pasadas_iguales < TECLADO_PASADAS_ESTABLES) {
        pasadas_iguales++;
      }
    } else {
      candidata = en_pasada;                       // cambió: vuelve a contar
      pasadas_iguales = 0;
    }
    if (pasadas_iguales == TECLADO_PASADAS_ESTABLES && candidata != estable) {
      estable = candidata;                         // estado firme (presionada o suelta)
      if (estable != 0) {
        tecla_lista = estable;                     // flanco: se acaba de presionar
      }
    }
    en_pasada = 0;
  }

  // 3) Activa la fila siguiente; se leerá en la próxima interrupción (1 ms)
  DDRL = fila_activa[fila_isr];                        // DDRL: solo esa fila como salida (en 0 por PORTL)
}

// Leer y borrar tecla_lista debe ser una sola operación: si la ISR entrara
// entre la lectura y el borrado, se perdería una tecla. Por eso se apagan las
// interrupciones un instante (se guarda SREG y se restaura, en vez de sei()).
char teclado_leer(void) {
  uint8_t sreg = SREG;      // SREG: guarda el bit I tal como estaba
  cli();                    // I = 0
  char t = tecla_lista;
  tecla_lista = 0;
  SREG = sreg;              // SREG: restaura I
  return t;
}
