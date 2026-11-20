// =====================================================================
// REFERENCIA PROBADA EN EL ENTRENADOR (18-nov-2026). NO es parte del proyecto.
// De aquí se reutilizan, con los MISMOS nombres y estilo, el LCD
// (pulsoE, instruc, escrib, texto, lcd_iniciar) y el teclado no bloqueante
// (teclado_iniciar, teclado_escanear, teclado_nueva). La parte de USART1 no se usa.
// =====================================================================
// ESCLAVO - USART1 bidireccional - Arduino Mega 2560, 16 MHz
// El esclavo NUNCA habla por su cuenta: cada vez que le llega un byte del
// maestro, contesta de inmediato con su propia tecla (o 0 si no hay).
// La respuesta se da en la interrupción de "byte recibido" (USART1_RX_vect),
// así contesta a tiempo aunque el loop esté ocupado con el teclado o el LCD.
// Formato: 9600 baudios, 8N1. Tecla = código ASCII. 0 = "no hay tecla".
//
// LCD:  Envie:  A
//       Recibi: 5
// =====================================================================

#include <util/delay.h>
#include <avr/interrupt.h>

// ======================= LCD (PA = datos, PC0 = RS, PC1 = E) =======================
void pulsoE(void) {
  PORTC |= 0b00000010;      // E = 1
  _delay_us(1);
  PORTC &= ~0b00000010;     // E = 0 -> el LCD lee el bus
}
void instruc(uint8_t comando) {
  PORTA = comando;
  PORTC &= ~0b00000001;     // RS = 0: instrucción
  pulsoE();
  _delay_ms(2);
}
void escrib(uint8_t letra) {
  PORTA = letra;
  PORTC |= 0b00000001;      // RS = 1: dato
  pulsoE();
  _delay_us(50);
}
void texto(const char t[]) {
  uint8_t i = 0;
  while (t[i] != 0) {
    escrib(t[i]);
    i++;
  }
}
void lcd_iniciar(void) {
  DDRA = 0b11111111;
  DDRC |= 0b00000011;
  PORTC &= ~0b00000011;
  _delay_ms(50);
  instruc(0x38);            // 8 bits, 2 líneas
  instruc(0x0C);            // display ON, sin cursor
  instruc(0x06);            // incrementa
  instruc(0x01);            // borrar
}

// ======================= TECLADO (PL0..PL3 filas, PL4..PL7 columnas) =======================
const char teclas[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
const uint8_t fila_activa[4] = {0b00000001, 0b00000010, 0b00000100, 0b00001000};
char tecla_anterior = 0;

void teclado_iniciar(void) {
  DDRL  = 0b00000000;       // todo entrada
  PORTL = 0b11110000;       // pull-ups internas SOLO en las columnas (PL4..PL7); filas en 0
}

// Revisa las 4 filas UNA vez. Devuelve la tecla presionada o 0 si no hay ninguna.
char teclado_escanear(void) {
  for (uint8_t fila = 0; fila < 4; fila++) {
    DDRL = fila_activa[fila];            // solo esta fila es salida, en 0
    _delay_us(10);
    uint8_t col = PINL & 0b11110000;
    DDRL = 0b00000000;
    if ((col & 0b00010000) == 0) return teclas[fila][0];
    if ((col & 0b00100000) == 0) return teclas[fila][1];
    if ((col & 0b01000000) == 0) return teclas[fila][2];
    if ((col & 0b10000000) == 0) return teclas[fila][3];
  }
  return 0;
}

// Devuelve la tecla SOLO en el momento en que se presiona (una vez por pulsación); si no, 0.
// No se queda esperando: así el programa sigue atendiendo la comunicación.
char teclado_nueva(void) {
  char actual = teclado_escanear();
  char nueva = 0;
  if (actual != 0 && tecla_anterior == 0) {
    nueva = actual;
  }
  tecla_anterior = actual;
  return nueva;
}

// ======================= USART1 =======================
void usart1_iniciar(void) {
  UBRR1H = 0;               // UBRR = 16 000 000 / (16 * 9600) - 1 = 103,17 -> 103
  UBRR1L = 103;
  UCSR1A = 0b00000000;      // velocidad normal
  UCSR1B = 0b10011000;      // RXCIE1 = 1 (bit 7): interrupción al recibir
                            // RXEN1  = 1 (bit 4): receptor
                            // TXEN1  = 1 (bit 3): transmisor
  UCSR1C = 0b00000110;      // 8N1
}

// Variables compartidas entre la interrupción y el loop: van "volatile"
volatile char pendiente = 0;        // tecla propia que falta por enviar
volatile uint8_t recibido = 0;      // último byte que mandó el maestro
volatile uint8_t hay_dato = 0;      // 1 = llegó algo nuevo del maestro

// Rutina corta, sin llamadas (regla de la profesora)
ISR(USART1_RX_vect)
{
  uint8_t estado = UCSR1A;          // errores antes que el dato
  recibido = UDR1;
  if (estado & 0b00011100) {        // FE1, DOR1, UPE1
    recibido = '?';
  }
  UDR1 = pendiente;                 // contestar de inmediato (el buffer está libre: solo
                                    // se transmite una vez por cada byte recibido)
  pendiente = 0;
  hay_dato = 1;
}

// ======================= PROGRAMA =======================
void setup() {
  lcd_iniciar();
  teclado_iniciar();
  usart1_iniciar();                 // el núcleo de Arduino ya habilitó las interrupciones (sei)
  instruc(0x80);  texto("Envie:  -");
  instruc(0xC0);  texto("Recibi: -");
}

void loop() {
  // 1. ¿Presionaron una tecla aquí? Se deja lista para el próximo turno del maestro.
  char tecla = teclado_nueva();
  if (tecla != 0) {
    pendiente = tecla;
    instruc(0x88);  escrib(tecla);  // línea 1, columna 8
  }

  // 2. ¿El maestro mandó algo? Mostrarlo si es una tecla (0 = no había tecla).
  if (hay_dato) {
    hay_dato = 0;
    if (recibido != 0) {
      instruc(0xC8);  escrib(recibido);   // línea 2, columna 8
    }
  }

  _delay_ms(20);                    // antirrebote del teclado
}
