// =====================================================================
// tarjeta.cpp — RC522 con la librería MFRC522 (GithubCommunity 1.4.12) + SPI.h
// Es el ÚNICO módulo que incluye MFRC522.h y SPI.h (D1). No escribe registros
// del ATmega2560: el SPI y los pines SS/RST los maneja la librería.
//
// ¿Por qué se acepta que estas llamadas bloqueen, si D2 pide no bloquear?
// Datos tomados del código fuente de MFRC522.cpp 1.4.12 (los tiempos con
// tarjeta son ESTIMADOS, no medidos en la placa):
//  - PCD_Init() configura el temporizador interno del RC522 en 25 ms
//    (TPrescaler 0xA9 -> 25 us por cuenta, TReload 1000). Toda comunicación
//    con la tarjeta espera como máximo ese tiempo; además el código se rinde
//    a los 36 ms (PCD_CommunicateWithPICC) o 89 ms (PCD_CalculateCRC) si el
//    RC522 no contesta.
//  - SIN tarjeta, PICC_IsNewCardPresent() espera ese tiempo límite: ~25 ms
//    por llamada. Por eso solo se llama en los estados que esperan tarjeta
//    (PAGO_TARJETA, RECARGA_TARJETA, CONSULTA_SALDO); allí el teclado igual
//    se lee cada ~25 ms (el antirrebote ya es de 20 ms) y las monedas se
//    ignoran (D5). El LCD no se congela de forma visible.
//  - CON tarjeta, cada intercambio dura pocos ms (SPI a 4 MHz, según
//    MFRC522_SPICLOCK); leer el saldo o escribir+releer se estima en
//    decenas de ms como máximo: una sola vez por operación, no en cada vuelta.
//  - PCD_Init() espera 50 ms o más (delay(50) en el reinicio), pero solo en setup().
// Si se quiere el dato real, se mide con millis() antes y después en el MODO 5.
// =====================================================================
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"
#include "saldo.h"
#include "tarjeta.h"

static MFRC522 lector(TARJETA_PIN_SS, TARJETA_PIN_RST);
static MFRC522::MIFARE_Key clave_a;       // clave A de fábrica: FF FF FF FF FF FF (D6)

// MIFARE_Read entrega 16 bytes de datos + 2 de CRC_A: el buffer debe ser de 18
// (la librería devuelve STATUS_NO_ROOM si es menor).
#define TAM_BUFFER_LECTURA 18

void tarjeta_iniciar(void) {
  SPI.begin();                    // la librería NO lo llama: hay que hacerlo antes de PCD_Init
  lector.PCD_Init();              // reinicia el RC522, configura su temporizador y enciende la antena
  for (uint8_t i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
    clave_a.keyByte[i] = 0xFF;
  }
}

// PICC_IsNewCardPresent: ¿alguna tarjeta respondió a REQA? (una tarjeta en HALT no responde)
// PICC_ReadCardSerial:   anticolisión y selección; deja el UID y el SAK en lector.uid
bool tarjeta_presente(void) {
  if (!lector.PICC_IsNewCardPresent()) {
    return false;
  }
  return lector.PICC_ReadCardSerial();
}

// Para VERIFICAR_TARJETA (D8). Tras un fallo, la tarjeta pudo quedar en el campo
// dormida (HALT, por tarjeta_terminar()). PICC_IsNewCardPresent() envía REQA, al
// que una tarjeta en HALT NO responde; PICC_WakeupA() envía WUPA (0x52), que según
// MFRC522.h despierta tarjetas en IDLE y en HALT. Así se vuelve a leer la misma
// tarjeta sin pedir que la retiren. Se acepta STATUS_COLLISION igual que
// PICC_IsNewCardPresent() (verificado en MFRC522.cpp 1.4.12). Esa función además
// reescribe TxModeReg, RxModeReg y ModWidthReg del RC522 antes del REQA; aquí no
// hace falta porque en MFRC522.cpp solo PCD_Init y ella los escriben, siempre
// con los mismos valores.
bool tarjeta_despertar(void) {
  byte atqa[2];                    // respuesta de la tarjeta a WUPA (2 bytes)
  byte tam = sizeof(atqa);
  MFRC522::StatusCode estado = lector.PICC_WakeupA(atqa, &tam);
  if (estado != MFRC522::STATUS_OK && estado != MFRC522::STATUS_COLLISION) {
    return false;
  }
  return lector.PICC_ReadCardSerial();
}

// D6: solo se acepta MIFARE Classic 1K. El tipo se deduce del byte SAK de la selección.
static bool es_mifare_1k(void) {
  return MFRC522::PICC_GetType(lector.uid.sak) == MFRC522::PICC_TYPE_MIFARE_1K;
}

// Abre el sector del bloque de saldo con la clave A. Se autentica el mismo bloque 4.
static bool autenticar(void) {
  MFRC522::StatusCode estado = lector.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, BLOQUE_SALDO, &clave_a, &lector.uid);
  return estado == MFRC522::STATUS_OK;
}

// Lee el bloque de saldo en buffer[0..15]. true si salió bien.
static bool leer_bloque(uint8_t buffer[TAM_BUFFER_LECTURA]) {
  uint8_t tam = TAM_BUFFER_LECTURA;      // entrada: tamaño del buffer; salida: bytes recibidos
  MFRC522::StatusCode estado = lector.MIFARE_Read(BLOQUE_SALDO, buffer, &tam);
  return estado == MFRC522::STATUS_OK && tam >= SALDO_TAM_BLOQUE;
}

ResultadoTarjeta tarjeta_leer_saldo(uint32_t *saldo) {
  *saldo = 0;
  if (!es_mifare_1k()) {
    return TARJETA_NO_SOPORTADA;
  }
  if (!autenticar()) {
    return TARJETA_ERROR_AUTENTICACION;
  }
  uint8_t buffer[TAM_BUFFER_LECTURA];
  if (!leer_bloque(buffer)) {
    return TARJETA_ERROR_LECTURA;
  }
  EstadoBloque estado = saldo_decodificar(buffer, saldo);   // usa solo los bytes 0–15
  if (estado == BLOQUE_EN_BLANCO) {
    return TARJETA_EN_BLANCO;
  }
  if (estado == BLOQUE_NO_VALIDO) {
    return TARJETA_NO_VALIDA;
  }
  return TARJETA_OK;
}

// D8: escribir, RELEER y comparar byte a byte. Solo si coincide se devuelve OK
// (y solo entonces la FSM dispensa). Se vuelve a autenticar aunque ya se haya
// leído: así la función sirve sola y es lo mismo que hace el ejemplo
// ReadAndWrite de la librería entre su lectura y su escritura.
ResultadoTarjeta tarjeta_escribir_saldo(uint32_t saldo) {
  if (saldo > SALDO_MAXIMO) {
    return TARJETA_SALDO_FUERA_DE_RANGO;
  }
  if (!es_mifare_1k()) {
    return TARJETA_NO_SOPORTADA;
  }
  if (!autenticar()) {
    return TARJETA_ERROR_AUTENTICACION;
  }

  uint8_t bloque[SALDO_TAM_BLOQUE];
  saldo_codificar(saldo, bloque);
  if (lector.MIFARE_Write(BLOQUE_SALDO, bloque, SALDO_TAM_BLOQUE) != MFRC522::STATUS_OK) {
    return TARJETA_ERROR_ESCRITURA;
  }

  uint8_t releido[TAM_BUFFER_LECTURA];
  if (!leer_bloque(releido)) {
    return TARJETA_ERROR_VERIFICACION;     // no se pudo comprobar: se trata como fallo
  }
  for (uint8_t i = 0; i < SALDO_TAM_BLOQUE; i++) {
    if (releido[i] != bloque[i]) {
      return TARJETA_ERROR_VERIFICACION;
    }
  }
  return TARJETA_OK;
}

// Cierra la operación: PICC_HaltA() duerme la tarjeta (no vuelve a detectarse
// hasta que se retire y se acerque de nuevo: no se cobra dos veces por dejarla
// apoyada) y PCD_StopCrypto1() apaga el cifrado que abrió la autenticación.
void tarjeta_terminar(void) {
  lector.PICC_HaltA();
  lector.PCD_StopCrypto1();
}

uint8_t tarjeta_uid(uint8_t destino[10]) {
  uint8_t tam = lector.uid.size;
  if (tam > 10) {
    tam = 10;
  }
  for (uint8_t i = 0; i < tam; i++) {
    destino[i] = lector.uid.uidByte[i];
  }
  return tam;
}

// Nombres cortos propios (PICC_GetTypeName de la librería devuelve texto en
// flash, largo para el LCD).
const char *tarjeta_tipo_texto(void) {
  switch (MFRC522::PICC_GetType(lector.uid.sak)) {
    case MFRC522::PICC_TYPE_MIFARE_1K:       return "1K";
    case MFRC522::PICC_TYPE_MIFARE_4K:       return "4K";
    case MFRC522::PICC_TYPE_MIFARE_MINI:     return "Mini";
    case MFRC522::PICC_TYPE_MIFARE_UL:       return "UL";
    case MFRC522::PICC_TYPE_MIFARE_PLUS:     return "Plus";
    case MFRC522::PICC_TYPE_ISO_14443_4:     return "14443";
    default:                                 return "?";
  }
}
