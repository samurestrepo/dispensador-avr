// =====================================================================
// pedido.h — Pedido y pago con monedas. LÓGICA PURA.
// Solo <stdint.h>, <stdbool.h> y config.h: compila igual en el Mega y en el PC.
// El estado (cantidades y pagado) vive dentro de pedido.cpp; desde fuera
// solo se usa a través de estas funciones.
// =====================================================================
#ifndef PEDIDO_H
#define PEDIDO_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// ------------------------- Pedido -------------------------
void pedido_reiniciar(void);                  // cantidades en 0 y pagado en 0
bool pedido_agregar(uint8_t producto);        // +1 unidad; false si ya hay MAX_UNIDADES o el producto no existe
bool pedido_quitar(uint8_t producto);         // -1 unidad; false si ya está en 0 o el producto no existe
uint8_t pedido_cantidad(uint8_t producto);    // unidades de ese producto (0 si no existe)
uint16_t pedido_subtotal(uint8_t producto);   // cantidad x precio
uint16_t pedido_total(void);                  // subtotal agua + subtotal gaseosa (máx. 10 800)

// ------------------------- Pago con monedas (R7) -------------------------
typedef enum {
  PAGO_ACEPTADA,   // la moneda se sumó y todavía falta dinero
  PAGO_DEVUELTA,   // la moneda se pasaba del total (o no es 100/200/500): no se suma
  PAGO_COMPLETO    // la moneda se sumó y pagado == total
} ResultadoPago;

void pago_reiniciar(void);                         // pagado = 0 (al entrar a PAGO_MONEDAS)
ResultadoPago pago_insertar_moneda(uint16_t moneda);
uint16_t pago_pagado(void);                        // lo que lleva ingresado
uint16_t pago_falta(void);                         // total - pagado

#endif
