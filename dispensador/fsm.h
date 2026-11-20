// =====================================================================
// fsm.h — Máquina de estados del dispensador (diseño/05_teclado_y_estados.md)
//
// No escribe registros ni usa Arduino.h: solo llama a las funciones de
// lcd, teclado, monedas, reles, tarjeta, pedido y saldo. La hora entra como
// parámetro (loop() le pasa millis()); así el mismo fsm.cpp se compila en el
// PC con módulos simulados y se prueba sin hardware (pruebas_pc/prueba_fsm.cpp).
// =====================================================================
#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>

// Los nombres son los de la tabla de estados de diseño/05.
typedef enum {
  INICIO,
  SELECCION,
  CONFIRMAR,
  METODO_PAGO,
  PAGO_MONEDAS,
  PAGO_TARJETA,
  DISPENSAR,
  GRACIAS,
  RECARGA_CLAVE,
  RECARGA_MONTO,
  RECARGA_TARJETA,
  CONSULTA_SALDO,
  VERIFICAR_TARJETA
} Estado;

void fsm_iniciar(uint32_t ahora);   // va en setup(), después de iniciar todos los módulos
void fsm_tick(uint32_t ahora);      // va en loop(): una vuelta, nunca se queda esperando

// Para las pruebas de PC (y para depurar).
Estado fsm_estado(void);
bool fsm_mostrando_mensaje(void);

#endif
