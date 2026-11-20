// =====================================================================
// prueba_saldo.cpp — Pruebas de saldo.cpp en el PC (sin hardware ni tarjeta)
// Se compila con:  g++ -std=c++17 -Wall -Wextra -I../dispensador prueba_saldo.cpp ../dispensador/saldo.cpp
// =====================================================================
#include "mini_prueba.h"
#include "saldo.h"

static void prueba_formato_bytes(void) {
  caso("formato D7: 1700 = 0x000006A4, little-endian");
  uint8_t b[SALDO_TAM_BLOQUE];
  saldo_codificar(1700, b);
  VERIFICAR(b[0] == 0xA4);
  VERIFICAR(b[1] == 0x06);
  VERIFICAR(b[2] == 0x00);
  VERIFICAR(b[3] == 0x00);
  VERIFICAR(b[4] == 0x5B);    // ~0xA4
  VERIFICAR(b[5] == 0xF9);    // ~0x06
  VERIFICAR(b[6] == 0xFF);
  VERIFICAR(b[7] == 0xFF);
  bool resto_en_cero = true;
  for (int i = 8; i < SALDO_TAM_BLOQUE; i++) {
    if (b[i] != 0) {
      resto_en_cero = false;
    }
  }
  VERIFICAR(resto_en_cero);
}

static void prueba_ida_y_vuelta(void) {
  caso("codificar -> decodificar devuelve el mismo saldo");
  const uint32_t valores[] = {0, 100, 1700, 5000, 65535, 65536, 123400, SALDO_MAXIMO};
  for (uint32_t v : valores) {
    uint8_t b[SALDO_TAM_BLOQUE];
    uint32_t leido = 12345;
    saldo_codificar(v, b);
    VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_VALIDO);
    VERIFICAR(leido == v);
  }
}

static void prueba_saldo_cero_no_es_blanco(void) {
  caso("saldo $0 valido es distinto de tarjeta en blanco");
  uint8_t b[SALDO_TAM_BLOQUE];
  uint32_t leido = 99;
  saldo_codificar(0, b);
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_VALIDO);
  VERIFICAR(leido == 0);
}

static void prueba_en_blanco(void) {
  caso("16 bytes en 0 = tarjeta nueva");
  uint8_t b[SALDO_TAM_BLOQUE] = {0};
  uint32_t leido = 99;
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_EN_BLANCO);
  VERIFICAR(leido == 0);
}

static void prueba_corrupto(void) {
  caso("bloque corrupto (D7)");
  uint8_t b[SALDO_TAM_BLOQUE];
  uint32_t leido = 99;

  saldo_codificar(1700, b);
  b[0] = 0xA5;                       // cambia un bit del saldo
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_NO_VALIDO);
  VERIFICAR(leido == 0);

  saldo_codificar(1700, b);
  b[6] = 0x00;                       // complemento dañado
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_NO_VALIDO);

  saldo_codificar(1700, b);
  b[4] = 0; b[5] = 0; b[6] = 0; b[7] = 0;   // escritura cortada: saldo sin complemento
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_NO_VALIDO);

  saldo_codificar(1700, b);
  b[15] = 0x01;                      // basura en los bytes que deben ir en 0
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_NO_VALIDO);

  uint8_t todo_ff[SALDO_TAM_BLOQUE];
  for (int i = 0; i < SALDO_TAM_BLOQUE; i++) {
    todo_ff[i] = 0xFF;
  }
  VERIFICAR(saldo_decodificar(todo_ff, &leido) == BLOQUE_NO_VALIDO);
}

static void prueba_tope_al_leer(void) {
  caso("saldo bien formado pero mayor que el tope = no valido (D10)");
  uint8_t b[SALDO_TAM_BLOQUE];
  uint32_t leido = 99;
  saldo_codificar(SALDO_MAXIMO + 100, b);
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_NO_VALIDO);
  VERIFICAR(leido == 0);
  saldo_codificar(0xFFFFFFFF, b);
  VERIFICAR(saldo_decodificar(b, &leido) == BLOQUE_NO_VALIDO);
}

static void prueba_cobro(void) {
  caso("cobro (R8)");
  uint32_t nuevo = 0;
  VERIFICAR(saldo_cobrar(5000, 1700, &nuevo) == true);
  VERIFICAR(nuevo == 3300);
  VERIFICAR(saldo_cobrar(1700, 1700, &nuevo) == true);   // exacto: queda en 0
  VERIFICAR(nuevo == 0);
  VERIFICAR(saldo_cobrar(1600, 1700, &nuevo) == false);  // insuficiente: rechazo
  VERIFICAR(nuevo == 1600);                              // el saldo no cambia
  VERIFICAR(saldo_cobrar(0, 500, &nuevo) == false);
  VERIFICAR(nuevo == 0);
  VERIFICAR(saldo_cobrar(SALDO_MAXIMO, 10800, &nuevo) == true);
  VERIFICAR(nuevo == SALDO_MAXIMO - 10800);
}

static void prueba_recarga(void) {
  caso("recarga (R9, D10)");
  uint32_t nuevo = 0;
  VERIFICAR(saldo_recargar(1000, 5000, &nuevo) == RECARGA_OK);
  VERIFICAR(nuevo == 6000);
  VERIFICAR(saldo_recargar(0, 100, &nuevo) == RECARGA_OK);          // tarjeta nueva, minimo
  VERIFICAR(nuevo == 100);

  VERIFICAR(saldo_recargar(1000, 150, &nuevo) == RECARGA_MONTO_NO_VALIDO);  // no es multiplo de 100
  VERIFICAR(nuevo == 1000);
  VERIFICAR(saldo_recargar(1000, 99, &nuevo) == RECARGA_MONTO_NO_VALIDO);
  VERIFICAR(saldo_recargar(1000, 0, &nuevo) == RECARGA_MONTO_NO_VALIDO);    // monto 0
  VERIFICAR(nuevo == 1000);
}

static void prueba_recarga_tope(void) {
  caso("recarga y tope $999.900 (D10)");
  uint32_t nuevo = 0;
  VERIFICAR(saldo_recargar(0, SALDO_MAXIMO, &nuevo) == RECARGA_OK);        // justo el tope
  VERIFICAR(nuevo == SALDO_MAXIMO);
  VERIFICAR(saldo_recargar(999800, 100, &nuevo) == RECARGA_OK);            // llega justo
  VERIFICAR(nuevo == SALDO_MAXIMO);
  VERIFICAR(saldo_recargar(999800, 200, &nuevo) == RECARGA_SUPERA_TOPE);   // se pasa por 100
  VERIFICAR(nuevo == 999800);
  VERIFICAR(saldo_recargar(SALDO_MAXIMO, 100, &nuevo) == RECARGA_SUPERA_TOPE);
  VERIFICAR(saldo_recargar(0, 999900 + 100, &nuevo) == RECARGA_SUPERA_TOPE); // 6 cifras, mas que el tope
  VERIFICAR(saldo_recargar(SALDO_MAXIMO + 100, 100, &nuevo) == RECARGA_SUPERA_TOPE); // saldo ya fuera de rango
  VERIFICAR(saldo_recargar(0, 4294967200UL, &nuevo) == RECARGA_SUPERA_TOPE); // sin desborde de uint32
}

int main() {
  printf("=== prueba_saldo ===\n");
  prueba_formato_bytes();
  prueba_ida_y_vuelta();
  prueba_saldo_cero_no_es_blanco();
  prueba_en_blanco();
  prueba_corrupto();
  prueba_tope_al_leer();
  prueba_cobro();
  prueba_recarga();
  prueba_recarga_tope();
  return resumen("prueba_saldo");
}
