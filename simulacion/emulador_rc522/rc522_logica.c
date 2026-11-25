// =====================================================================
// rc522_logica.c — Emula el MFRC522 visto desde su bus SPI + una tarjeta
// MIFARE Classic 1K en el campo. SOLO PARA LA SIMULACIÓN EN PROTEUS.
//
// Qué recibe el Mega del RC522 real: solo bytes por MISO, en tramas SPI:
//   SS en bajo · byte de dirección (bit 7 = 1 lectura, bits 6:1 registro) ·
//   datos · SS en alto.
// Aquí se guarda un banco de 64 registros y se reproduce lo que el chip haría
// al ejecutar los comandos que usa la librería MFRC522 1.4.12:
//   CalcCRC, Transceive, MFAuthent, SoftReset, Idle.
// La tarjeta no se cifra (Crypto1 ocurre dentro del RC522 real; al Mega solo
// le llega el bit MFCrypto1On de Status2Reg).
// =====================================================================
#include "rc522_logica.h"

// En el AVR la parte rápida se mete dentro de la ISR (menos ciclos hasta cargar SPDR)
#ifdef __AVR__
#define RAPIDA __attribute__((always_inline)) inline
#else
#define RAPIDA
#endif

// --- Registros (dirección real del MFRC522) ---
#define CommandReg     0x01
#define ComIrqReg      0x04
#define DivIrqReg      0x05
#define ErrorReg       0x06
#define Status2Reg     0x08
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define ControlReg     0x0C
#define BitFramingReg  0x0D
#define CollReg        0x0E
#define CRCResultRegH  0x21
#define CRCResultRegL  0x22
#define VersionReg     0x37

#define CMD_IDLE        0x00
#define CMD_CALCCRC     0x03
#define CMD_TRANSCEIVE  0x0C
#define CMD_MFAUTHENT   0x0E
#define CMD_SOFTRESET   0x0F

#define FIFO_TAM 64

static volatile uint8_t reg[64];
static volatile uint8_t fifo[FIFO_TAM];
static volatile uint8_t fifo_n;          // bytes en la FIFO
static volatile uint8_t fifo_lect;       // índice de lectura

// Estado de la trama SPI
static uint8_t t_idx, t_dir, t_lectura;   // solo los tocan las ISR

// Trabajo pendiente (lo ejecuta emu_procesar fuera de la ISR)
static volatile uint8_t pend_comando;    // 0 = nada
static volatile uint8_t pend_enviar;     // Transceive con StartSend

// --- Tarjetas ---
// A: UID DE AD BE EF, saldo inicial $5.000 (formato de saldo.cpp: saldo LE + complemento + 0)
// B: UID 12 34 56 78, bloque 4 en blanco (tarjeta nueva)
static const uint8_t uid_tarjeta[2][4] = { {0xDE, 0xAD, 0xBE, 0xEF}, {0x12, 0x34, 0x56, 0x78} };
static uint8_t bloque4[2][16] = {
  {0x88, 0x13, 0x00, 0x00, 0x77, 0xEC, 0xFF, 0xFF, 0,0,0,0,0,0,0,0},  // 5000 = 0x00001388
  {0}
};

enum { T_FUERA, T_IDLE, T_READY, T_ACTIVA, T_AUTENTICADA, T_HALT };
static uint8_t t_estado = T_FUERA;
static uint8_t t_actual = 0;             // 0 = A, 1 = B
static uint8_t esperando_datos_escritura;
static uint8_t in_falla, in_guarda;
static uint8_t ev_cola[8], ev_ini, ev_fin;   // eventos para el registro (se leen en el bucle)
static void evento_poner(uint8_t e) { uint8_t s = (uint8_t)((ev_fin + 1) & 7); if (s != ev_ini) { ev_cola[ev_fin] = e; ev_fin = s; } }

// ---------------------------------------------------------------------
static uint16_t crc_a(const uint8_t *d, uint8_t n) {
  uint16_t crc = 0x6363;                 // ModeReg = 0x3D -> preset 6363h (ISO 14443-3)
  for (uint8_t i = 0; i < n; i++) {
    uint8_t ch = d[i] ^ (uint8_t)crc;
    ch ^= (uint8_t)(ch << 4);
    crc = (crc >> 8) ^ ((uint16_t)ch << 8) ^ ((uint16_t)ch << 3) ^ (ch >> 4);
  }
  return crc;
}

static void reiniciar_chip(void) {
  for (uint8_t i = 0; i < 64; i++) reg[i] = 0;
  reg[CommandReg]   = 0x20;              // RcvOff = 1 tras el reset (bit 4 PowerDown = 0)
  reg[ComIrqReg]    = 0x14;
  reg[VersionReg]   = 0x92;              // MFRC522 versión 2.0
  reg[0x14]         = 0x80;              // TxControlReg: antena apagada tras el reset
  fifo_n = 0; fifo_lect = 0;
  pend_comando = 0; pend_enviar = 0;
}

// ======================= Parte rápida (ISR del SPI) =======================
static RAPIDA uint8_t leer_registro(uint8_t r) {
  if (r == FIFODataReg) {
    if (fifo_lect < fifo_n) return fifo[fifo_lect++];
    return 0;
  }
  if (r == FIFOLevelReg) return (uint8_t)(fifo_n - fifo_lect);
  return reg[r];
}

static RAPIDA void escribir_registro(uint8_t r, uint8_t v) {
  switch (r) {
    case FIFODataReg:
      if (fifo_n < FIFO_TAM) fifo[fifo_n++] = v;
      break;
    case FIFOLevelReg:
      if (v & 0x80) { fifo_n = 0; fifo_lect = 0; }       // FlushBuffer
      break;
    case ComIrqReg:
    case DivIrqReg:                                       // bit 7 Set1/Set2: 1 = poner, 0 = borrar
      if (v & 0x80) reg[r] |= (v & 0x7F);
      else          reg[r] &= (uint8_t)~(v & 0x7F);
      break;
    case CommandReg:
      reg[CommandReg] = (reg[CommandReg] & 0xF0) | (v & 0x0F);
      if ((v & 0x0F) == CMD_IDLE) { pend_comando = 0; pend_enviar = 0; }
      else pend_comando = v & 0x0F;
      break;
    case BitFramingReg:
      reg[r] = v & 0x7F;
      if ((v & 0x80) && (reg[CommandReg] & 0x0F) == CMD_TRANSCEIVE) pend_enviar = 1;  // StartSend
      break;
    case VersionReg:
      break;                                              // solo lectura
    default:
      reg[r] = v;
  }
}

void emu_ss_bajo(void) { t_idx = 0; }
void emu_ss_alto(void) { t_idx = 0; }

RAPIDA uint8_t emu_byte(uint8_t b) {
  if (t_idx == 0) {
    t_idx = 1;
    t_dir = (b >> 1) & 0x3F;
    t_lectura = b & 0x80;
    return t_lectura ? leer_registro(t_dir) : 0;
  }
  if (t_lectura) {
    // Lectura múltiple: el maestro manda otra vez la dirección para seguir, 0 para terminar
    return b ? leer_registro(t_dir) : 0;
  }
  escribir_registro(t_dir, b);
  return 0;
}

// ======================= Parte lenta (bucle principal) =======================
static uint8_t resp[18];
static uint8_t resp_n, resp_bits;     // bits válidos del último byte (0 = 8)

// La tarjeta recibe 'tx' (n bytes, 'ult' bits válidos en el último) y contesta en resp[].
// Devuelve 1 si contesta, 0 si calla (timeout).
static uint8_t tarjeta_recibe(const uint8_t *tx, uint8_t n, uint8_t ult) {
  resp_n = 0; resp_bits = 0;
  if (t_estado == T_FUERA) return 0;
  const uint8_t *uid = uid_tarjeta[t_actual];
  uint8_t bcc = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];

  // REQA (26h) / WUPA (52h), 7 bits
  if (n == 1 && ult == 7 && (tx[0] == 0x26 || tx[0] == 0x52)) {
    if (t_estado == T_IDLE || (tx[0] == 0x52 && t_estado == T_HALT)) {
      t_estado = T_READY;
      resp[0] = 0x04; resp[1] = 0x00; resp_n = 2;        // ATQA de una MIFARE 1K
      return 1;
    }
    return 0;
  }
  if (ult != 0) return 0;

  // Anticolisión nivel 1: 93 20
  if (n == 2 && tx[0] == 0x93 && tx[1] == 0x20 && t_estado == T_READY) {
    for (uint8_t i = 0; i < 4; i++) resp[i] = uid[i];
    resp[4] = bcc; resp_n = 5;
    return 1;
  }
  // SELECT nivel 1: 93 70 UID BCC CRC
  if (n == 9 && tx[0] == 0x93 && tx[1] == 0x70 && t_estado == T_READY) {
    for (uint8_t i = 0; i < 4; i++) if (tx[2 + i] != uid[i]) return 0;
    if (tx[6] != bcc || crc_a(tx, 7) != (uint16_t)(tx[7] | (tx[8] << 8))) return 0;
    t_estado = T_ACTIVA;
    resp[0] = 0x08;                                      // SAK = MIFARE Classic 1K
    uint16_t c = crc_a(resp, 1);
    resp[1] = (uint8_t)c; resp[2] = (uint8_t)(c >> 8); resp_n = 3;
    return 1;
  }
  // Todo lo demás debe traer un CRC correcto
  if (n < 3 || crc_a(tx, n - 2) != (uint16_t)(tx[n - 2] | (tx[n - 1] << 8))) return 0;

  // HLTA: 50 00 CRC -> la tarjeta se duerme y no contesta
  if (n == 4 && tx[0] == 0x50 && tx[1] == 0x00) {
    if (t_estado != T_FUERA) { t_estado = T_HALT; evento_poner(EV_HALT); }
    esperando_datos_escritura = 0;
    return 0;
  }
  if (t_estado != T_AUTENTICADA) return 0;

  // Segunda fase de WRITE: 16 datos + CRC
  if (esperando_datos_escritura && n == 18) {
    esperando_datos_escritura = 0;
    if (in_falla) {
      if (in_guarda) for (uint8_t i = 0; i < 16; i++) bloque4[t_actual][i] = tx[i];
      evento_poner(EV_ESCRITURA_FALLIDA);
      return 0;                                          // sin ACK -> timeout en el maestro
    }
    for (uint8_t i = 0; i < 16; i++) bloque4[t_actual][i] = tx[i];
    evento_poner(EV_ESCRITURA);
    resp[0] = 0x0A; resp_n = 1; resp_bits = 4;           // ACK de 4 bits
    return 1;
  }
  // READ: 30 blk CRC -> 16 bytes + CRC
  if (n == 4 && tx[0] == 0x30) {
    for (uint8_t i = 0; i < 16; i++) resp[i] = (tx[1] == 4) ? bloque4[t_actual][i] : 0;
    uint16_t c = crc_a(resp, 16);
    resp[16] = (uint8_t)c; resp[17] = (uint8_t)(c >> 8); resp_n = 18;
    evento_poner(EV_LECTURA);
    return 1;
  }
  // WRITE fase 1: A0 blk CRC -> ACK
  if (n == 4 && tx[0] == 0xA0 && tx[1] == 4) {
    esperando_datos_escritura = 1;
    resp[0] = 0x0A; resp_n = 1; resp_bits = 4;
    return 1;
  }
  return 0;
}

static uint8_t tx_buf[FIFO_TAM];

uint8_t emu_hay_trabajo(void) {
  return pend_comando == CMD_SOFTRESET || pend_comando == CMD_CALCCRC ||
         pend_comando == CMD_MFAUTHENT || pend_enviar;
}

void emu_procesar(void) {
  uint8_t cmd = pend_comando;
  if (cmd == 0) return;

  if (cmd == CMD_SOFTRESET) {
    reiniciar_chip();
    return;
  }
  if (cmd == CMD_CALCCRC) {
    uint8_t n = fifo_n;
    for (uint8_t i = 0; i < n; i++) tx_buf[i] = fifo[i];
    uint16_t c = crc_a(tx_buf, n);
    reg[CRCResultRegL] = (uint8_t)c;
    reg[CRCResultRegH] = (uint8_t)(c >> 8);
    reg[DivIrqReg] |= 0x04;                              // CRCIRq
    pend_comando = 0;
    return;
  }
  if (cmd == CMD_MFAUTHENT) {
    // FIFO: 60/61 bloque clave[6] uid[4]
    uint8_t ok = (fifo_n == 12) && (fifo[0] == 0x60) && (t_estado == T_ACTIVA || t_estado == T_AUTENTICADA);
    for (uint8_t i = 0; ok && i < 6; i++) if (fifo[2 + i] != 0xFF) ok = 0;            // clave A de fábrica
    for (uint8_t i = 0; ok && i < 4; i++) if (fifo[8 + i] != uid_tarjeta[t_actual][i]) ok = 0;
    fifo_n = 0; fifo_lect = 0;
    if (ok) {
      t_estado = T_AUTENTICADA;
      reg[Status2Reg] |= 0x08;                           // MFCrypto1On
      reg[ErrorReg] = 0;
      reg[CommandReg] &= 0xF0;                           // el comando termina solo
      reg[ComIrqReg] |= 0x10;                            // IdleIRq
    } else {
      evento_poner(EV_AUTH_MAL);
      reg[ComIrqReg] |= 0x01;                            // TimerIRq: la tarjeta no contestó
    }
    pend_comando = 0;
    return;
  }
  if (cmd == CMD_TRANSCEIVE) {
    if (!pend_enviar) return;                            // espera StartSend
    pend_enviar = 0;
    uint8_t n = fifo_n;
    for (uint8_t i = 0; i < n; i++) tx_buf[i] = fifo[i];
    uint8_t ult = reg[BitFramingReg] & 0x07;
    fifo_n = 0; fifo_lect = 0;
    reg[ErrorReg] = 0;
    if (tarjeta_recibe(tx_buf, n, ult)) {
      for (uint8_t i = 0; i < resp_n; i++) fifo[i] = resp[i];
      fifo_n = resp_n;
      reg[ControlReg] = (reg[ControlReg] & 0xF8) | resp_bits;
      reg[ComIrqReg] |= 0x20;                            // RxIRq
    } else {
      reg[ControlReg] &= 0xF8;
      reg[ComIrqReg] |= 0x01;                            // TimerIRq (en el chip real: 25 ms después)
    }
    reg[BitFramingReg] &= 0x7F;
    // Transceive sigue activo (el chip vuelve a esperar), como en el real
    return;
  }
  pend_comando = 0;                                      // Mem, Receive, etc.: no se usan
}

void emu_entradas(uint8_t presente, uint8_t tarjeta_b, uint8_t falla, uint8_t falla_guarda) {
  in_falla = falla; in_guarda = falla_guarda;
  uint8_t cual = tarjeta_b ? 1 : 0;
  if (!presente) { t_estado = T_FUERA; esperando_datos_escritura = 0; return; }
  if (t_estado == T_FUERA || cual != t_actual) {         // tarjeta nueva en el campo
    t_actual = cual;
    t_estado = T_IDLE;
    esperando_datos_escritura = 0;
  }
}

uint8_t emu_bloque4(uint8_t t, uint8_t i) { return bloque4[t][i]; }
uint8_t emu_evento(void) { if (ev_ini == ev_fin) return EV_NADA; uint8_t e = ev_cola[ev_ini]; ev_ini = (uint8_t)((ev_ini + 1) & 7); return e; }

// Arranque: el chip sale del reset de encendido
void emu_iniciar(void) { reiniciar_chip(); }
