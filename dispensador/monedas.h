// =====================================================================
// monedas.h — Tres switches = tres monedas, por registros
// PG0 = $100 · PG1 = $200 · PG2 = $500 (el entrenador trae las resistencias)
// Una moneda = flanco 0->1 del switch, estable durante 30 ms (D3).
// =====================================================================
#ifndef MONEDAS_H
#define MONEDAS_H

#include <stdint.h>

void monedas_iniciar(void);       // PG0–PG2 como entradas y toma la primera foto
void monedas_rearmar(void);       // D4: "foto" de los switches; solo cuenta un cambio posterior
uint16_t monedas_leer(void);      // 100, 200 o 500 si acaba de entrar una moneda; si no, 0
uint8_t monedas_crudo(void);      // PING & 0b00000111 sin invertir (para la prueba de nivel activo)

#endif
