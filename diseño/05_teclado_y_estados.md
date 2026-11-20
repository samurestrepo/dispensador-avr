# 05 — Teclado y máquina de estados

## Teclado
```
 1  2  3  A        1 = + agua        4 = − agua (cancelar 1)
 4  5  6  B        2 = + gaseosa     5 = − gaseosa (cancelar 1)
 7  8  9  C        # = confirmar / aceptar
 *  0  #  D        * = volver / cancelar todo
                   A = pagar con monedas   B = pagar con tarjeta
                   C = consultar saldo (desde INICIO, D16)
                   D = modo recarga (desde INICIO)
                   0–9 = dígitos (clave y monto de recarga)
```

## Diagrama
```
                 1/2                     #                      A
  INICIO ───────────────► SELECCION ──────────► CONFIRMAR ─────────► METODO_PAGO ──► PAGO_MONEDAS ──┐
    ▲ │                     │  ▲  *(todo)          │ 4/5, *           │ B                            │ pagado = total
    │ │ D                   │  └───────────────────┘                  ▼                              ▼
    │ ▼                     └──────────► INICIO                  PAGO_TARJETA ── saldo OK ──►  DISPENSAR
  RECARGA_CLAVE ─ok─► RECARGA_MONTO ─#─► RECARGA_TARJETA                │ saldo insuficiente          │
    │ clave mala            │ *                 │ ok / error            ▼                              ▼
    └──► INICIO ◄───────────┴───────────────────┘                  METODO_PAGO                    GRACIAS ──► INICIO
```
Además, desde INICIO: `C ─► CONSULTA_SALDO ─(3 s · * · 20 s)─► INICIO` (D16, solo lectura).

## Tabla de estados
| Estado | LCD (línea 1 / línea 2) | Entradas → acción |
|---|---|---|
| INICIO | `Dispensador` / `1:Agua 2:Gaseosa` | 1/2 → +1 y SELECCION · C → CONSULTA_SALDO · D → RECARGA_CLAVE |
| SELECCION | `Ag:2 Gs:1` / `Total $1700` | 1/2 = +1 (máx. 9) · 4/5 = −1 · # (total > 0) → CONFIRMAR · * → borrar todo → INICIO |
| CONFIRMAR | Alterna cada 2 s: `Ag 2x500=$1000` / `Gs 1x700=$ 700` ↔ `Total $1700` / `#:Si *:Atras` | # → METODO_PAGO · 4/5 = −1 (si queda en 0 → INICIO) · * → SELECCION |
| METODO_PAGO | `A:Monedas` / `B:Tarjeta *:Atr` | A → PAGO_MONEDAS (foto de switches) · B → PAGO_TARJETA · * → CONFIRMAR |
| PAGO_MONEDAS | `Falta $1200` / `Pagado $500` | Moneda aceptada → suma · se pasa → `Devuelta $500` / `Ingrese $X` (2 s) · pagado = total → DISPENSAR · * → `Devolviendo $X` → INICIO |
| PAGO_TARJETA | `Acerque tarjeta` / `Total $1700` | Saldo ≥ total → escribir, verificar, `Saldo $X` → DISPENSAR · saldo < total → `Saldo insuf.` / `Saldo $X` (2 s) → METODO_PAGO · no válida → mensaje → METODO_PAGO · * o 20 s → METODO_PAGO |
| DISPENSAR | `Sirviendo agua` / `Unidad 1 de 2` | Secuencia de D12; al final → GRACIAS |
| GRACIAS | `Gracias!` / `Disfrute` (2 s) | → INICIO (pedido en 0) |
| RECARGA_CLAVE | `Clave admin:` / `****` | 4 dígitos + # → bien: RECARGA_MONTO · mal: `Clave errada` → INICIO · * → INICIO |
| RECARGA_MONTO | `Monto recarga:` / `$5000` | 0–9 (máx. 6) · * borra un dígito (vacío → INICIO) · # (> 0 y múltiplo de 100) → RECARGA_TARJETA |
| RECARGA_TARJETA | `Acerque tarjeta` / `Recarga $5000` | Saldo válido → saldo + monto · en blanco → `Tarjeta nueva`, saldo = monto · tope $999.900 · escribir, verificar → `Saldo $X` → INICIO · * o 20 s → INICIO |
| CONSULTA_SALDO | `Acerque tarjeta` / `Consulta saldo` | Válida → `Saldo $X` (3 s) → INICIO · en blanco → `Saldo $0` (3 s) → INICIO · no válida → `Tarjeta no valida` (2 s) → INICIO · * o 20 s → INICIO. **Solo lectura (D16).** |
| VERIFICAR_TARJETA | `Acerque de nuevo` / `la misma tarjeta` | Se llega desde PAGO_TARJETA o RECARGA_TARJETA cuando falla la escritura o la verificación (D8). Tarjeta con **otro UID** → `Otra tarjeta` (2 s) y sigue esperando · mismo UID: saldo = nuevo → (pago) `Saldo $X` → DISPENSAR / (recarga) `Saldo $X` → INICIO · saldo = original → `No se cobro` / `No se recargo` (2 s) → vuelve al estado de origen para reintentar · no válida → `Tarjeta no valida` → INICIO · * → `Op. no confirmada` (2 s) → INICIO. **Sin tiempo límite.** |

## Regla de las monedas (R7)
```
si (pagado + moneda > total):  devolver moneda; mostrar "Ingrese $(total − pagado)"
si no:                         pagado += moneda
si (pagado == total):          ir a DISPENSAR
```

## Pago con tarjeta (D7, D8)
```
leer bloque 4 → saldo, comp
si saldo != ~comp:                  "Tarjeta no valida"
si saldo < total:                   "Saldo insuficiente"   (rechazo, R8)
nuevo = saldo − total
escribir bloque 4 (nuevo, ~nuevo) → releer → ¿igual?
   sí → DISPENSAR
   no (o falló la escritura) → VERIFICAR_TARJETA   (guardar UID, original y nuevo)
```

## Verificación tras una escritura dudosa (D8)
```
pedir la misma tarjeta (comparar UID)
leer saldo:
  == nuevo     → la operación SÍ quedó: pago → DISPENSAR · recarga → confirmar
  == original  → NO quedó: volver al estado de origen y reintentar (sin riesgo)
  no válida    → "Tarjeta no valida" → INICIO
* → "Op. no confirmada" → INICIO   (sin tiempo límite)
```
