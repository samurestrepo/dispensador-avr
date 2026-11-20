// =====================================================================
// prueba_fsm.cpp — Pruebas de la máquina de estados en el PC (sin hardware)
//
// Se compila fsm.cpp, pedido.cpp y saldo.cpp DE VERDAD, y aquí abajo se
// escriben versiones SIMULADAS de lcd, teclado, monedas, reles y tarjeta
// (las mismas funciones de sus .h). Así se puede:
//   - "pulsar" una tecla, "meter" una moneda, acercar/retirar una tarjeta,
//   - avanzar el reloj (la FSM recibe la hora como parámetro),
//   - y comprobar el texto exacto del LCD, el estado y los LEDs.
// =====================================================================
#include <string.h>
#include "mini_prueba.h"
#include "config.h"
#include "lcd.h"
#include "teclado.h"
#include "monedas.h"
#include "reles.h"
#include "tarjeta.h"
#include "pedido.h"
#include "saldo.h"
#include "fsm.h"

// ======================= Módulos simulados =======================

// --- LCD: guarda las dos líneas y cuenta cuántas veces se dibujó.
static char lcd1[17];
static char lcd2[17];
static int dibujos = 0;

void lcd_pantalla(const char linea1[], const char linea2[]) {
  strncpy(lcd1, linea1, 16);
  lcd1[16] = 0;
  strncpy(lcd2, linea2, 16);
  lcd2[16] = 0;
  dibujos++;
}

// --- Teclado: una tecla pendiente que se entrega una sola vez.
static char tecla_pendiente = 0;

char teclado_leer(void) {
  char t = tecla_pendiente;
  tecla_pendiente = 0;
  return t;
}

// --- Monedas: una moneda pendiente; solo "entra" si la FSM llama a monedas_leer().
static uint16_t moneda_pendiente = 0;
static int rearmes = 0;

void monedas_rearmar(void) {
  rearmes++;
}

uint16_t monedas_leer(void) {
  uint16_t m = moneda_pendiente;
  moneda_pendiente = 0;
  return m;
}

// --- Relés: estado de cada LED y cuántas veces se encendió.
static bool led[2];
static int encendidos[2];

void rele_encender(uint8_t producto) {
  led[producto] = true;
  encendidos[producto]++;
}

void rele_apagar(uint8_t producto) {
  led[producto] = false;
}

// --- Tarjeta: una tarjeta simulada con su UID y su bloque 4.
// Imita lo importante del RC522: una tarjeta dormida (tras terminar) no la ve
// tarjeta_presente() (REQA), pero sí tarjeta_despertar() (WUPA).
enum FalloEscritura {
  SIN_FALLO,
  FALLA_SIN_ESCRIBIR,       // la escritura falla y el bloque queda igual
  FALLA_DESPUES_DE_ESCRIBIR,// el bloque SÍ cambió, pero la relectura falló
  FALLA_Y_CORROMPE          // se cortó a mitad: el bloque queda dañado
};

static struct {
  bool en_campo;
  bool dormida;
  bool es_1k;
  uint8_t uid[4];
  uint8_t bloque[SALDO_TAM_BLOQUE];
  FalloEscritura fallo;
  int escrituras;            // intentos de escritura
  int terminadas;            // llamadas a tarjeta_terminar()
} tarjeta;

bool tarjeta_presente(void) {
  return tarjeta.en_campo && !tarjeta.dormida;
}

bool tarjeta_despertar(void) {
  if (!tarjeta.en_campo) {
    return false;
  }
  tarjeta.dormida = false;
  return true;
}

ResultadoTarjeta tarjeta_leer_saldo(uint32_t *saldo) {
  *saldo = 0;
  if (!tarjeta.es_1k) {
    return TARJETA_NO_SOPORTADA;
  }
  EstadoBloque estado = saldo_decodificar(tarjeta.bloque, saldo);
  if (estado == BLOQUE_EN_BLANCO) return TARJETA_EN_BLANCO;
  if (estado == BLOQUE_NO_VALIDO) return TARJETA_NO_VALIDA;
  return TARJETA_OK;
}

ResultadoTarjeta tarjeta_escribir_saldo(uint32_t saldo) {
  tarjeta.escrituras++;
  if (tarjeta.fallo == FALLA_SIN_ESCRIBIR) {
    return TARJETA_ERROR_ESCRITURA;
  }
  saldo_codificar(saldo, tarjeta.bloque);
  if (tarjeta.fallo == FALLA_DESPUES_DE_ESCRIBIR) {
    return TARJETA_ERROR_VERIFICACION;
  }
  if (tarjeta.fallo == FALLA_Y_CORROMPE) {
    tarjeta.bloque[5] = 0x00;          // complemento roto
    return TARJETA_ERROR_VERIFICACION;
  }
  return TARJETA_OK;
}

void tarjeta_terminar(void) {
  tarjeta.terminadas++;
  tarjeta.dormida = true;
}

uint8_t tarjeta_uid(uint8_t destino[10]) {
  for (uint8_t i = 0; i < 4; i++) {
    destino[i] = tarjeta.uid[i];
  }
  return 4;
}

// ======================= Ayudas de las pruebas =======================
static uint32_t reloj = 0;

static void vuelta(void) {               // una vuelta del loop, 10 ms después
  reloj += 10;
  fsm_tick(reloj);
}

static void esperar(uint32_t ms) {
  for (uint32_t i = 0; i < ms / 10; i++) {
    vuelta();
  }
}

static void pulsar(char tecla) {
  tecla_pendiente = tecla;
  vuelta();
}

static void pulsar_varias(const char teclas[]) {
  for (int i = 0; teclas[i] != 0; i++) {
    pulsar(teclas[i]);
  }
}

static void meter_moneda(uint16_t valor) {
  moneda_pendiente = valor;
  vuelta();
}

static void preparar_tarjeta(uint8_t id, uint32_t saldo) {
  tarjeta.es_1k = true;
  tarjeta.uid[0] = id;
  tarjeta.uid[1] = 0x22;
  tarjeta.uid[2] = 0x33;
  tarjeta.uid[3] = 0x44;
  saldo_codificar(saldo, tarjeta.bloque);
  tarjeta.fallo = SIN_FALLO;
}

static void preparar_tarjeta_en_blanco(uint8_t id) {
  preparar_tarjeta(id, 0);
  for (int i = 0; i < SALDO_TAM_BLOQUE; i++) {
    tarjeta.bloque[i] = 0;
  }
}

static void acercar(void) {
  tarjeta.en_campo = true;
  tarjeta.dormida = false;
}

static void retirar(void) {
  tarjeta.en_campo = false;
  tarjeta.dormida = false;               // al salir del campo la tarjeta se reinicia
}

static uint32_t saldo_en_tarjeta(void) {
  uint32_t s = 0;
  saldo_decodificar(tarjeta.bloque, &s);
  return s;
}

static bool lcd_es(const char a[], const char b[]) {
  bool igual = strcmp(lcd1, a) == 0 && strcmp(lcd2, b) == 0;
  if (!igual) {
    printf("    LCD: \"%s\" / \"%s\"  (se esperaba \"%s\" / \"%s\")\n", lcd1, lcd2, a, b);
  }
  return igual;
}

// Deja todo como recién encendido.
static void reiniciar(void) {
  memset(&tarjeta, 0, sizeof(tarjeta));
  tecla_pendiente = 0;
  moneda_pendiente = 0;
  rearmes = 0;
  led[0] = false;
  led[1] = false;
  encendidos[0] = 0;
  encendidos[1] = 0;
  reloj = 1000;
  fsm_iniciar(reloj);
}

// Desde INICIO hasta METODO_PAGO con "agua" aguas y "gaseosa" gaseosas.
static void llegar_a_metodo_pago(int agua, int gaseosa) {
  for (int i = 0; i < agua; i++) pulsar('1');
  for (int i = 0; i < gaseosa; i++) pulsar('2');
  pulsar('#');                           // SELECCION -> CONFIRMAR
  pulsar('#');                           // CONFIRMAR -> METODO_PAGO
}

// ======================= Pruebas =======================

static void prueba_inicio(void) {
  caso("INICIO: pantalla y teclas que no hacen nada");
  reiniciar();
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(lcd_es("Dispensador", "1:Agua 2:Gaseosa"));
  pulsar('#');
  pulsar('A');
  pulsar('9');
  VERIFICAR(fsm_estado() == INICIO);
}

static void prueba_redibujar_solo_si_cambia(void) {
  caso("el LCD solo se redibuja cuando algo cambia");
  reiniciar();
  int antes = dibujos;
  esperar(5000);                         // 500 vueltas sin hacer nada
  VERIFICAR(dibujos == antes);
  pulsar('1');
  VERIFICAR(dibujos == antes + 1);
}

static void prueba_seleccion(void) {
  caso("SELECCION: +1, -1, maximo 9, # solo con total > 0, * borra todo");
  reiniciar();
  pulsar('1');
  VERIFICAR(fsm_estado() == SELECCION);
  VERIFICAR(lcd_es("Ag:1 Gs:0", "Total $500"));
  pulsar('1');
  pulsar('2');
  VERIFICAR(lcd_es("Ag:2 Gs:1", "Total $1700"));
  pulsar('4');
  VERIFICAR(lcd_es("Ag:1 Gs:1", "Total $1200"));
  pulsar('4');
  pulsar('5');
  VERIFICAR(lcd_es("Ag:0 Gs:0", "Total $0"));
  pulsar('#');                                          // total 0: no avanza
  VERIFICAR(fsm_estado() == SELECCION);
  pulsar_varias("2222222222");                          // 10 veces: se queda en 9
  VERIFICAR(lcd_es("Ag:0 Gs:9", "Total $6300"));
  pulsar('*');
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(pedido_total() == 0);
}

static void prueba_confirmar(void) {
  caso("CONFIRMAR: alterna cada 2 s, 4/5 quitan, vacio -> INICIO, * -> SELECCION");
  reiniciar();
  pulsar_varias("112#");
  VERIFICAR(fsm_estado() == CONFIRMAR);
  VERIFICAR(lcd_es("Ag 2x500=$1000", "Gs 1x700=$ 700"));
  esperar(1990);
  VERIFICAR(lcd_es("Ag 2x500=$1000", "Gs 1x700=$ 700"));
  esperar(10);
  VERIFICAR(lcd_es("Total $1700", "#:Si *:Atras"));
  esperar(2000);
  VERIFICAR(lcd_es("Ag 2x500=$1000", "Gs 1x700=$ 700"));
  pulsar('*');
  VERIFICAR(fsm_estado() == SELECCION);
  VERIFICAR(lcd_es("Ag:2 Gs:1", "Total $1700"));        // * no borra el pedido
  pulsar('#');
  pulsar('5');
  VERIFICAR(fsm_estado() == CONFIRMAR);
  VERIFICAR(lcd_es("Ag 2x500=$1000", "Gs 0x700=$   0"));
  pulsar('4');
  pulsar('4');
  VERIFICAR(fsm_estado() == INICIO);                    // quedó en 0
}

static void prueba_metodo_pago(void) {
  caso("METODO_PAGO: pantalla y * -> CONFIRMAR");
  reiniciar();
  llegar_a_metodo_pago(1, 0);
  VERIFICAR(fsm_estado() == METODO_PAGO);
  VERIFICAR(lcd_es("A:Monedas", "B:Tarjeta *:Atr"));
  pulsar('*');
  VERIFICAR(fsm_estado() == CONFIRMAR);
}

static void prueba_monedas_fuera_de_pago(void) {
  caso("D5: fuera de PAGO_MONEDAS no se leen las monedas");
  reiniciar();
  llegar_a_metodo_pago(1, 0);
  moneda_pendiente = 500;
  vuelta();
  VERIFICAR(moneda_pendiente == 500);                   // nadie la leyó
  moneda_pendiente = 0;
}

static void prueba_monedas_con_devuelta(void) {
  caso("PAGO_MONEDAS: foto al entrar (D4), devuelta (R7) y completar el restante");
  reiniciar();
  llegar_a_metodo_pago(0, 1);                            // total 700
  pulsar('A');
  VERIFICAR(fsm_estado() == PAGO_MONEDAS);
  VERIFICAR(rearmes == 1);
  VERIFICAR(lcd_es("Falta $700", "Pagado $0"));
  meter_moneda(500);
  VERIFICAR(lcd_es("Falta $200", "Pagado $500"));
  meter_moneda(500);                                     // se pasa
  VERIFICAR(fsm_mostrando_mensaje());
  VERIFICAR(lcd_es("Devuelta $500", "Ingrese $200"));
  moneda_pendiente = 200;                                // entra durante el mensaje...
  vuelta();
  VERIFICAR(moneda_pendiente == 200);                    // ...no se pierde: espera
  esperar(2000);
  VERIFICAR(rearmes == 1);                               // el mensaje no repite la foto
  VERIFICAR(fsm_estado() == DISPENSAR);                  // la de 200 completó el pago
  VERIFICAR(lcd_es("Sirv. gaseosa", "Unidad 1 de 1"));
  VERIFICAR(led[PRODUCTO_GASEOSA] == true);
}

static void prueba_cancelar_monedas(void) {
  caso("D13: * en PAGO_MONEDAS devuelve lo ingresado y vuelve a INICIO");
  reiniciar();
  llegar_a_metodo_pago(2, 1);
  pulsar('A');
  meter_moneda(500);
  meter_moneda(200);
  pulsar('*');
  VERIFICAR(lcd_es("Devolviendo", "$700"));
  esperar(1990);
  VERIFICAR(fsm_estado() == PAGO_MONEDAS);               // todavía el mensaje
  esperar(10);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(pedido_total() == 0);
  VERIFICAR(pago_pagado() == 0);

  caso("D13: * sin monedas va directo a INICIO");
  llegar_a_metodo_pago(1, 0);
  pulsar('A');
  pulsar('*');
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(!fsm_mostrando_mensaje());
}

static void prueba_dispensar(void) {
  caso("DISPENSAR (D12): agua primero, 2 s encendido y 0,5 s apagado por unidad");
  reiniciar();
  llegar_a_metodo_pago(2, 1);
  pulsar('A');
  meter_moneda(500);
  meter_moneda(500);
  meter_moneda(500);
  meter_moneda(200);
  VERIFICAR(fsm_estado() == DISPENSAR);
  VERIFICAR(lcd_es("Sirviendo agua", "Unidad 1 de 2"));
  VERIFICAR(led[PRODUCTO_AGUA] && !led[PRODUCTO_GASEOSA]);
  esperar(2000);
  VERIFICAR(!led[PRODUCTO_AGUA]);                        // apagado 0,5 s
  esperar(500);
  VERIFICAR(led[PRODUCTO_AGUA]);
  VERIFICAR(lcd_es("Sirviendo agua", "Unidad 2 de 2"));
  esperar(2500);
  VERIFICAR(led[PRODUCTO_GASEOSA] && !led[PRODUCTO_AGUA]);
  VERIFICAR(lcd_es("Sirv. gaseosa", "Unidad 1 de 1"));
  pulsar('*');                                           // las teclas no interrumpen
  VERIFICAR(fsm_estado() == DISPENSAR);
  esperar(2500);
  VERIFICAR(encendidos[PRODUCTO_AGUA] == 2);
  VERIFICAR(encendidos[PRODUCTO_GASEOSA] == 1);
  VERIFICAR(fsm_estado() == GRACIAS);
  VERIFICAR(lcd_es("Gracias!", "Disfrute"));
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(pedido_total() == 0);
}

static void prueba_pago_tarjeta_ok(void) {
  caso("PAGO_TARJETA: cobra, escribe y dispensa");
  reiniciar();
  preparar_tarjeta(0xA1, 5000);
  llegar_a_metodo_pago(1, 0);
  pulsar('B');
  VERIFICAR(lcd_es("Acerque tarjeta", "Total $500"));
  esperar(1000);                                         // sin tarjeta no pasa nada
  VERIFICAR(fsm_estado() == PAGO_TARJETA);
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo $4500", ""));
  VERIFICAR(saldo_en_tarjeta() == 4500);
  VERIFICAR(tarjeta.escrituras == 1);
  VERIFICAR(tarjeta.terminadas == 1);                    // se cerró la operación
  esperar(2000);
  VERIFICAR(fsm_estado() == DISPENSAR);
}

static void prueba_saldo_insuficiente(void) {
  caso("PAGO_TARJETA: saldo insuficiente (R8) -> METODO_PAGO sin escribir");
  reiniciar();
  preparar_tarjeta(0xA1, 1000);
  llegar_a_metodo_pago(2, 1);                            // 1700
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo insuf.", "Saldo $1000"));
  VERIFICAR(tarjeta.escrituras == 0);
  VERIFICAR(tarjeta.terminadas == 1);
  esperar(2000);
  VERIFICAR(fsm_estado() == METODO_PAGO);
  VERIFICAR(pedido_total() == 1700);                     // el pedido se conserva

  caso("PAGO_TARJETA: tarjeta en blanco = saldo $0");
  preparar_tarjeta_en_blanco(0xA1);
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo insuf.", "Saldo $0"));
  VERIFICAR(tarjeta.escrituras == 0);
}

static void prueba_pago_tarjeta_otros(void) {
  caso("PAGO_TARJETA: no valida, no soportada, * y 20 s (D14)");
  reiniciar();
  preparar_tarjeta(0xA1, 5000);
  tarjeta.bloque[4] = 0x00;                              // complemento dañado
  llegar_a_metodo_pago(1, 0);
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Tarjeta", "no valida"));
  esperar(2000);
  VERIFICAR(fsm_estado() == METODO_PAGO);

  retirar();
  preparar_tarjeta(0xA1, 5000);
  tarjeta.es_1k = false;
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(lcd_es("No soportada", "Use MIFARE 1K"));
  esperar(2000);
  VERIFICAR(fsm_estado() == METODO_PAGO);

  retirar();
  pulsar('B');
  pulsar('*');
  VERIFICAR(fsm_estado() == METODO_PAGO);

  pulsar('B');
  esperar(19980);
  VERIFICAR(fsm_estado() == PAGO_TARJETA);
  esperar(20);
  VERIFICAR(fsm_estado() == METODO_PAGO);
  VERIFICAR(tarjeta.escrituras == 0);
}

static void prueba_consulta_saldo(void) {
  caso("CONSULTA_SALDO (D16): muestra 3 s y nunca escribe");
  reiniciar();
  preparar_tarjeta(0xB2, 3300);
  pulsar('C');
  VERIFICAR(fsm_estado() == CONSULTA_SALDO);
  VERIFICAR(lcd_es("Acerque tarjeta", "Consulta saldo"));
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo $3300", ""));
  esperar(2990);
  VERIFICAR(fsm_mostrando_mensaje());
  esperar(10);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(tarjeta.escrituras == 0);
  VERIFICAR(tarjeta.terminadas == 1);

  caso("CONSULTA_SALDO: en blanco -> Saldo $0; no valida; * y 20 s");
  retirar();
  preparar_tarjeta_en_blanco(0xB2);
  pulsar('C');
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo $0", ""));
  esperar(3000);

  retirar();
  preparar_tarjeta(0xB2, 3300);
  tarjeta.bloque[9] = 0x07;                              // basura
  pulsar('C');
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Tarjeta", "no valida"));
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);

  retirar();
  pulsar('C');
  pulsar('*');
  VERIFICAR(fsm_estado() == INICIO);
  pulsar('C');
  esperar(20000);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(tarjeta.escrituras == 0);
}

static void prueba_recarga(void) {
  caso("RECARGA: clave con asteriscos, monto, tarjeta nueva y tarjeta con saldo");
  reiniciar();
  pulsar('D');
  VERIFICAR(fsm_estado() == RECARGA_CLAVE);
  VERIFICAR(lcd_es("Clave admin:", ""));
  pulsar_varias("123");
  VERIFICAR(lcd_es("Clave admin:", "***"));
  pulsar_varias("45");                                   // el 5.o digito se ignora
  VERIFICAR(lcd_es("Clave admin:", "****"));
  pulsar('#');
  VERIFICAR(fsm_estado() == RECARGA_MONTO);
  VERIFICAR(lcd_es("Monto recarga:", "$"));
  pulsar('0');                                           // cero a la izquierda: ignorado
  VERIFICAR(lcd_es("Monto recarga:", "$"));
  pulsar_varias("5000");
  VERIFICAR(lcd_es("Monto recarga:", "$5000"));
  pulsar('*');
  VERIFICAR(lcd_es("Monto recarga:", "$500"));
  pulsar('0');
  pulsar('#');
  VERIFICAR(fsm_estado() == RECARGA_TARJETA);
  VERIFICAR(lcd_es("Acerque tarjeta", "Recarga $5000"));

  preparar_tarjeta_en_blanco(0xC3);
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Tarjeta nueva", "Saldo $5000"));
  VERIFICAR(saldo_en_tarjeta() == 5000);
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);

  retirar();
  pulsar_varias("D1234#1000#");
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo $6000", ""));
  VERIFICAR(saldo_en_tarjeta() == 6000);
}

static void prueba_recarga_errores(void) {
  caso("RECARGA: clave errada, monto no multiplo de 100, borrar hasta salir, tope");
  reiniciar();
  pulsar_varias("D1111#");
  VERIFICAR(lcd_es("Clave errada", ""));
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
  pulsar_varias("D12#");                                 // menos de 4 digitos
  VERIFICAR(lcd_es("Clave errada", ""));
  esperar(2000);
  pulsar_varias("D12*");
  VERIFICAR(fsm_estado() == INICIO);

  pulsar_varias("D1234#150#");
  VERIFICAR(lcd_es("Monto no valido", "Multiplo de 100"));
  esperar(2000);
  VERIFICAR(fsm_estado() == RECARGA_MONTO);
  VERIFICAR(lcd_es("Monto recarga:", "$150"));           // conserva lo digitado
  pulsar_varias("***");
  VERIFICAR(lcd_es("Monto recarga:", "$"));
  pulsar('*');
  VERIFICAR(fsm_estado() == INICIO);

  pulsar_varias("D1234#9999999");                        // maximo 6 digitos
  VERIFICAR(lcd_es("Monto recarga:", "$999999"));

  reiniciar();
  preparar_tarjeta(0xC3, 999800);
  pulsar_varias("D1234#200#");
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Supera el tope", "Saldo $999800"));
  VERIFICAR(tarjeta.escrituras == 0);
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
}

static void prueba_verificar_quedo(void) {
  caso("VERIFICAR_TARJETA caso 1: la escritura SI quedo -> dispensa sin cobrar otra vez");
  reiniciar();
  preparar_tarjeta(0xD4, 5000);
  tarjeta.fallo = FALLA_DESPUES_DE_ESCRIBIR;
  llegar_a_metodo_pago(1, 0);
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  VERIFICAR(lcd_es("Acerque de nuevo", "la misma tarjeta"));
  VERIFICAR(tarjeta.dormida);                            // tarjeta_terminar() tras el fallo
  tarjeta.fallo = SIN_FALLO;
  vuelta();                                              // sigue en el campo: WUPA la despierta
  VERIFICAR(lcd_es("Saldo $4500", ""));
  esperar(2000);
  VERIFICAR(fsm_estado() == DISPENSAR);
  VERIFICAR(tarjeta.escrituras == 1);                    // un solo cobro
  VERIFICAR(saldo_en_tarjeta() == 4500);
}

static void prueba_verificar_no_quedo(void) {
  caso("VERIFICAR_TARJETA caso 2: NO quedo -> vuelve a PAGO_TARJETA y se reintenta");
  reiniciar();
  preparar_tarjeta(0xD4, 5000);
  tarjeta.fallo = FALLA_SIN_ESCRIBIR;
  llegar_a_metodo_pago(1, 0);
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  tarjeta.fallo = SIN_FALLO;
  vuelta();
  VERIFICAR(lcd_es("No se cobro", ""));
  esperar(2000);
  VERIFICAR(fsm_estado() == PAGO_TARJETA);
  VERIFICAR(lcd_es("Acerque tarjeta", "Total $500"));
  esperar(500);                                          // dormida: no se cobra sola
  VERIFICAR(fsm_estado() == PAGO_TARJETA);
  retirar();
  vuelta();
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Saldo $4500", ""));
  VERIFICAR(saldo_en_tarjeta() == 4500);                 // cobrado una sola vez

  caso("VERIFICAR_TARJETA caso 2 en recarga de tarjeta nueva: el original es 'en blanco'");
  reiniciar();
  preparar_tarjeta_en_blanco(0xE5);
  tarjeta.fallo = FALLA_SIN_ESCRIBIR;
  pulsar_varias("D1234#5000#");
  acercar();
  vuelta();
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  tarjeta.fallo = SIN_FALLO;
  vuelta();
  VERIFICAR(lcd_es("No se recargo", ""));
  esperar(2000);
  VERIFICAR(fsm_estado() == RECARGA_TARJETA);
  VERIFICAR(lcd_es("Acerque tarjeta", "Recarga $5000")); // conserva el monto

  caso("VERIFICAR_TARJETA caso 1 en recarga: quedo -> INICIO");
  reiniciar();
  preparar_tarjeta(0xE5, 1000);
  tarjeta.fallo = FALLA_DESPUES_DE_ESCRIBIR;
  pulsar_varias("D1234#2000#");
  acercar();
  vuelta();
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  vuelta();
  VERIFICAR(lcd_es("Saldo $3000", ""));
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(tarjeta.escrituras == 1);
}

static void prueba_verificar_no_valida(void) {
  caso("VERIFICAR_TARJETA caso 3: quedo danada -> Tarjeta no valida -> INICIO");
  reiniciar();
  preparar_tarjeta(0xD4, 5000);
  tarjeta.fallo = FALLA_Y_CORROMPE;
  llegar_a_metodo_pago(1, 0);
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  vuelta();
  VERIFICAR(lcd_es("Tarjeta", "no valida"));
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(encendidos[PRODUCTO_AGUA] == 0);             // no se dispensó
}

static void prueba_verificar_otra_y_cancelar(void) {
  caso("VERIFICAR_TARJETA: otra tarjeta, sin tiempo limite, * -> Op. no confirmada");
  reiniciar();
  preparar_tarjeta(0xD4, 5000);
  tarjeta.fallo = FALLA_SIN_ESCRIBIR;
  llegar_a_metodo_pago(1, 0);
  pulsar('B');
  acercar();
  vuelta();
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  retirar();
  esperar(60000);                                        // un minuto: sigue esperando
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  preparar_tarjeta(0x99, 5000);                          // otro UID
  acercar();
  vuelta();
  VERIFICAR(lcd_es("Otra tarjeta", ""));
  retirar();
  esperar(2000);
  VERIFICAR(fsm_estado() == VERIFICAR_TARJETA);
  VERIFICAR(lcd_es("Acerque de nuevo", "la misma tarjeta"));
  pulsar('*');
  VERIFICAR(lcd_es("Operacion no", "confirmada"));
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
}

static void prueba_teclas_durante_mensaje(void) {
  caso("las teclas se descartan mientras hay un mensaje");
  reiniciar();
  pulsar_varias("D1111#");                               // "Clave errada"
  pulsar('1');
  esperar(2000);
  VERIFICAR(fsm_estado() == INICIO);
  VERIFICAR(pedido_total() == 0);                        // el '1' no agrego agua
}

int main() {
  printf("=== prueba_fsm ===\n");
  prueba_inicio();
  prueba_redibujar_solo_si_cambia();
  prueba_seleccion();
  prueba_confirmar();
  prueba_metodo_pago();
  prueba_monedas_fuera_de_pago();
  prueba_monedas_con_devuelta();
  prueba_cancelar_monedas();
  prueba_dispensar();
  prueba_pago_tarjeta_ok();
  prueba_saldo_insuficiente();
  prueba_pago_tarjeta_otros();
  prueba_consulta_saldo();
  prueba_recarga();
  prueba_recarga_errores();
  prueba_verificar_quedo();
  prueba_verificar_no_quedo();
  prueba_verificar_no_valida();
  prueba_verificar_otra_y_cancelar();
  prueba_teclas_durante_mensaje();
  return resumen("prueba_fsm");
}
