// =====================================================================
// dispensador.ino — Dispensador de bebidas · Arduino Mega 2560
// Proyecto final de Microprocesadores, Universidad de Caldas, 2026-II.
//
// Un solo sketch: la constante MODO de config.h elige qué se compila,
// el programa completo (MODO_NORMAL) o una prueba de hardware (1 a 5).
//
// MODO_NORMAL: setup() inicia todos los módulos y loop() le da una vuelta
// a la máquina de estados (fsm.cpp). fsm_tick recibe millis() como parámetro
// para que fsm.cpp no dependa de Arduino.h y se pueda probar en el PC.
// =====================================================================
#include "config.h"
#include "lcd.h"
#include "teclado.h"
#include "monedas.h"
#include "reles.h"
#include "tarjeta.h"
#include "fsm.h"
#include "pruebas_hw.h"

void setup() {
#if MODO == MODO_NORMAL
  lcd_iniciar();
  teclado_iniciar();
  monedas_iniciar();
  reles_iniciar();
  tarjeta_iniciar();
  fsm_iniciar(millis());
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
  fsm_tick(millis());
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
