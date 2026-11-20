// =====================================================================
// saldo.h — Formato del saldo en la tarjeta y reglas de cobro/recarga. LÓGICA PURA.
// Solo <stdint.h>, <stdbool.h> y config.h: compila igual en el Mega y en el PC.
//
// Bloque 4 de la tarjeta (16 bytes), D7:
//   bytes 0–3  : saldo (uint32_t, little-endian: el byte menos significativo primero)
//   bytes 4–7  : ~saldo (complemento a uno, también little-endian)
//   bytes 8–15 : 0
// =====================================================================
#ifndef SALDO_H
#define SALDO_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define SALDO_TAM_BLOQUE 16

typedef enum {
  BLOQUE_VALIDO,      // saldo y complemento coinciden, saldo <= SALDO_MAXIMO, resto en 0
  BLOQUE_EN_BLANCO,   // los 16 bytes en 0: tarjeta nueva
  BLOQUE_NO_VALIDO    // dañado, escritura cortada o formato ajeno
} EstadoBloque;

typedef enum {
  RECARGA_OK,
  RECARGA_MONTO_NO_VALIDO,   // monto = 0 o no es múltiplo de 100 (D10)
  RECARGA_SUPERA_TOPE        // saldo + monto > SALDO_MAXIMO (D10)
} ResultadoRecarga;

// Arma los 16 bytes del bloque a partir del saldo.
void saldo_codificar(uint32_t saldo, uint8_t bloque[SALDO_TAM_BLOQUE]);

// Lee los 16 bytes. Si es BLOQUE_VALIDO, deja el saldo en *saldo; en los otros casos deja 0.
EstadoBloque saldo_decodificar(const uint8_t bloque[SALDO_TAM_BLOQUE], uint32_t *saldo);

// R8: true y *nuevo = saldo - total si alcanza; false (rechazo, *nuevo = saldo) si no.
bool saldo_cobrar(uint32_t saldo, uint32_t total, uint32_t *nuevo);

// R9/D10: si es RECARGA_OK, *nuevo = saldo + monto; si no, *nuevo = saldo (no cambia).
ResultadoRecarga saldo_recargar(uint32_t saldo, uint32_t monto, uint32_t *nuevo);

#endif
