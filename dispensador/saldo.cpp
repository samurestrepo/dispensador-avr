// =====================================================================
// saldo.cpp — Bloque de saldo de la tarjeta (D6, D7) y reglas de cobro/recarga
// LÓGICA PURA: no lee ni escribe la tarjeta (eso es tarjeta.cpp, fase 3).
//
// Nota sobre los desplazamientos (>> y <<) de este archivo:
// NO son máscaras de registro (esas van en 0b..., convención de la profesora).
// Aquí sirven para partir un número de 32 bits en sus 4 bytes y volver a unirlo:
//   saldo >> 8  deja el byte 1 en la posición del byte 0, y el cast (uint8_t)
//   se queda solo con esos 8 bits. Al revés, (uint32_t)b << 8 lo devuelve a su lugar.
// =====================================================================
#include "saldo.h"

// Escribe un uint32_t en 4 bytes, del menos significativo al más (little-endian).
// Ejemplo: 1700 = 0x000006A4 -> A4 06 00 00
static void escribir_uint32(uint32_t valor, uint8_t destino[4]) {
  destino[0] = (uint8_t)(valor);          // bits 0–7
  destino[1] = (uint8_t)(valor >> 8);     // bits 8–15
  destino[2] = (uint8_t)(valor >> 16);    // bits 16–23
  destino[3] = (uint8_t)(valor >> 24);    // bits 24–31
}

// Une 4 bytes little-endian en un uint32_t (lo contrario de escribir_uint32).
static uint32_t leer_uint32(const uint8_t origen[4]) {
  uint32_t valor = (uint32_t)origen[0];
  valor = valor | ((uint32_t)origen[1] << 8);
  valor = valor | ((uint32_t)origen[2] << 16);
  valor = valor | ((uint32_t)origen[3] << 24);
  return valor;
}

// D7: saldo, su complemento y el resto en 0.
void saldo_codificar(uint32_t saldo, uint8_t bloque[SALDO_TAM_BLOQUE]) {
  escribir_uint32(saldo, &bloque[0]);          // bytes 0–3
  escribir_uint32(~saldo, &bloque[4]);         // bytes 4–7: complemento a uno
  for (uint8_t i = 8; i < SALDO_TAM_BLOQUE; i++) {
    bloque[i] = 0;                             // bytes 8–15
  }
}

// D7: si saldo y complemento no coinciden, la tarjeta está dañada o la escritura se cortó.
EstadoBloque saldo_decodificar(const uint8_t bloque[SALDO_TAM_BLOQUE], uint32_t *saldo) {
  *saldo = 0;

  // 1. ¿Tarjeta nueva? Los 16 bytes en 0.
  //    (Un saldo de $0 válido NO es en blanco: su complemento es FF FF FF FF.)
  bool todo_cero = true;
  for (uint8_t i = 0; i < SALDO_TAM_BLOQUE; i++) {
    if (bloque[i] != 0) {
      todo_cero = false;
    }
  }
  if (todo_cero) {
    return BLOQUE_EN_BLANCO;
  }

  // 2. Los bytes 8–15 deben estar en 0 (si no, el bloque no tiene nuestro formato).
  for (uint8_t i = 8; i < SALDO_TAM_BLOQUE; i++) {
    if (bloque[i] != 0) {
      return BLOQUE_NO_VALIDO;
    }
  }

  // 3. El complemento debe coincidir.
  uint32_t valor = leer_uint32(&bloque[0]);
  uint32_t complemento = leer_uint32(&bloque[4]);
  if (valor != ~complemento) {
    return BLOQUE_NO_VALIDO;
  }

  // 4. D10: nunca puede haber más que el tope.
  if (valor > SALDO_MAXIMO) {
    return BLOQUE_NO_VALIDO;
  }

  *saldo = valor;
  return BLOQUE_VALIDO;
}

// R8: si el saldo no alcanza, se rechaza y el saldo no cambia.
bool saldo_cobrar(uint32_t saldo, uint32_t total, uint32_t *nuevo) {
  if (saldo < total) {
    *nuevo = saldo;
    return false;
  }
  *nuevo = saldo - total;
  return true;
}

// R9 + D10: monto > 0, múltiplo de 100, y el resultado no pasa de SALDO_MAXIMO.
ResultadoRecarga saldo_recargar(uint32_t saldo, uint32_t monto, uint32_t *nuevo) {
  *nuevo = saldo;
  if (monto == 0 || monto % 100 != 0) {
    return RECARGA_MONTO_NO_VALIDO;
  }
  // Se compara "monto > tope - saldo" en lugar de "saldo + monto > tope"
  // para que la suma nunca se desborde. Primero se descarta saldo > tope,
  // porque entonces la resta daría un número enorme.
  if (saldo > SALDO_MAXIMO || monto > SALDO_MAXIMO - saldo) {
    return RECARGA_SUPERA_TOPE;
  }
  *nuevo = saldo + monto;
  return RECARGA_OK;
}
