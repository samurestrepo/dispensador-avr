// =====================================================================
// pedido.cpp — Pedido y pago con monedas. LÓGICA PURA (sin hardware).
// =====================================================================
#include "pedido.h"

// Estado del módulo ("static": nadie fuera de este archivo lo toca directamente).
static uint8_t cantidad[2] = {0, 0};    // [PRODUCTO_AGUA], [PRODUCTO_GASEOSA]
static uint16_t pagado = 0;

// Precio de un producto; 0 si el número de producto no existe.
static uint16_t precio(uint8_t producto) {
  if (producto == PRODUCTO_AGUA) {
    return PRECIO_AGUA;
  }
  if (producto == PRODUCTO_GASEOSA) {
    return PRECIO_GASEOSA;
  }
  return 0;
}

static bool producto_valido(uint8_t producto) {
  return producto == PRODUCTO_AGUA || producto == PRODUCTO_GASEOSA;
}

// ======================= Pedido =======================

void pedido_reiniciar(void) {
  cantidad[PRODUCTO_AGUA] = 0;
  cantidad[PRODUCTO_GASEOSA] = 0;
  pagado = 0;
}

// D11: máximo MAX_UNIDADES (9) por producto, para que quepa en una cifra.
bool pedido_agregar(uint8_t producto) {
  if (!producto_valido(producto)) {
    return false;
  }
  if (cantidad[producto] >= MAX_UNIDADES) {
    return false;                   // ya está en el máximo: no cambia nada
  }
  cantidad[producto]++;
  return true;
}

// R5: cancelar = quitar UNA unidad. Nunca baja de 0.
bool pedido_quitar(uint8_t producto) {
  if (!producto_valido(producto)) {
    return false;
  }
  if (cantidad[producto] == 0) {
    return false;                   // ya está en 0: no cambia nada
  }
  cantidad[producto]--;
  return true;
}

uint8_t pedido_cantidad(uint8_t producto) {
  if (!producto_valido(producto)) {
    return 0;
  }
  return cantidad[producto];
}

// R4: costo de cada producto.
uint16_t pedido_subtotal(uint8_t producto) {
  return (uint16_t)(pedido_cantidad(producto) * precio(producto));
}

// R3/R4: un solo total para todo el pedido. Máximo 9*500 + 9*700 = 10 800 (cabe en uint16_t).
uint16_t pedido_total(void) {
  return (uint16_t)(pedido_subtotal(PRODUCTO_AGUA) + pedido_subtotal(PRODUCTO_GASEOSA));
}

// ======================= Pago con monedas =======================

void pago_reiniciar(void) {
  pagado = 0;
}

// R6 + R7 (regla de diseño/05):
//   si (pagado + moneda > total):  devolver moneda (no se suma)
//   si no:                         pagado += moneda
//   si (pagado == total):          pago completo
// Una moneda que no sea 100, 200 o 500 también se devuelve (no debería llegar nunca).
ResultadoPago pago_insertar_moneda(uint16_t moneda) {
  if (moneda != MONEDA_1_VALOR && moneda != MONEDA_2_VALOR && moneda != MONEDA_3_VALOR) {
    return PAGO_DEVUELTA;
  }
  uint16_t total = pedido_total();
  if (pagado + moneda > total) {
    return PAGO_DEVUELTA;           // se pasaba: la FSM muestra "Ingrese $" + pago_falta()
  }
  pagado = (uint16_t)(pagado + moneda);
  if (pagado == total) {
    return PAGO_COMPLETO;
  }
  return PAGO_ACEPTADA;
}

uint16_t pago_pagado(void) {
  return pagado;
}

uint16_t pago_falta(void) {
  uint16_t total = pedido_total();
  if (pagado >= total) {
    return 0;                       // por si acaso: nunca un número "negativo"
  }
  return (uint16_t)(total - pagado);
}
