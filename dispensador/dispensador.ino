// =====================================================================
// dispensador.ino — Dispensador de bebidas · Arduino Mega 2560
// Proyecto final de Microprocesadores, Universidad de Caldas, 2026-II.
//
// Un solo sketch: la constante MODO de config.h elige qué se compila,
// el programa completo (MODO_NORMAL) o una prueba de hardware (1 a 5).
//
// Fase 1: solo periféricos por registros y pruebas. En MODO_NORMAL se
// inicializa todo y se muestra un mensaje; la máquina de estados llega después.
// =====================================================================
#include "config.h"
#include "lcd.h"
#include "teclado.h"
#include "monedas.h"
#include "reles.h"
#include "pruebas_hw.h"

void setup() {
#if MODO == MODO_NORMAL
  lcd_iniciar();
  teclado_iniciar();
  monedas_iniciar();
  reles_iniciar();
  lcd_pantalla("Dispensador", "Fase 1");
#elif MODO == MODO_PRUEBA_LCD
  prueba_lcd_iniciar();
#elif MODO == MODO_PRUEBA_TECLADO
  prueba_teclado_iniciar();
#elif MODO == MODO_PRUEBA_MONEDAS
  prueba_monedas_iniciar();
#elif MODO == MODO_PRUEBA_RELES
  prueba_reles_iniciar();
#elif MODO == MODO_PRUEBA_RFID
  prueba_rfid_iniciar();
#else
#error "MODO no valido: use 0 a 5 (ver config.h)"
#endif
}

void loop() {
#if MODO == MODO_NORMAL
  // Fase 1: nada que hacer todavía (aquí irá fsm_tick()).
#elif MODO == MODO_PRUEBA_LCD
  prueba_lcd_ciclo();
#elif MODO == MODO_PRUEBA_TECLADO
  prueba_teclado_ciclo();
#elif MODO == MODO_PRUEBA_MONEDAS
  prueba_monedas_ciclo();
#elif MODO == MODO_PRUEBA_RELES
  prueba_reles_ciclo();
#elif MODO == MODO_PRUEBA_RFID
  prueba_rfid_ciclo();
#endif
}
