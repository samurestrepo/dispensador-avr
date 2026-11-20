// =====================================================================
// reles.cpp — LEDs/relés por registros
// Solo |= y &= ~ sobre PORTC: los bits 0 y 1 son del LCD y no se tocan.
// =====================================================================
#include <avr/io.h>
#include "config.h"
#include "reles.h"

void reles_iniciar(void) {
  DDRC  |= RELES_MASCARA;      // DDRC bits 2–3 = 1: PC2 y PC3 como salidas
  PORTC &= ~RELES_MASCARA;     // PORTC bits 2–3 = 0: los dos LEDs apagados
}

void rele_encender(uint8_t producto) {
  if (producto == PRODUCTO_AGUA) {
    PORTC |= RELE_AGUA_MASCARA;        // PORTC bit 2 = 1: LED agua encendido
  } else if (producto == PRODUCTO_GASEOSA) {
    PORTC |= RELE_GASEOSA_MASCARA;     // PORTC bit 3 = 1: LED gaseosa encendido
  }
}

void rele_apagar(uint8_t producto) {
  if (producto == PRODUCTO_AGUA) {
    PORTC &= ~RELE_AGUA_MASCARA;       // PORTC bit 2 = 0: LED agua apagado
  } else if (producto == PRODUCTO_GASEOSA) {
    PORTC &= ~RELE_GASEOSA_MASCARA;    // PORTC bit 3 = 0: LED gaseosa apagado
  }
}
