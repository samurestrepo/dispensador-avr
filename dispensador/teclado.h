// =====================================================================
// teclado.h — Teclado 4x4 por registros, leído por INTERRUPCIÓN del Timer2
// PL0–PL3 = filas A–D · PL4–PL7 = columnas 1–4 (pull-up interna)
// teclado_iniciar, teclado_escanear y teclado_nueva vienen de
// referencia/esclavo_usart1.ino (probados en el entrenador).
//
// D17 (25-nov, pedido de la profesora): el teclado se atiende con una
// interrupción. El puerto L no tiene interrupciones por pin (ni INTn ni
// PCINT), y el cableado no se cambia; por eso la interrupción es la del
// Timer2 en modo CTC cada 1 ms: en cada interrupción se lee UNA fila.
// =====================================================================
#ifndef TECLADO_H
#define TECLADO_H

void teclado_iniciar(void);      // columnas con pull-up + Timer2 CTC 1 ms con interrupción
char teclado_escanear(void);     // (referencia) una pasada por las 4 filas: tecla o 0
char teclado_nueva(void);        // (referencia) la tecla solo en el instante en que se presiona

// Lo que llama el programa: devuelve la tecla que la interrupción detectó
// (una vez por pulsación, ya sin rebote) o 0 si no hay ninguna nueva.
char teclado_leer(void);

#endif
