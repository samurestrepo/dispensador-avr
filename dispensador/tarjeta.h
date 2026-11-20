// =====================================================================
// tarjeta.h — Lector RFID RC522 con la librería MFRC522 (único módulo con librería)
// El saldo vive en el bloque BLOQUE_SALDO (4) de una MIFARE Classic 1K (D6),
// con el formato de saldo.h (D7).
//
// Uso de una operación con la tarjeta (consulta, pago o recarga):
//   if (tarjeta_presente()) {
//     r = tarjeta_leer_saldo(&saldo);          // y, si hay que cobrar o recargar:
//     r = tarjeta_escribir_saldo(nuevo);       // escribe, relee y compara (D8)
//     tarjeta_terminar();                      // SIEMPRE al final, salga bien o mal
//   }
// tarjeta_terminar() no va dentro de leer/escribir: después de PICC_HaltA() la
// tarjeta queda dormida (HALT) y ya no se podría escribir lo que se acaba de leer.
// =====================================================================
#ifndef TARJETA_H
#define TARJETA_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  TARJETA_OK,                     // leído/escrito y verificado
  TARJETA_EN_BLANCO,              // solo al leer: bloque en 0 (tarjeta nueva), saldo = 0
  TARJETA_NO_VALIDA,              // solo al leer: el bloque no tiene un saldo válido (D7)
  TARJETA_NO_SOPORTADA,           // no es MIFARE Classic 1K (D6)
  TARJETA_ERROR_AUTENTICACION,    // la clave A de fábrica no abrió el sector
  TARJETA_ERROR_LECTURA,          // falló MIFARE_Read (p. ej. se retiró la tarjeta)
  TARJETA_ERROR_ESCRITURA,        // falló MIFARE_Write
  TARJETA_ERROR_VERIFICACION,     // se escribió, pero lo releído no coincide (D8)
  TARJETA_SALDO_FUERA_DE_RANGO    // se pidió escribir un saldo > SALDO_MAXIMO: no se escribe
} ResultadoTarjeta;

void tarjeta_iniciar(void);              // SPI y RC522; va en setup()
bool tarjeta_presente(void);             // true si hay una tarjeta NUEVA y se leyó su UID
ResultadoTarjeta tarjeta_leer_saldo(uint32_t *saldo);        // *saldo = 0 si no es TARJETA_OK
ResultadoTarjeta tarjeta_escribir_saldo(uint32_t saldo);
void tarjeta_terminar(void);             // PICC_HaltA() + PCD_StopCrypto1()

// Para la prueba de hardware (MODO 5): datos de la última tarjeta detectada.
uint8_t tarjeta_uid(uint8_t destino[10]);   // copia el UID y devuelve cuántos bytes tiene (4, 7 o 10)
const char *tarjeta_tipo_texto(void);       // "1K", "4K", "Mini", "UL", "Plus", ... o "?"

#endif
