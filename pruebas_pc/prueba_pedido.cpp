// =====================================================================
// prueba_pedido.cpp — Pruebas de pedido.cpp en el PC (sin hardware)
// Se compila con:  g++ -std=c++17 -Wall -Wextra -I../dispensador prueba_pedido.cpp ../dispensador/pedido.cpp
// =====================================================================
#include "mini_prueba.h"
#include "pedido.h"

// Arma un pedido de "agua" aguas y "gaseosa" gaseosas, empezando de cero.
static void armar(uint8_t agua, uint8_t gaseosa) {
  pedido_reiniciar();
  for (uint8_t i = 0; i < agua; i++) {
    pedido_agregar(PRODUCTO_AGUA);
  }
  for (uint8_t i = 0; i < gaseosa; i++) {
    pedido_agregar(PRODUCTO_GASEOSA);
  }
}

static void prueba_inicio_en_cero(void) {
  caso("pedido nuevo: todo en 0");
  pedido_reiniciar();
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 0);
  VERIFICAR(pedido_cantidad(PRODUCTO_GASEOSA) == 0);
  VERIFICAR(pedido_total() == 0);
  VERIFICAR(pago_pagado() == 0);
}

static void prueba_agregar_quitar(void) {
  caso("agregar y quitar (R5: cancelar = quitar una unidad)");
  pedido_reiniciar();
  VERIFICAR(pedido_agregar(PRODUCTO_AGUA) == true);
  VERIFICAR(pedido_agregar(PRODUCTO_AGUA) == true);
  VERIFICAR(pedido_agregar(PRODUCTO_GASEOSA) == true);
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 2);
  VERIFICAR(pedido_cantidad(PRODUCTO_GASEOSA) == 1);
  VERIFICAR(pedido_quitar(PRODUCTO_AGUA) == true);
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 1);
  VERIFICAR(pedido_cantidad(PRODUCTO_GASEOSA) == 1);   // quitar agua no toca la gaseosa
}

static void prueba_maximo(void) {
  caso("maximo 9 por producto (D11)");
  armar(MAX_UNIDADES, 0);
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 9);
  VERIFICAR(pedido_agregar(PRODUCTO_AGUA) == false);    // el 10.o se rechaza
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 9);       // y no cambia nada
  VERIFICAR(pedido_agregar(PRODUCTO_GASEOSA) == true);  // el otro producto sigue libre
  VERIFICAR(pedido_quitar(PRODUCTO_AGUA) == true);      // desde 9 se puede quitar
  VERIFICAR(pedido_agregar(PRODUCTO_AGUA) == true);     // y volver a 9
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 9);
}

static void prueba_nunca_negativo(void) {
  caso("nunca negativo");
  pedido_reiniciar();
  VERIFICAR(pedido_quitar(PRODUCTO_AGUA) == false);
  VERIFICAR(pedido_quitar(PRODUCTO_GASEOSA) == false);
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 0);       // no dio la vuelta a 255
  VERIFICAR(pedido_cantidad(PRODUCTO_GASEOSA) == 0);
  armar(1, 0);
  VERIFICAR(pedido_quitar(PRODUCTO_AGUA) == true);
  VERIFICAR(pedido_quitar(PRODUCTO_AGUA) == false);
  VERIFICAR(pedido_cantidad(PRODUCTO_AGUA) == 0);
}

static void prueba_producto_invalido(void) {
  caso("producto que no existe");
  pedido_reiniciar();
  VERIFICAR(pedido_agregar(2) == false);
  VERIFICAR(pedido_quitar(2) == false);
  VERIFICAR(pedido_cantidad(2) == 0);
  VERIFICAR(pedido_subtotal(2) == 0);
  VERIFICAR(pedido_total() == 0);
}

static void prueba_subtotal_y_total(void) {
  caso("subtotales y total (R4)");
  armar(2, 1);                                          // el ejemplo de diseno/05
  VERIFICAR(pedido_subtotal(PRODUCTO_AGUA) == 1000);
  VERIFICAR(pedido_subtotal(PRODUCTO_GASEOSA) == 700);
  VERIFICAR(pedido_total() == 1700);
  armar(9, 9);                                          // el maximo posible (D11)
  VERIFICAR(pedido_total() == 10800);
}

static void prueba_reiniciar(void) {
  caso("reiniciar borra cantidades y pagado");
  armar(3, 2);
  pago_insertar_moneda(500);
  pedido_reiniciar();
  VERIFICAR(pedido_total() == 0);
  VERIFICAR(pago_pagado() == 0);
}

static void prueba_pago_exacto(void) {
  caso("pago exacto: 1700 = 500 + 500 + 500 + 200");
  armar(2, 1);
  pago_reiniciar();
  VERIFICAR(pago_insertar_moneda(500) == PAGO_ACEPTADA);
  VERIFICAR(pago_falta() == 1200);
  VERIFICAR(pago_insertar_moneda(500) == PAGO_ACEPTADA);
  VERIFICAR(pago_insertar_moneda(500) == PAGO_ACEPTADA);
  VERIFICAR(pago_pagado() == 1500);
  VERIFICAR(pago_insertar_moneda(200) == PAGO_COMPLETO);
  VERIFICAR(pago_pagado() == 1700);
  VERIFICAR(pago_falta() == 0);
}

static void prueba_una_moneda_completa(void) {
  caso("una sola moneda completa el total (1 agua = 500)");
  armar(1, 0);
  pago_reiniciar();
  VERIFICAR(pago_insertar_moneda(500) == PAGO_COMPLETO);
}

static void prueba_moneda_que_se_pasa(void) {
  caso("moneda que se pasa (R7) y completar el restante");
  armar(0, 1);                                          // total 700
  pago_reiniciar();
  VERIFICAR(pago_insertar_moneda(500) == PAGO_ACEPTADA);  // pagado 500, falta 200
  VERIFICAR(pago_insertar_moneda(500) == PAGO_DEVUELTA);  // 1000 > 700: se devuelve
  VERIFICAR(pago_pagado() == 500);                        // no se sumo
  VERIFICAR(pago_falta() == 200);                         // "Ingrese $200"
  VERIFICAR(pago_insertar_moneda(200) == PAGO_COMPLETO);  // completa con el restante
  VERIFICAR(pago_pagado() == 700);
}

static void prueba_devuelta_y_luego_varias(void) {
  caso("despues de una devuelta, completar con monedas menores");
  armar(0, 1);                                          // total 700
  pago_reiniciar();
  VERIFICAR(pago_insertar_moneda(500) == PAGO_ACEPTADA);
  VERIFICAR(pago_insertar_moneda(500) == PAGO_DEVUELTA);
  VERIFICAR(pago_insertar_moneda(100) == PAGO_ACEPTADA);
  VERIFICAR(pago_insertar_moneda(200) == PAGO_DEVUELTA);  // 800 > 700
  VERIFICAR(pago_insertar_moneda(100) == PAGO_COMPLETO);
}

static void prueba_moneda_despues_de_completo(void) {
  caso("una moneda despues de completar se devuelve");
  armar(1, 0);
  pago_reiniciar();
  VERIFICAR(pago_insertar_moneda(500) == PAGO_COMPLETO);
  VERIFICAR(pago_insertar_moneda(100) == PAGO_DEVUELTA);
  VERIFICAR(pago_pagado() == 500);
}

static void prueba_moneda_invalida(void) {
  caso("moneda que no es 100/200/500 se devuelve");
  armar(2, 1);
  pago_reiniciar();
  VERIFICAR(pago_insertar_moneda(0) == PAGO_DEVUELTA);
  VERIFICAR(pago_insertar_moneda(50) == PAGO_DEVUELTA);
  VERIFICAR(pago_insertar_moneda(1000) == PAGO_DEVUELTA);
  VERIFICAR(pago_pagado() == 0);
}

static void prueba_pedido_vacio(void) {
  caso("pedido vacio: cualquier moneda se devuelve");
  pedido_reiniciar();
  VERIFICAR(pago_insertar_moneda(100) == PAGO_DEVUELTA);
  VERIFICAR(pago_pagado() == 0);
  VERIFICAR(pago_falta() == 0);
}

static void prueba_pago_maximo(void) {
  caso("pedido maximo 10800 con monedas de 500 y 100 (sin desbordes)");
  armar(9, 9);
  pago_reiniciar();
  for (int i = 0; i < 21; i++) {                        // 21 x 500 = 10500
    VERIFICAR(pago_insertar_moneda(500) == PAGO_ACEPTADA);
  }
  VERIFICAR(pago_insertar_moneda(500) == PAGO_DEVUELTA);  // 11000 > 10800
  VERIFICAR(pago_insertar_moneda(100) == PAGO_ACEPTADA);
  VERIFICAR(pago_insertar_moneda(200) == PAGO_COMPLETO);
  VERIFICAR(pago_pagado() == 10800);
}

static void prueba_pago_reiniciar(void) {
  caso("pago_reiniciar (cancelar con *, D13) deja el pedido intacto");
  armar(2, 1);
  pago_reiniciar();
  pago_insertar_moneda(500);
  VERIFICAR(pago_pagado() == 500);                      // lo que la FSM muestra como devuelto
  pago_reiniciar();
  VERIFICAR(pago_pagado() == 0);
  VERIFICAR(pedido_total() == 1700);
}

int main() {
  printf("=== prueba_pedido ===\n");
  prueba_inicio_en_cero();
  prueba_agregar_quitar();
  prueba_maximo();
  prueba_nunca_negativo();
  prueba_producto_invalido();
  prueba_subtotal_y_total();
  prueba_reiniciar();
  prueba_pago_exacto();
  prueba_una_moneda_completa();
  prueba_moneda_que_se_pasa();
  prueba_devuelta_y_luego_varias();
  prueba_moneda_despues_de_completo();
  prueba_moneda_invalida();
  prueba_pedido_vacio();
  prueba_pago_maximo();
  prueba_pago_reiniciar();
  return resumen("prueba_pedido");
}
