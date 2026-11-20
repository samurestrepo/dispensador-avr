// =====================================================================
// fsm.cpp — Máquina de estados del dispensador. NO BLOQUEANTE (D2).
//
// Cómo está organizada:
//   entrar(estado)  : acciones al ENTRAR a un estado (una sola vez) + dibujar.
//   dibujar()       : pinta en el LCD la pantalla del estado actual.
//   tick_xxx()      : lo que se revisa en CADA vuelta del loop en el estado xxx.
//   mensaje(...)    : muestra un texto unos segundos y después va a otro estado.
//                     Mientras dura, las teclas se descartan y las monedas esperan
//                     (monedas_leer() no se llama, así que ninguna se pierde).
// El LCD solo se redibuja cuando algo cambia, nunca en cada vuelta.
// Todos los tiempos se miden como (ahora - inicio >= duración), que sigue
// funcionando cuando millis() da la vuelta (cada ~49 días).
// =====================================================================
#include "config.h"
#include "lcd.h"
#include "teclado.h"
#include "monedas.h"
#include "reles.h"
#include "tarjeta.h"
#include "pedido.h"
#include "saldo.h"
#include "fsm.h"

// ======================= Variables de la máquina =======================
static Estado estado = INICIO;
static uint32_t t_estado = 0;              // cuándo se entró al estado actual (D14, GRACIAS)

// Mensaje temporal
static bool mensaje_activo = false;
static uint32_t t_mensaje = 0;
static uint32_t duracion_mensaje = 0;
static Estado estado_tras_mensaje = INICIO;

// Textos del LCD (16 caracteres + el 0 final)
static char linea1[17];
static char linea2[17];

// CONFIRMAR: cuál de las dos pantallas se ve y desde cuándo
static uint8_t pantalla_confirmar = 0;
static uint32_t t_alternar = 0;

// DISPENSAR (D12)
static uint8_t producto_sirviendo = PRODUCTO_AGUA;
static uint8_t unidad = 1;                 // unidad que se sirve, desde 1
static bool led_encendido = false;
static uint32_t t_led = 0;

// RECARGA_CLAVE y RECARGA_MONTO
static char clave[5];                      // 4 dígitos + 0 final
static uint8_t digitos_clave = 0;
static uint32_t monto_recarga = 0;
static uint8_t digitos_monto = 0;

// VERIFICAR_TARJETA (D8): lo necesario para decidir si la operación quedó
static uint8_t verif_uid[10];
static uint8_t verif_tam_uid = 0;
static uint32_t verif_original = 0;
static bool verif_original_en_blanco = false;   // recarga de una tarjeta nueva
static uint32_t verif_nuevo = 0;
static Estado verif_origen = PAGO_TARJETA;      // PAGO_TARJETA o RECARGA_TARJETA

// ======================= Textos =======================
// Se arman en linea1/linea2 y se muestran con lcd_pantalla(), que rellena con espacios.

static void texto_vaciar(char t[]) {
  t[0] = 0;
}

// Agrega s al final de t sin pasar de 16 caracteres.
static void texto_agregar(char t[], const char s[]) {
  uint8_t n = 0;
  while (t[n] != 0) {
    n++;
  }
  uint8_t i = 0;
  while (s[i] != 0 && n < 16) {
    t[n] = s[i];
    n++;
    i++;
  }
  t[n] = 0;
}

static void texto_agregar_letra(char t[], char letra) {
  char s[2] = {letra, 0};
  texto_agregar(t, s);
}

// Igual que lcd_dinero: las cifras salen al revés y se agregan al final.
static void texto_agregar_numero(char t[], uint32_t valor) {
  char cifras[10];
  uint8_t n = 0;
  do {
    cifras[n] = (char)('0' + (valor % 10));
    valor = valor / 10;
    n++;
  } while (valor > 0);
  while (n > 0) {
    n--;
    texto_agregar_letra(t, cifras[n]);
  }
}

static void texto_agregar_dinero(char t[], uint32_t valor) {
  texto_agregar(t, "$");
  texto_agregar_numero(t, valor);
}

// "$ 700": dinero alineado a 4 cifras (CONFIRMAR; un subtotal llega a 6300).
static void texto_agregar_dinero_4(char t[], uint32_t valor) {
  texto_agregar(t, "$");
  if (valor < 1000) texto_agregar(t, " ");
  if (valor < 100)  texto_agregar(t, " ");
  if (valor < 10)   texto_agregar(t, " ");
  texto_agregar_numero(t, valor);
}

static void mostrar(void) {
  lcd_pantalla(linea1, linea2);
}

static void poner_textos(const char a[], const char b[]) {
  texto_vaciar(linea1);
  texto_agregar(linea1, a);
  texto_vaciar(linea2);
  texto_agregar(linea2, b);
}

static bool textos_iguales(const char a[], const char b[]) {
  uint8_t i = 0;
  while (a[i] != 0 && a[i] == b[i]) {
    i++;
  }
  return a[i] == b[i];
}

// ======================= Mensajes temporales =======================
// Muestra linea1/linea2 (ya armadas) durante 'duracion' ms y después va a 'siguiente'.
// Si 'siguiente' es el mismo estado, solo se redibuja (no se repiten sus acciones de entrada).
static void mensaje(uint32_t ahora, uint32_t duracion, Estado siguiente) {
  mostrar();
  mensaje_activo = true;
  t_mensaje = ahora;
  duracion_mensaje = duracion;
  estado_tras_mensaje = siguiente;
}

static void mensaje_fijo(uint32_t ahora, const char a[], const char b[], uint32_t duracion, Estado siguiente) {
  poner_textos(a, b);
  mensaje(ahora, duracion, siguiente);
}

// "Saldo $X" en la línea 1 y lo que se pida en la 2.
static void mensaje_saldo(uint32_t ahora, uint32_t saldo, const char linea_2[], uint32_t duracion, Estado siguiente) {
  texto_vaciar(linea1);
  texto_agregar(linea1, "Saldo ");
  texto_agregar_dinero(linea1, saldo);
  texto_vaciar(linea2);
  texto_agregar(linea2, linea_2);
  mensaje(ahora, duracion, siguiente);
}

// "Tarjeta no valida" no cabe en 16 columnas: se parte en dos líneas.
static void mensaje_no_valida(uint32_t ahora, Estado siguiente) {
  mensaje_fijo(ahora, "Tarjeta", "no valida", TIEMPO_MENSAJE_MS, siguiente);
}

// ======================= Pantalla de cada estado =======================
static void dibujar(void) {
  texto_vaciar(linea1);
  texto_vaciar(linea2);
  switch (estado) {
    case INICIO:
      texto_agregar(linea1, "Dispensador");
      texto_agregar(linea2, "1:Agua 2:Gaseosa");
      break;

    case SELECCION:                                   // "Ag:2 Gs:1" / "Total $1700"
      texto_agregar(linea1, "Ag:");
      texto_agregar_numero(linea1, pedido_cantidad(PRODUCTO_AGUA));
      texto_agregar(linea1, " Gs:");
      texto_agregar_numero(linea1, pedido_cantidad(PRODUCTO_GASEOSA));
      texto_agregar(linea2, "Total ");
      texto_agregar_dinero(linea2, pedido_total());
      break;

    case CONFIRMAR:
      if (pantalla_confirmar == 0) {                  // "Ag 2x500=$1000" / "Gs 1x700=$ 700"
        texto_agregar(linea1, "Ag ");
        texto_agregar_numero(linea1, pedido_cantidad(PRODUCTO_AGUA));
        texto_agregar(linea1, "x");
        texto_agregar_numero(linea1, PRECIO_AGUA);
        texto_agregar(linea1, "=");
        texto_agregar_dinero_4(linea1, pedido_subtotal(PRODUCTO_AGUA));
        texto_agregar(linea2, "Gs ");
        texto_agregar_numero(linea2, pedido_cantidad(PRODUCTO_GASEOSA));
        texto_agregar(linea2, "x");
        texto_agregar_numero(linea2, PRECIO_GASEOSA);
        texto_agregar(linea2, "=");
        texto_agregar_dinero_4(linea2, pedido_subtotal(PRODUCTO_GASEOSA));
      } else {                                        // "Total $1700" / "#:Si *:Atras"
        texto_agregar(linea1, "Total ");
        texto_agregar_dinero(linea1, pedido_total());
        texto_agregar(linea2, "#:Si *:Atras");
      }
      break;

    case METODO_PAGO:
      texto_agregar(linea1, "A:Monedas");
      texto_agregar(linea2, "B:Tarjeta *:Atr");
      break;

    case PAGO_MONEDAS:                                // "Falta $1200" / "Pagado $500"
      texto_agregar(linea1, "Falta ");
      texto_agregar_dinero(linea1, pago_falta());
      texto_agregar(linea2, "Pagado ");
      texto_agregar_dinero(linea2, pago_pagado());
      break;

    case PAGO_TARJETA:
      texto_agregar(linea1, "Acerque tarjeta");
      texto_agregar(linea2, "Total ");
      texto_agregar_dinero(linea2, pedido_total());
      break;

    case DISPENSAR:                                   // "Sirviendo agua" / "Unidad 1 de 2"
      if (producto_sirviendo == PRODUCTO_AGUA) {
        texto_agregar(linea1, "Sirviendo agua");
      } else {
        texto_agregar(linea1, "Sirv. gaseosa");      // "Sirviendo gaseosa" tiene 17 letras
      }
      texto_agregar(linea2, "Unidad ");
      texto_agregar_numero(linea2, unidad);
      texto_agregar(linea2, " de ");
      texto_agregar_numero(linea2, pedido_cantidad(producto_sirviendo));
      break;

    case GRACIAS:
      texto_agregar(linea1, "Gracias!");
      texto_agregar(linea2, "Disfrute");
      break;

    case RECARGA_CLAVE:                               // un asterisco por dígito (D9)
      texto_agregar(linea1, "Clave admin:");
      for (uint8_t i = 0; i < digitos_clave; i++) {
        texto_agregar(linea2, "*");
      }
      break;

    case RECARGA_MONTO:                               // "Monto recarga:" / "$5000"
      texto_agregar(linea1, "Monto recarga:");
      texto_agregar(linea2, "$");
      if (digitos_monto > 0) {
        texto_agregar_numero(linea2, monto_recarga);
      }
      break;

    case RECARGA_TARJETA:
      texto_agregar(linea1, "Acerque tarjeta");
      texto_agregar(linea2, "Recarga ");
      texto_agregar_dinero(linea2, monto_recarga);
      break;

    case CONSULTA_SALDO:
      texto_agregar(linea1, "Acerque tarjeta");
      texto_agregar(linea2, "Consulta saldo");
      break;

    case VERIFICAR_TARJETA:
      texto_agregar(linea1, "Acerque de nuevo");
      texto_agregar(linea2, "la misma tarjeta");
      break;
  }
  mostrar();
}

// ======================= Entrar a un estado =======================
static void entrar(Estado nuevo, uint32_t ahora) {
  estado = nuevo;
  t_estado = ahora;

  switch (nuevo) {
    case INICIO:                        // INICIO = máquina sin pedido y con las salidas apagadas
      pedido_reiniciar();
      rele_apagar(PRODUCTO_AGUA);
      rele_apagar(PRODUCTO_GASEOSA);
      break;

    case CONFIRMAR:
      pantalla_confirmar = 0;
      t_alternar = ahora;
      break;

    case PAGO_MONEDAS:                  // D4: la "foto" de los switches se toma al ENTRAR
      pago_reiniciar();
      monedas_rearmar();
      break;

    case DISPENSAR:                     // D12: primero el agua, luego la gaseosa
      if (pedido_cantidad(PRODUCTO_AGUA) > 0) {
        producto_sirviendo = PRODUCTO_AGUA;
      } else {
        producto_sirviendo = PRODUCTO_GASEOSA;
      }
      unidad = 1;
      rele_encender(producto_sirviendo);
      led_encendido = true;
      t_led = ahora;
      break;

    case RECARGA_CLAVE:
      digitos_clave = 0;
      clave[0] = 0;
      break;

    case RECARGA_MONTO:
      monto_recarga = 0;
      digitos_monto = 0;
      break;

    default:
      break;
  }
  dibujar();
}

// ======================= D8: datos para VERIFICAR_TARJETA =======================
static void guardar_verificacion(uint32_t original, bool original_en_blanco,
                                 uint32_t nuevo, Estado origen) {
  verif_tam_uid = tarjeta_uid(verif_uid);
  verif_original = original;
  verif_original_en_blanco = original_en_blanco;
  verif_nuevo = nuevo;
  verif_origen = origen;
}

static bool misma_tarjeta(void) {
  uint8_t uid[10];
  uint8_t tam = tarjeta_uid(uid);
  if (tam != verif_tam_uid) {
    return false;
  }
  for (uint8_t i = 0; i < tam; i++) {
    if (uid[i] != verif_uid[i]) {
      return false;
    }
  }
  return true;
}

static bool es_digito(char tecla) {
  return tecla >= '0' && tecla <= '9';
}

// ======================= Lo que se revisa en cada estado =======================

static void tick_inicio(char tecla, uint32_t ahora) {
  if (tecla == '1') {
    pedido_agregar(PRODUCTO_AGUA);
    entrar(SELECCION, ahora);
  } else if (tecla == '2') {
    pedido_agregar(PRODUCTO_GASEOSA);
    entrar(SELECCION, ahora);
  } else if (tecla == 'C') {
    entrar(CONSULTA_SALDO, ahora);
  } else if (tecla == 'D') {
    entrar(RECARGA_CLAVE, ahora);
  }
}

static void tick_seleccion(char tecla, uint32_t ahora) {
  if (tecla == '1') {
    if (pedido_agregar(PRODUCTO_AGUA)) dibujar();          // en 9 no cambia: no se redibuja
  } else if (tecla == '2') {
    if (pedido_agregar(PRODUCTO_GASEOSA)) dibujar();
  } else if (tecla == '4') {
    if (pedido_quitar(PRODUCTO_AGUA)) dibujar();
  } else if (tecla == '5') {
    if (pedido_quitar(PRODUCTO_GASEOSA)) dibujar();
  } else if (tecla == '#') {
    if (pedido_total() > 0) entrar(CONFIRMAR, ahora);
  } else if (tecla == '*') {
    entrar(INICIO, ahora);                                 // INICIO borra el pedido
  }
}

static void tick_confirmar(char tecla, uint32_t ahora) {
  if (tecla == '#') {
    entrar(METODO_PAGO, ahora);
    return;
  }
  if (tecla == '*') {
    entrar(SELECCION, ahora);
    return;
  }
  if (tecla == '4' || tecla == '5') {
    uint8_t producto = PRODUCTO_AGUA;
    if (tecla == '5') {
      producto = PRODUCTO_GASEOSA;
    }
    if (pedido_quitar(producto)) {
      if (pedido_total() == 0) {
        entrar(INICIO, ahora);                             // quedó vacío
        return;
      }
      dibujar();
    }
  }
  // Alterna las dos pantallas cada 2 s.
  if (ahora - t_alternar >= TIEMPO_ALTERNAR_MS) {
    pantalla_confirmar = 1 - pantalla_confirmar;
    t_alternar = ahora;
    dibujar();
  }
}

static void tick_metodo_pago(char tecla, uint32_t ahora) {
  if (tecla == 'A') {
    entrar(PAGO_MONEDAS, ahora);
  } else if (tecla == 'B') {
    entrar(PAGO_TARJETA, ahora);
  } else if (tecla == '*') {
    entrar(CONFIRMAR, ahora);
  }
}

static void tick_pago_monedas(char tecla, uint32_t ahora) {
  if (tecla == '*') {                                      // D13: cancelar y devolver lo ingresado
    if (pago_pagado() > 0) {
      texto_vaciar(linea1);
      texto_agregar(linea1, "Devolviendo");
      texto_vaciar(linea2);
      texto_agregar_dinero(linea2, pago_pagado());
      mensaje(ahora, TIEMPO_MENSAJE_MS, INICIO);
    } else {
      entrar(INICIO, ahora);                               // no había nada que devolver
    }
    return;
  }

  uint16_t moneda = monedas_leer();                        // D5: solo en este estado
  if (moneda == 0) {
    return;
  }
  ResultadoPago resultado = pago_insertar_moneda(moneda);  // R7
  if (resultado == PAGO_ACEPTADA) {
    dibujar();
  } else if (resultado == PAGO_DEVUELTA) {                 // "Devuelta $500" / "Ingrese $200"
    texto_vaciar(linea1);
    texto_agregar(linea1, "Devuelta ");
    texto_agregar_dinero(linea1, moneda);
    texto_vaciar(linea2);
    texto_agregar(linea2, "Ingrese ");
    texto_agregar_dinero(linea2, pago_falta());
    mensaje(ahora, TIEMPO_MENSAJE_MS, PAGO_MONEDAS);
  } else {                                                 // PAGO_COMPLETO
    entrar(DISPENSAR, ahora);
  }
}

static void tick_pago_tarjeta(char tecla, uint32_t ahora) {
  if (tecla == '*' || ahora - t_estado >= TIEMPO_TARJETA_MS) {   // D14
    entrar(METODO_PAGO, ahora);
    return;
  }
  if (!tarjeta_presente()) {
    return;
  }

  uint32_t saldo = 0;
  ResultadoTarjeta lectura = tarjeta_leer_saldo(&saldo);
  if (lectura == TARJETA_OK || lectura == TARJETA_EN_BLANCO) {   // en blanco = saldo $0
    uint32_t nuevo = 0;
    if (!saldo_cobrar(saldo, pedido_total(), &nuevo)) {          // R8: rechazo
      texto_vaciar(linea1);
      texto_agregar(linea1, "Saldo insuf.");
      texto_vaciar(linea2);
      texto_agregar(linea2, "Saldo ");
      texto_agregar_dinero(linea2, saldo);
      mensaje(ahora, TIEMPO_MENSAJE_MS, METODO_PAGO);
    } else {
      ResultadoTarjeta escritura = tarjeta_escribir_saldo(nuevo);  // escribe, relee, compara (D8)
      if (escritura == TARJETA_OK) {
        mensaje_saldo(ahora, nuevo, "", TIEMPO_MENSAJE_MS, DISPENSAR);
      } else {
        // D8: no se sabe si quedó. NO se reintenta a ciegas.
        guardar_verificacion(saldo, false, nuevo, PAGO_TARJETA);
        tarjeta_terminar();
        entrar(VERIFICAR_TARJETA, ahora);
        return;
      }
    }
  } else if (lectura == TARJETA_NO_VALIDA) {
    mensaje_no_valida(ahora, METODO_PAGO);
  } else if (lectura == TARJETA_NO_SOPORTADA) {
    mensaje_fijo(ahora, "No soportada", "Use MIFARE 1K", TIEMPO_MENSAJE_MS, METODO_PAGO);
  } else {                                                       // no se pudo leer: nada se escribió
    mensaje_fijo(ahora, "Error lectura", "Intente de nuevo", TIEMPO_MENSAJE_MS, PAGO_TARJETA);
  }
  tarjeta_terminar();                                            // cierra la operación
}

static void tick_dispensar(uint32_t ahora) {
  if (led_encendido) {
    if (ahora - t_led >= TIEMPO_LED_ON_MS) {           // 2 s encendido
      rele_apagar(producto_sirviendo);
      led_encendido = false;
      t_led = ahora;
    }
    return;
  }
  if (ahora - t_led < TIEMPO_LED_OFF_MS) {             // 0,5 s apagado
    return;
  }
  // Siguiente unidad
  unidad++;
  if (unidad > pedido_cantidad(producto_sirviendo)) {
    if (producto_sirviendo == PRODUCTO_AGUA && pedido_cantidad(PRODUCTO_GASEOSA) > 0) {
      producto_sirviendo = PRODUCTO_GASEOSA;
      unidad = 1;
    } else {
      entrar(GRACIAS, ahora);
      return;
    }
  }
  rele_encender(producto_sirviendo);
  led_encendido = true;
  t_led = ahora;
  dibujar();
}

static void tick_gracias(uint32_t ahora) {
  if (ahora - t_estado >= TIEMPO_MENSAJE_MS) {
    entrar(INICIO, ahora);                             // INICIO deja el pedido en 0
  }
}

static void tick_recarga_clave(char tecla, uint32_t ahora) {
  if (es_digito(tecla)) {
    if (digitos_clave < 4) {
      clave[digitos_clave] = tecla;
      digitos_clave++;
      clave[digitos_clave] = 0;
      dibujar();
    }
  } else if (tecla == '#') {
    if (digitos_clave == 4 && textos_iguales(clave, CLAVE_ADMIN)) {
      entrar(RECARGA_MONTO, ahora);
    } else {
      mensaje_fijo(ahora, "Clave errada", "", TIEMPO_MENSAJE_MS, INICIO);
    }
  } else if (tecla == '*') {
    entrar(INICIO, ahora);
  }
}

static void tick_recarga_monto(char tecla, uint32_t ahora) {
  if (es_digito(tecla)) {
    bool cero_a_la_izquierda = (digitos_monto == 0 && tecla == '0');
    if (digitos_monto < RECARGA_MAX_DIGITOS && !cero_a_la_izquierda) {
      monto_recarga = monto_recarga * 10 + (uint32_t)(tecla - '0');
      digitos_monto++;
      dibujar();
    }
  } else if (tecla == '*') {                           // borra un dígito; vacío -> INICIO
    if (digitos_monto == 0) {
      entrar(INICIO, ahora);
    } else {
      monto_recarga = monto_recarga / 10;
      digitos_monto--;
      dibujar();
    }
  } else if (tecla == '#') {                           // D10: > 0 y múltiplo de 100
    if (monto_recarga > 0 && monto_recarga % 100 == 0) {
      entrar(RECARGA_TARJETA, ahora);
    } else {
      mensaje_fijo(ahora, "Monto no valido", "Multiplo de 100", TIEMPO_MENSAJE_MS, RECARGA_MONTO);
    }
  }
}

static void tick_recarga_tarjeta(char tecla, uint32_t ahora) {
  if (tecla == '*' || ahora - t_estado >= TIEMPO_TARJETA_MS) {   // D14
    entrar(INICIO, ahora);
    return;
  }
  if (!tarjeta_presente()) {
    return;
  }

  uint32_t saldo = 0;
  ResultadoTarjeta lectura = tarjeta_leer_saldo(&saldo);
  if (lectura == TARJETA_OK || lectura == TARJETA_EN_BLANCO) {
    bool en_blanco = (lectura == TARJETA_EN_BLANCO);             // tarjeta nueva: saldo = monto
    uint32_t nuevo = 0;
    if (saldo_recargar(saldo, monto_recarga, &nuevo) != RECARGA_OK) {   // D10: tope
      texto_vaciar(linea1);
      texto_agregar(linea1, "Supera el tope");
      texto_vaciar(linea2);
      texto_agregar(linea2, "Saldo ");
      texto_agregar_dinero(linea2, saldo);
      mensaje(ahora, TIEMPO_MENSAJE_MS, INICIO);
    } else {
      ResultadoTarjeta escritura = tarjeta_escribir_saldo(nuevo);
      if (escritura == TARJETA_OK) {
        if (en_blanco) {
          texto_vaciar(linea1);
          texto_agregar(linea1, "Tarjeta nueva");
          texto_vaciar(linea2);
          texto_agregar(linea2, "Saldo ");
          texto_agregar_dinero(linea2, nuevo);
          mensaje(ahora, TIEMPO_MENSAJE_MS, INICIO);
        } else {
          mensaje_saldo(ahora, nuevo, "", TIEMPO_MENSAJE_MS, INICIO);
        }
      } else {
        guardar_verificacion(saldo, en_blanco, nuevo, RECARGA_TARJETA);   // D8
        tarjeta_terminar();
        entrar(VERIFICAR_TARJETA, ahora);
        return;
      }
    }
  } else if (lectura == TARJETA_NO_VALIDA) {
    mensaje_no_valida(ahora, INICIO);
  } else if (lectura == TARJETA_NO_SOPORTADA) {
    mensaje_fijo(ahora, "No soportada", "Use MIFARE 1K", TIEMPO_MENSAJE_MS, INICIO);
  } else {
    mensaje_fijo(ahora, "Error lectura", "Intente de nuevo", TIEMPO_MENSAJE_MS, RECARGA_TARJETA);
  }
  tarjeta_terminar();
}

static void tick_consulta_saldo(char tecla, uint32_t ahora) {
  if (tecla == '*' || ahora - t_estado >= TIEMPO_TARJETA_MS) {   // D14
    entrar(INICIO, ahora);
    return;
  }
  if (!tarjeta_presente()) {
    return;
  }

  uint32_t saldo = 0;                                            // D16: SOLO LECTURA
  ResultadoTarjeta lectura = tarjeta_leer_saldo(&saldo);
  if (lectura == TARJETA_OK || lectura == TARJETA_EN_BLANCO) {   // en blanco -> "Saldo $0"
    mensaje_saldo(ahora, saldo, "", TIEMPO_CONSULTA_MS, INICIO);
  } else if (lectura == TARJETA_NO_VALIDA) {
    mensaje_no_valida(ahora, INICIO);
  } else if (lectura == TARJETA_NO_SOPORTADA) {
    mensaje_fijo(ahora, "No soportada", "Use MIFARE 1K", TIEMPO_MENSAJE_MS, INICIO);
  } else {
    mensaje_fijo(ahora, "Error lectura", "Intente de nuevo", TIEMPO_MENSAJE_MS, CONSULTA_SALDO);
  }
  tarjeta_terminar();
}

// D8. Sin tiempo límite: si el cobro ya quedó, volver al menú haría perder el dinero.
static void tick_verificar_tarjeta(char tecla, uint32_t ahora) {
  if (tecla == '*') {
    mensaje_fijo(ahora, "Operacion no", "confirmada", TIEMPO_MENSAJE_MS, INICIO);
    return;
  }
  if (!tarjeta_despertar()) {          // WUPA: también encuentra la tarjeta si quedó dormida
    return;
  }
  if (!misma_tarjeta()) {
    tarjeta_terminar();
    mensaje_fijo(ahora, "Otra tarjeta", "", TIEMPO_MENSAJE_MS, VERIFICAR_TARJETA);
    return;
  }

  uint32_t saldo = 0;
  ResultadoTarjeta lectura = tarjeta_leer_saldo(&saldo);
  bool es_el_nuevo = (lectura == TARJETA_OK && saldo == verif_nuevo);
  bool es_el_original;
  if (verif_original_en_blanco) {
    es_el_original = (lectura == TARJETA_EN_BLANCO);
  } else {
    es_el_original = (lectura == TARJETA_OK && saldo == verif_original);
  }

  if (es_el_nuevo) {                                   // la operación SÍ quedó
    if (verif_origen == PAGO_TARJETA) {
      mensaje_saldo(ahora, saldo, "", TIEMPO_MENSAJE_MS, DISPENSAR);
    } else {
      mensaje_saldo(ahora, saldo, "", TIEMPO_MENSAJE_MS, INICIO);
    }
  } else if (es_el_original) {                         // NO quedó: se puede reintentar sin riesgo
    if (verif_origen == PAGO_TARJETA) {
      mensaje_fijo(ahora, "No se cobro", "", TIEMPO_MENSAJE_MS, PAGO_TARJETA);
    } else {
      mensaje_fijo(ahora, "No se recargo", "", TIEMPO_MENSAJE_MS, RECARGA_TARJETA);
    }
  } else if (lectura == TARJETA_OK) {                  // válida, pero ni el original ni el nuevo
    mensaje_fijo(ahora, "Operacion no", "confirmada", TIEMPO_MENSAJE_MS, INICIO);
  } else if (lectura == TARJETA_NO_VALIDA || lectura == TARJETA_EN_BLANCO) {
    mensaje_no_valida(ahora, INICIO);
  } else {                                             // no se pudo leer: se sigue esperando
    mensaje_fijo(ahora, "Error lectura", "Acerque de nuevo", TIEMPO_MENSAJE_MS, VERIFICAR_TARJETA);
  }
  tarjeta_terminar();
}

// ======================= Interfaz pública =======================

void fsm_iniciar(uint32_t ahora) {
  mensaje_activo = false;
  entrar(INICIO, ahora);
}

void fsm_tick(uint32_t ahora) {
  // El teclado se lee en todas las vueltas (su antirrebote depende de eso),
  // aunque durante un mensaje la tecla se descarte.
  char tecla = teclado_leer();

  if (mensaje_activo) {
    if (ahora - t_mensaje >= duracion_mensaje) {
      mensaje_activo = false;
      if (estado_tras_mensaje == estado) {
        dibujar();                                     // mismo estado: solo se repinta
      } else {
        entrar(estado_tras_mensaje, ahora);
      }
    }
    return;
  }

  switch (estado) {
    case INICIO:            tick_inicio(tecla, ahora);            break;
    case SELECCION:         tick_seleccion(tecla, ahora);         break;
    case CONFIRMAR:         tick_confirmar(tecla, ahora);         break;
    case METODO_PAGO:       tick_metodo_pago(tecla, ahora);       break;
    case PAGO_MONEDAS:      tick_pago_monedas(tecla, ahora);      break;
    case PAGO_TARJETA:      tick_pago_tarjeta(tecla, ahora);      break;
    case DISPENSAR:         tick_dispensar(ahora);                break;
    case GRACIAS:           tick_gracias(ahora);                  break;
    case RECARGA_CLAVE:     tick_recarga_clave(tecla, ahora);     break;
    case RECARGA_MONTO:     tick_recarga_monto(tecla, ahora);     break;
    case RECARGA_TARJETA:   tick_recarga_tarjeta(tecla, ahora);   break;
    case CONSULTA_SALDO:    tick_consulta_saldo(tecla, ahora);    break;
    case VERIFICAR_TARJETA: tick_verificar_tarjeta(tecla, ahora); break;
  }
}

Estado fsm_estado(void) {
  return estado;
}

bool fsm_mostrando_mensaje(void) {
  return mensaje_activo;
}
