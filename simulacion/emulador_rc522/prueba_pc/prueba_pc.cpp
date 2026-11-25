// Prueba en el PC: la librería MFRC522 1.4.12 REAL + tarjeta.cpp/saldo.cpp REALES
// hablando byte a byte con rc522_logica.c (el mismo código que corre en el ATmega328P).
#include <stdio.h>
#include "Arduino.h"
#include "SPI.h"
#include "../rc522_logica.h"
#include "tarjeta.h"
#include "saldo.h"

StubSerial Serial;
SPIClass SPI;
static unsigned long reloj = 0;
static uint8_t spdr = 0x5A;           // basura antes del primer byte
static long bytes_spi = 0;

void pinMode(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return HIGH; }      // RST con pull-up: la librería hace reset por software
unsigned long millis(void) { return reloj++; }
void delay(unsigned long ms) { reloj += ms; emu_procesar(); }
void delayMicroseconds(unsigned int) {}
void digitalWrite(uint8_t pin, uint8_t v) {
  if (pin != 53) return;
  if (v == LOW) { emu_ss_bajo(); spdr = 0x5A; }
  else { emu_ss_alto(); emu_procesar(); }      // el bucle principal trabaja entre tramas
}
uint8_t SPIClass::transfer(uint8_t b) {
  uint8_t r = spdr;                   // lo que el esclavo cargó después del byte anterior
  spdr = emu_byte(b);
  bytes_spi++;
  return r;
}

static int fallos = 0;
#define CHECK(c, msg) do { if (c) printf("  ok   %s\n", msg); else { printf("  FALLA %s\n", msg); fallos++; } } while (0)

int main() {
  emu_iniciar();
  emu_entradas(0, 0, 0, 0);
  tarjeta_iniciar();
  uint32_t s; ResultadoTarjeta r;

  printf("1. Sin tarjeta\n");
  CHECK(!tarjeta_presente(), "no detecta nada");

  printf("2. Tarjeta A ($5000)\n");
  emu_entradas(1, 0, 0, 0);
  long b0 = bytes_spi;
  CHECK(tarjeta_presente(), "detecta");
  uint8_t uid[10]; uint8_t n = tarjeta_uid(uid);
  CHECK(n == 4 && uid[0] == 0xDE && uid[3] == 0xEF, "UID DE AD BE EF");
  CHECK(strcmp(tarjeta_tipo_texto(), "1K") == 0, "tipo 1K");
  r = tarjeta_leer_saldo(&s);
  printf("       bytes SPI para detectar+leer: %ld\n", bytes_spi - b0);
  CHECK(r == TARJETA_OK && s == 5000, "saldo 5000");
  r = tarjeta_escribir_saldo(3800);
  CHECK(r == TARJETA_OK, "escribe 3800 y verifica");
  tarjeta_terminar();
  CHECK(!tarjeta_presente(), "dormida (HALT): REQA no la ve");
  CHECK(tarjeta_despertar(), "WUPA la despierta");
  r = tarjeta_leer_saldo(&s);
  CHECK(r == TARJETA_OK && s == 3800, "relee 3800");
  tarjeta_terminar();

  printf("3. Retirar y volver a acercar\n");
  emu_entradas(0, 0, 0, 0); emu_entradas(1, 0, 0, 0);
  CHECK(tarjeta_presente(), "se detecta otra vez");
  tarjeta_terminar();

  printf("4. Tarjeta B (en blanco)\n");
  emu_entradas(1, 1, 0, 0);
  CHECK(tarjeta_presente(), "detecta B");
  r = tarjeta_leer_saldo(&s);
  CHECK(r == TARJETA_EN_BLANCO, "en blanco");
  r = tarjeta_escribir_saldo(10000);
  CHECK(r == TARJETA_OK, "recarga 10000");
  tarjeta_terminar();

  printf("5. Falla de escritura SIN guardar (no quedo)\n");
  emu_entradas(0, 0, 0, 0); emu_entradas(1, 0, 1, 0);
  CHECK(tarjeta_presente(), "detecta A");
  r = tarjeta_escribir_saldo(1000);
  CHECK(r == TARJETA_ERROR_ESCRITURA, "error de escritura");
  tarjeta_terminar();
  emu_entradas(1, 0, 0, 0);
  CHECK(tarjeta_despertar(), "WUPA");
  r = tarjeta_leer_saldo(&s);
  CHECK(r == TARJETA_OK && s == 3800, "saldo sigue en 3800");
  tarjeta_terminar();

  printf("6. Falla de escritura GUARDANDO (si quedo)\n");
  emu_entradas(0, 0, 0, 0); emu_entradas(1, 0, 1, 1);
  CHECK(tarjeta_presente(), "detecta A");
  r = tarjeta_escribir_saldo(1000);
  CHECK(r == TARJETA_ERROR_ESCRITURA, "error de escritura");
  tarjeta_terminar();
  emu_entradas(1, 0, 0, 0);
  CHECK(tarjeta_despertar(), "WUPA");
  r = tarjeta_leer_saldo(&s);
  CHECK(r == TARJETA_OK && s == 1000, "saldo quedo en 1000");
  tarjeta_terminar();

  printf("7. Cambiar a B con VERIFICAR (otra tarjeta)\n");
  emu_entradas(1, 1, 0, 0);
  CHECK(tarjeta_despertar(), "WUPA ve B");
  n = tarjeta_uid(uid);
  CHECK(uid[0] == 0x12, "UID de B (la FSM dira 'Otra tarjeta')");

  printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO OK", fallos);
  return fallos;
}
