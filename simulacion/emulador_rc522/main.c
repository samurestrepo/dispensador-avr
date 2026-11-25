// =====================================================================
// main.c — Emulador del RC522 en un ATmega328P. SOLO PARA PROTEUS.
// El Mega ejecuta el MISMO .hex que va al montaje; este micro hace de RC522.
//
// Conexiones (esclavo SPI):
//   PB2 SS   <- pin 53 del Mega (SDA/SS)     PB3 MOSI <- pin 51
//   PB5 SCK  <- pin 52                       PB4 MISO -> pin 50
// Interruptores (LOGICSTATE):
//   PC0 TARJETA      1 = hay tarjeta en el campo
//   PC1 TARJETA_B    0 = tarjeta A ($5.000), 1 = tarjeta B (en blanco)
//   PC2 FALLA        1 = la próxima escritura no recibe ACK (D8)
//   PC3 FALLA_GUARDA con FALLA = 1: 1 = los datos SÍ quedan en la tarjeta
// Registro de eventos: TXD (PD1) a 9600 8N1 -> VIRTUAL TERMINAL.
//
// RELOJ: la librería usa SPI a 4 MHz; entre dos bytes el Mega deja menos de
// 1 µs, y en ese tiempo este micro debe cargar SPDR con la respuesta. A 16 MHz
// no alcanza, así que en Proteus se le pone un reloj de 64 MHz (propiedad
// Clock Frequency, CKSEL = Ext. Clock, CKDIV8 sin programar). Proteus 8.13 no
// respeta 160 MHz (probado: la USART sale corrida); 64 MHz sí y sin colisiones SPI.
// En la vida real esto no existe: el RC522 es hardware dedicado.
// =====================================================================
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "rc522_logica.c"       // una sola unidad de compilación: la ISR queda en línea

ISR(SPI_STC_vect) {
  SPDR = emu_byte(SPDR);        // SPCR bit SPIE: un byte completo -> se carga la respuesta
}

EMPTY_INTERRUPT(PCINT1_vect);   // un interruptor cambió: solo despierta al bucle

ISR(PCINT0_vect) {              // PB2 (SS) cambió: borde de trama
  if (PINB & 0b00000100) emu_ss_alto();
  else                   emu_ss_bajo();
}

// ---------- registro por la USART (sin bloquear) ----------
static char cola[255];
static uint8_t c_ini, c_fin;   // índices de la cola (sizeof(cola) = 255 cabe en uint8_t)
static void poner(char c) { uint8_t s = (uint8_t)((c_fin + 1) % sizeof(cola)); if (s != c_ini) { cola[c_fin] = c; c_fin = s; } }
static void texto(const char *t) { while (*t) poner(*t++); }
static void numero(uint32_t v) { char b[11]; int8_t i = 0; do { b[i++] = (char)('0' + v % 10); v /= 10; } while (v); while (i) poner(b[--i]); }
static void hex8(uint8_t v) { const char *h = "0123456789ABCDEF"; poner(h[v >> 4]); poner(h[v & 15]); }

static void mostrar_bloque(uint8_t t) {
  uint32_t s = 0, c = 0; uint8_t blanco = 1;
  for (uint8_t i = 0; i < 16; i++) if (emu_bloque4(t, i)) blanco = 0;
  for (int8_t i = 3; i >= 0; i--) { s = (s << 8) | emu_bloque4(t, i); c = (c << 8) | emu_bloque4(t, 4 + i); }
  if (blanco) texto("en blanco");
  else if (s == ~c) { texto("$"); numero(s); }
  else { texto("NO VALIDO "); for (uint8_t i = 0; i < 8; i++) hex8(emu_bloque4(t, i)); }
}

int main(void) {
  emu_iniciar();
  DDRB  = 0b00010000;           // PB4 MISO salida; SS, MOSI, SCK entradas
  SPCR  = 0b11000000;           // SPIE=1, SPE=1, esclavo, modo 0, MSB primero
  PCMSK0 = 0b00000100;          // PCINT2 = PB2 (SS)
  PCMSK1 = 0b00001111;          // PCINT8–11 = PC0–PC3 (interruptores)
  PCICR  = 0b00000011;          // PCIE0 y PCIE1
  DDRC  = 0b00000000;           // PC0–PC3 interruptores
  UBRR0 = (uint16_t)(F_CPU / 16 / 9600 - 1);
  UCSR0B = 0b00001000;          // TXEN0
  UCSR0C = 0b00000110;          // 8N1
  sei();

  texto("\r\nEmulador RC522 listo. A=$5000, B=en blanco\r\n");
  uint8_t prev = 0xFF;
  for (;;) {
    emu_procesar();

    uint8_t e = PINC & 0b00001111;
    if (e != prev) {
      emu_entradas(e & 1, (e >> 1) & 1, (e >> 2) & 1, (e >> 3) & 1);
      if ((e ^ prev) & 0b00000011) {
        if (e & 1) { texto("Tarjeta "); poner((e & 2) ? 'B' : 'A'); texto(" en el campo: "); mostrar_bloque((e >> 1) & 1); texto("\r\n"); }
        else texto("Tarjeta retirada\r\n");
      }
      prev = e;
    }
    uint8_t ev = emu_evento();
    if (ev) {
      uint8_t t = (prev >> 1) & 1;
      poner(t ? 'B' : 'A'); texto(": ");
      if (ev == EV_LECTURA)  { texto("lectura bloque 4 -> "); mostrar_bloque(t); }
      if (ev == EV_ESCRITURA) { texto("ESCRITO -> "); mostrar_bloque(t); }
      if (ev == EV_ESCRITURA_FALLIDA) { texto("FALLA de escritura (sin ACK). Bloque: "); mostrar_bloque(t); }
      if (ev == EV_HALT) texto("HALT (dormida)");
      if (ev == EV_AUTH_MAL) texto("autenticacion rechazada");
      texto("\r\n");
    }
    if (c_ini != c_fin) {
      if (UCSR0A & 0b00100000) { UDR0 = (uint8_t)cola[c_ini]; c_ini = (uint8_t)((c_ini + 1) % sizeof(cola)); }
      continue;                 // queda texto por enviar: no dormir
    }
    // Dormir hasta la próxima interrupción (SPI, SS o interruptor): así Proteus
    // no simula un bucle vacío a F_CPU y la simulación va más rápido.
    cli();
    if (!emu_hay_trabajo()) {
      sleep_enable();
      sei();                    // sei + sleep: no se pierde una interrupción entre los dos
      sleep_cpu();
      sleep_disable();
    }
    sei();
  }
}
