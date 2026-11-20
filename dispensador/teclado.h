// =====================================================================
// teclado.h — Teclado 4x4 por registros, no bloqueante
// PL0–PL3 = filas A–D · PL4–PL7 = columnas 1–4 (pull-up interna)
// teclado_iniciar, teclado_escanear y teclado_nueva vienen de
// referencia/esclavo_usart1.ino (probados en el entrenador).
// =====================================================================
#ifndef TECLADO_H
#define TECLADO_H

void teclado_iniciar(void);      // columnas con pull-up, filas sin manejar
char teclado_escanear(void);     // una pasada por las 4 filas: tecla o 0
char teclado_nueva(void);        // la tecla solo en el instante en que se presiona; si no, 0

// Nueva: lo que debe llamar el loop. Llama a teclado_nueva() como máximo cada
// ANTIRREBOTE_TECLADO_MS (20 ms), con millis(). Hace el papel del _delay_ms(20)
// que la referencia tenía al final del loop, pero sin detener el programa.
char teclado_leer(void);

#endif
