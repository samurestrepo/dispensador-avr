// =====================================================================
// monedas.cpp — Switches de monedas por registros, antirrebote con millis()
//
// Cada switch se vigila por separado con tres datos:
//   crudo_anterior[i] : lo que se leyó la vez pasada (con rebotes)
//   t_cambio[i]       : cuándo cambió por última vez la lectura cruda
//   estable[i]        : el valor aceptado (lleva 30 ms sin cambiar)
// Hay moneda cuando "estable" pasa de 0 a 1 (D3).
// =====================================================================
#include <Arduino.h>        // solo por millis()
#include <avr/io.h>
#include "config.h"
#include "monedas.h"

static const uint8_t mascara[3] = {MONEDA_100_MASCARA, MONEDA_200_MASCARA, MONEDA_500_MASCARA};
static const uint16_t valor[3]  = {MONEDA_1_VALOR, MONEDA_2_VALOR, MONEDA_3_VALOR};

static uint8_t crudo_anterior[3];
static uint8_t estable[3];
static unsigned long t_cambio[3];

// Lectura cruda del puerto: bit i = nivel eléctrico del pin PGi.
uint8_t monedas_crudo(void) {
  return PING & MONEDAS_MASCARA;          // PING: lee los pines; solo PG0–PG2
}

// Lectura "activa": bit i = 1 si el switch i está en posición de moneda,
// sin importar si el entrenador da 5 V o 0 V en esa posición (D15).
static uint8_t monedas_activas(void) {
#if MONEDA_ACTIVA_EN_ALTO
  return monedas_crudo();
#else
  return (~monedas_crudo()) & MONEDAS_MASCARA;   // se invierten solo PG0–PG2
#endif
}

void monedas_iniciar(void) {
  DDRG  &= ~MONEDAS_MASCARA;   // DDRG bits 0–2 = 0: PG0–PG2 como entradas
  PORTG &= ~MONEDAS_MASCARA;   // PORTG bits 0–2 = 0: sin pull-up (el entrenador trae resistencias)
  monedas_rearmar();
}

// D4: lo que esté arriba en este momento NO cuenta como moneda.
void monedas_rearmar(void) {
  uint8_t activas = monedas_activas();
  unsigned long ahora = millis();
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t nivel = 0;
    if ((activas & mascara[i]) != 0) {
      nivel = 1;
    }
    crudo_anterior[i] = nivel;
    estable[i] = nivel;
    t_cambio[i] = ahora;
  }
}

// No bloqueante: se llama en cada vuelta del loop.
// Devuelve UNA moneda por llamada; si dos switches suben a la vez, la otra sale en la siguiente.
uint16_t monedas_leer(void) {
  uint8_t activas = monedas_activas();
  unsigned long ahora = millis();
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t nivel = 0;
    if ((activas & mascara[i]) != 0) {
      nivel = 1;
    }
    if (nivel != crudo_anterior[i]) {
      // La lectura cambió (puede ser rebote): se vuelve a contar el tiempo.
      crudo_anterior[i] = nivel;
      t_cambio[i] = ahora;
    } else if (nivel != estable[i] && ahora - t_cambio[i] >= ANTIRREBOTE_MONEDA_MS) {
      // Lleva 30 ms sin cambiar y es distinta de la aceptada: se acepta.
      estable[i] = nivel;
      if (nivel == 1) {
        return valor[i];          // flanco 0->1: entró una moneda
      }
    }
  }
  return 0;
}
