# TP4 — Sistema de Clasificación de Paquetes por Altura

**Materia:** Microcontroladores  
**Carrera:** Ingeniería en Mecatrónica — UNER  
**Grupo:** Alexander Medrano / Juan Ignacio  
**Microcontrolador:** ATmega328P (Arduino Uno)

---

## Descripción del sistema

Sistema automatizado de clasificación de cajas en una cinta transportadora. El sistema mide la altura de cada caja con un sensor ultrasónico HC-SR04, la clasifica en tres categorías (pequeña, mediana, grande) y activa el servomotor SG90 correspondiente para eyectarla hacia el canal correcto cuando detecta su posición mediante sensores infrarrojos TCRT5000.

---

## Hardware utilizado

| Componente | Pin/Puerto | Función |
|------------|-----------|---------|
| HC-SR04 TRIG | PB1 | Disparo ultrasónico |
| HC-SR04 ECHO | PB2 | Recepción del eco |
| TCRT5000 IR0 (S0) | PD2 | Detección de caja en zona de medición |
| TCRT5000 IR1 (S1) | PD3 | Posición del pateador 1 |
| TCRT5000 IR2 (S2) | PD4 | Posición del pateador 2 |
| TCRT5000 IR3 (S3) | PD5 | Posición del pateador 3 |
| SG90 SERVO1 | PD7 | Eyector caja pequeña |
| SG90 SERVO2 | PB4 | Eyector caja mediana |
| SG90 SERVO3 | PB3 | Eyector caja grande |
| LED builtin | PB5 | Heartbeat (parpadeo cada 100ms) |

---

## Especificaciones de las cajas

| Tipo | Altura (h) | Ancho (w) | Espesor (t) |
|------|-----------|-----------|-------------|
| Pequeña | 6 cm | 10 cm | 3 cm |
| Mediana | 8 cm | 10 cm | 3 cm |
| Grande | 10 cm | 10 cm | 3 cm |

---

## Estructura del proyecto

```
TP4 Microcontroladores/
├── Actividad Nº 4.pdf          <- Consigna completa
├── SG90.PDF                    <- Datasheet servomotor
├── HC-SR04-Ultrasonic.pdf      <- Datasheet sensor ultrasónico
├── tcrt5000.pdf                <- Datasheet sensor infrarrojo
├── A000066-full-pinout.pdf     <- Pinout Arduino Uno
├── ATmega48A-...-DS40002061A.pdf <- Datasheet ATmega328P
├── README.md                   <- Este archivo
└── Actividad 4/
    └── Ejercicio/
        └── Ejercicio/
            ├── main.c          <- Código principal (máquina de estados)
            ├── TIMERS.c/h      <- Driver de timers (Timer0, Timer1, Timer2)
            ├── HCSR04.c/h      <- Driver del sensor ultrasónico HC-SR04
            ├── COMUNICATION.c/h <- Driver USART / Protocolo UNER
            ├── SERVO.c/h       <- Driver software PWM para servos SG90
            ├── IR.c/h          <- Driver sensores infrarrojos TCRT5000
            ├── CLASSIFIER.c/h  <- Lógica de clasificación de cajas por altura
            └── Debug/          <- Binarios compilados (.hex, .elf)
```

---

## Arquitectura del firmware

### Timers

| Timer | Modo | Período | Uso |
|-------|------|---------|-----|
| Timer0 | CTC | 1 ms | Tick del sistema (flag GPIOR00) |
| Timer1 | CTC | 1 ms | Software PWM de los tres servos |
| Timer2 | CTC | 1 µs | Temporización interna del HC-SR04 |

### Protocolo de comunicación (UART)

USART0 a 115200 baud, 8N1. Protocolo UNER:

```
| 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
```

| Comando | Descripción | Payload |
|---------|-------------|---------|
| 0xA1 | Medición de distancia | 1 byte (cm) |
| 0xA0 | Error del sensor | Sin payload |

### Máquina de estados (main.c)

```
           ┌─────────────────────────────┐
           │                             │
           ▼                             │
       CS_MEASURE                        │
    (IR0 + HCSR04)                       │
           │                             │
    ┌──────┼──────┐                      │
    │      │      │                      │
    ▼      ▼      ▼                      │
 PUSHER1 PUSHER2 PUSHER3                 │
 (IR1)   (IR2)   (IR3)                   │
    │      │      │                      │
    └──────┴──────┘                      │
           │ 500 ms                      │
           └─────────────────────────────┘
```

### Software PWM (SERVO.c)

- Señal: 50 Hz (período 20 ms), generada por software en `ISR(TIMER1_COMPA_vect)`
- Pulso 1 ms → posición HOME (retraído / 0°)
- Pulso 2 ms → posición PUSH (extendido / 180°)
- Los tres servos se manejan con un contador compartido de 0–19 ms

---

## Historial por fecha

---

### 01/05/2026 — Inicio del proyecto (Alexander Medrano)

**Creación del proyecto base en Microchip Studio.**

Implementaciones realizadas:

- **`TIMERS.c / TIMERS.h`** — Configuración de Timer0 (1 ms, con ISR), Timer1 (1 ms, con ISR vacío) y Timer2 (1 µs sin ISR, para HC-SR04).
- **`HCSR04.c / HCSR04.h`** — Driver completo y no bloqueante para el sensor ultrasónico HC-SR04. Implementa una máquina de estados interna (IDLE → TRIG → WAIT_ECHO_HIGH → WAIT_ECHO_LOW → DONE/TIMEOUT). Usa punteros a función para abstraer el hardware (portable).
- **`COMUNICATION.c / COMUNICATION.h`** — Implementación del Protocolo UNER sobre USART0 a 115200 baud. Buffers circulares de 128 bytes para TX/RX. Recepción por ISR, transmisión por polling.
- **`main.c` (base)** — Estructura inicial con:
  - Enumeración `_eConveyorState` (CS_MEASURE, CS_PUSHER1, CS_PUSHER2, CS_PUSHER3)
  - Struct `_sBoxs` con las dimensiones de las tres cajas
  - Inicialización de GPIOs (pines de servos, IRs, trigger/echo, LED)
  - Heartbeat LED en PB5 (toggle cada 100 ms)
  - Estado CS_MEASURE funcional: medición continua con HC-SR04 y envío por USART (comando 0xA1 con distancia, 0xA0 en error)
  - ISR de Timer1 vacío (pendiente de uso)
  - Función `decodeCMD()` vacía (pendiente de implementación)

**Estado al cierre:** medición y comunicación funcionando. Sin lógica de clasificación ni control de servos.

---

### 15/05/2026 — Planificación e implementación (Juan Ignacio)

#### Planificación

Análisis del código existente y definición del plan de trabajo:

1. El ISR de Timer1 estaba configurado pero vacío y `ini_TIMER1()` no era llamado en `main()`, por lo que el timer nunca corría.
2. Los pines de los servos (PD7, PB4, PB3) no tienen salida PWM hardware disponible (Timer2 ya está ocupado por HC-SR04). Se decidió usar **software PWM** en el ISR de Timer1.
3. Con resolución de 1 ms por tick y período de 20 ms, se obtienen dos posiciones exactas (1 ms y 2 ms), suficientes para la operación home/push.
4. La clasificación se realiza convirtiendo la distancia medida en altura de caja (`h = REFERENCE_DIST_CM - d_cm`) y comparando con los umbrales del struct `_sBoxs`.
5. Los estados CS_PUSHER1/2/3 esperan a que el IR correspondiente detecte la caja, activan el servo 500 ms y regresan a CS_MEASURE.

#### Implementaciones realizadas

**Nuevo archivo: `SERVO.h`**

API pública del driver de servos:

```c
typedef enum { SERVO_HOME = 1, SERVO_PUSH = 2 } _eServoPos;
typedef enum { SERVO_ID_1 = 0, SERVO_ID_2 = 1, SERVO_ID_3 = 2 } _eServoID;

void SERVO_Init(void);
void SERVO_Set(_eServoID id, _eServoPos pos);
void SERVO_On1ms(void);
```

**Nuevo archivo: `SERVO.c`**

Driver de software PWM para tres servos SG90:

- Contador `pwm_tick` de 0 a 19 (20 ms de período a 50 Hz)
- Array `pulse_width[3]` con el ancho de pulso en ms de cada servo
- En cada tick: si `pwm_tick == 0` → pin HIGH; si `pwm_tick == pulse_width[id]` → pin LOW
- Resultado: señal de 50 Hz con pulso de 1 ms (HOME) o 2 ms (PUSH) en cada servo

**Modificaciones en `main.c`**

| Cambio | Descripción |
|--------|-------------|
| `#include "SERVO.h"` | Inclusión del nuevo módulo |
| `#define REFERENCE_DIST_CM 20` | Distancia de referencia sensor→transportador (ajustar en banco) |
| `#define SERVO_HOLD_MS 500` | Tiempo en ms que el servo permanece en posición PUSH |
| `#define IR_DETECTED(pin)` | Macro para leer IR: `!(PIND & (1 << pin))` — activo bajo |
| `classify_box()` prototipo | Declaración anticipada de la función |
| `servo_hold_timer` (uint16_t) | Contador regresivo del tiempo de acción del servo |
| `servo_active` (uint8_t) | Flag para saber si el servo está en posición PUSH |
| `detected_type` (uint8_t) | Almacena el tipo de caja clasificada (1/2/3) |
| `ISR(TIMER1_COMPA_vect)` | Ahora llama a `SERVO_On1ms()` en cada tick de 1 ms |
| `On1ms()` | Agrega decremento de `servo_hold_timer` |
| `ini_TIMER1()` en `main()` | Faltaba esta llamada — sin ella el Timer1 nunca corría |
| `SERVO_Init()` en `main()` | Inicializa los tres servos en posición HOME |
| `classify_box(d_cm)` | Convierte distancia → tipo de caja usando `REFERENCE_DIST_CM` |
| CS_MEASURE (ampliado) | Agrega detección de IR0 y transición al estado de pusher correcto |
| CS_PUSHER1 | Espera IR1, activa SERVO1 por 500 ms, retorna a CS_MEASURE |
| CS_PUSHER2 | Espera IR2, activa SERVO2 por 500 ms, retorna a CS_MEASURE |
| CS_PUSHER3 | Espera IR3, activa SERVO3 por 500 ms, retorna a CS_MEASURE |

---

### 15/05/2026 (continuación) — Simulación de HC-SR04 con botones y depuración (Juan Ignacio)

#### Problema: sin HC-SR04 disponible en el banco

Se agregó simulación del sensor HC-SR04 usando dos botones físicos:

| Botón | Pin | Simula |
|-------|-----|--------|
| BTN_SMALL | PD6 (Arduino D6) | Caja pequeña → SERVO1 |
| BTN_MEDIUM | PB0 (Arduino D8) | Caja mediana → SERVO2 |

Ambos botones usan **resistencia pull-down externa** (activo alto: presionar conecta el pin a 5V).

#### Bugs encontrados y corregidos

| Bug | Causa | Fix |
|-----|-------|-----|
| Servo se movía al flashear y luego no respondía | `SERVO_Set(SERVO_ID_1, SERVO_PUSH)` dejado de un test de diagnóstico | Removido |
| Falsa transición de estado en CS_MEASURE | ECHO (PB2) flotante → HC-SR04 detectaba eco inmediatamente; IR0 (PD2) flotante → `IR_DETECTED` siempre true | `PORTB |= (1 << ECHO)` y `PORTD |= IR0|IR1|IR2|IR3` (pull-ups internos) |
| Lógica de botones en CS_MEASURE nunca ejecutada | El bloque `if (!servo_active)` tenía `#define` dentro y sin código real | Reemplazado con `if (BTN_SMALL_PRESSED)` / `else if (BTN_MEDIUM_PRESSED)` que activan servo y transicionan al pusher correcto |

#### Flujo final de simulación con botón

```
Presionar BTN_SMALL (D6):
  → CS_MEASURE detecta presión
  → SERVO_Set(SERVO_ID_1, SERVO_PUSH)  ← servo va a 180° inmediatamente
  → servo_hold_timer = 500ms
  → servo_active = TRUE
  → state = CS_PUSHER1

CS_PUSHER1 (durante 500ms):
  → servo sigue en PUSH mientras corre el timer

Al expirar el timer:
  → SERVO_Set(SERVO_ID_1, SERVO_HOME)  ← servo vuelve a 0°
  → servo_active = FALSE
  → state = CS_MEASURE
```

#### Cambios en `main.c`

| Cambio | Descripción |
|--------|-------------|
| `BTN_SMALL` / `BTN_MEDIUM` defines | PD6 y PB0 como entradas sin pull-up interno |
| `ini_GPIOs()` | Pull-up en ECHO y en IR0-IR3 para evitar flotación |
| `ini_TIMER1()` | Faltaba esta llamada (Timer1 nunca corría) — agregada |
| Bloque simulación en CS_MEASURE | Reemplazó código roto con lógica correcta de botón → pusher |
| TEST DIRECTO eliminado | Código de diagnóstico temporal removido del loop principal |

---

### 15/05/2026 ~18:30 — Simulación HC-SR04 con botones: flujo 2 fases (Juan Ignacio)

#### Motivación

La implementación anterior activaba el servo **directamente desde CS_MEASURE** al presionar el botón, salteando la lógica de detección IR en CS_PUSHER. El objetivo es que el botón solo simule la medición del HC-SR04 (la caja pasa por la zona de medición), y que el IR físico en la posición del pateador sea quien dispare el servo.

#### Flujo actual con botones

```
Presionar BTN_SMALL (D6) — simula HC-SR04 + IR0:
  → detected_type = 1 (caja pequeña)
  → state = CS_PUSHER1   ← servo quieto todavía

CS_PUSHER1: espera IR1 físico (PD3):
  → IR_IsDetected(IR_ID_1) == true
  → SERVO_Set(SERVO_ID_1, SERVO_PUSH)
  → 500 ms → HOME automático → CS_MEASURE
```

#### Cambios en `main.c`

| Cambio | Descripción |
|--------|-------------|
| CS_MEASURE bloque botones | Solo asigna `detected_type` y cambia `state`; el servo no se toca |
| CS_PUSHER1/2/3 | Sin cambios — esperan el IR físico real para accionar |

---

### 15/05/2026 ~19:00 — Driver IR.c/h para sensores TCRT5000 (Juan Ignacio)

**Nuevo archivo: `IR.h`**

```c
typedef enum { IR_ID_0=0, IR_ID_1=1, IR_ID_2=2, IR_ID_3=3 } _eIRID;

void    IR_Init(void);
uint8_t IR_IsDetected(_eIRID id);  // 1 = objeto detectado, 0 = libre
```

**Nuevo archivo: `IR.c`**

- `IR_Init()`: configura PD2–PD5 como entradas con pull-up interno (evita flotación si el sensor no está conectado)
- `IR_IsDetected(id)`: lee el pin correspondiente; retorna 1 cuando la señal S está en LOW (activo bajo — comportamiento del TCRT5000)

**Conexión del TCRT5000:**

| Pin sensor | Arduino |
|-----------|---------|
| V+ | 5V |
| G | GND |
| S | PD3 para IR1, PD4 para IR2, PD5 para IR3 |

**Cambios en `main.c`:**

| Cambio | Descripción |
|--------|-------------|
| `#include "IR.h"` | Inclusión del nuevo módulo |
| `IR_Init()` en `main()` | Reemplaza la config de GPIOs IR que estaba en `ini_GPIOs()` |
| `IR_DETECTED(IRx)` → `IR_IsDetected(IR_ID_x)` | Todas las lecturas IR pasan por el driver |
| Defines `IR0`–`IR3` y macro `IR_DETECTED` | Eliminados de `main.c` |

---

### 15/05/2026 ~19:30 — Driver CLASSIFIER.c/h para clasificación de cajas (Juan Ignacio)

La función `classify_box()`, el struct `_sBoxs` y el `#define REFERENCE_DIST_CM` se movieron de `main.c` a un módulo propio.

**Nuevo archivo: `CLASSIFIER.h`**

```c
typedef enum { BOX_NONE=0, BOX_SMALL=1, BOX_MEDIUM=2, BOX_BIG=3 } _eBoxType;

void      CLASSIFIER_Init(void);
_eBoxType CLASSIFIER_Classify(uint8_t d_cm);
```

**Nuevo archivo: `CLASSIFIER.c`**

- `_sBoxConfig` struct privado (static): h_small=6, h_medium=8, h_big=10
- `REFERENCE_DIST_CM = 20` como `#define` interno — ajustar antes de montar en el banco
- `CLASSIFIER_Classify(d_cm)`: calcula `h = REFERENCE_DIST_CM - d_cm` y retorna `_eBoxType`

**Cambios en `main.c`:**

| Qué | Acción |
|-----|--------|
| `typedef struct _sBoxs` + `boxs` global | Eliminados — lógica encapsulada en CLASSIFIER.c |
| `#define REFERENCE_DIST_CM` | Eliminado — movido a CLASSIFIER.c |
| `uint8_t classify_box()` | Eliminada — reemplazada por `CLASSIFIER_Classify()` |
| `uint8_t detected_type` | Cambiado a `_eBoxType detected_type = BOX_NONE` |
| Comparaciones `== 1/2/3` | Reemplazadas por `BOX_SMALL / BOX_MEDIUM / BOX_BIG` |
| `CLASSIFIER_Init()` en `main()` | Agregado |

---

### 16/05/2026 — Planificación: protocolo UNER completo, contadores y Modo Estimado (Juan Ignacio)

Definición del trabajo pendiente para completar la actividad. Tres áreas identificadas:

#### 1. Protocolo UNER completo (`decodeCMD()`)

`decodeCMD()` está vacío. Se define la tabla de comandos completa:

**Firmware → GUI (TX)**

| CMD  | Nombre          | Payload                              | Descripción                        |
|------|-----------------|--------------------------------------|------------------------------------|
| 0xA0 | ERR_SENSOR      | —                                    | Error HC-SR04                      |
| 0xA1 | DIST_MEAS       | d_cm (1B)                            | Nueva medición de distancia        |
| 0xA2 | BOX_CLASSIF     | type (1B)                            | Caja clasificada (SMALL/MED/BIG)   |
| 0xA3 | BOX_EJECTED     | type (1B)                            | Caja eyectada (servo retraído)     |
| 0xA4 | STATE_UPDATE    | state (1B)                           | Estado actual de la cinta          |
| 0xA5 | BOX_COUNTS      | small(2B) medium(2B) big(2B)         | Contadores acumulados              |
| 0xA6 | ACK             | echo_cmd(1B) status(1B)              | Confirmación / rechazo de comando  |

**GUI → Firmware (RX)**

| CMD  | Nombre          | Payload                              | Descripción                              |
|------|-----------------|--------------------------------------|------------------------------------------|
| 0xB0 | GET_STATE       | —                                    | Consulta estado de la cinta              |
| 0xB1 | GET_COUNTS      | —                                    | Consulta contadores de cajas             |
| 0xB2 | SET_MODE        | mode(1B): 0=Normal 1=Estimado        | Cambia modo de operación                 |
| 0xB3 | SET_BOX_MAP     | s_srv(1B) m_srv(1B) b_srv(1B)        | Asigna caja→servo (1/2/3)                |
| 0xB4 | SET_THRESH      | h_small(1B) h_med(1B) h_big(1B)      | Umbrales de altura (cm)                  |
| 0xB5 | SET_CALIB       | ref_dist(1B)                         | Calibración distancia de referencia (cm) |
| 0xB6 | RESET_COUNTS    | —                                    | Reinicia contadores                      |

**Safety lock:** los comandos 0xB2–0xB5 responden `ACK_BUSY (0x01)` si `state != CS_MEASURE` o `servo_active == TRUE`.

#### 2. Contadores de cajas (`main.c`)

- Agregar `uint16_t box_count[4]` (índice = `_eBoxType`)
- Incrementar en cada eyección (cuando el servo regresa a HOME)
- Enviar `CMD_BOX_EJECTED` automáticamente al eyectar
- Responder `CMD_COUNTS` al recibir `GET_COUNTS`

#### 3. Modo Estimado (`main.c`)

La actividad requiere soporte para dos modos de operación:

- **Normal** (actual): el servo espera que el IR en la posición del pateador detecte la caja para disparar.
- **Estimado**: si el IR del pateador no responde (falla o no está conectado), el servo se dispara automáticamente después de un tiempo fijo (`EST_DELAY_MS`) desde que se clasificó la caja. Modelo open-loop basado en la velocidad de la cinta.

Cambios necesarios: variable global `conv_mode`, timer de estimación `est_delay_timer`, carga del timer al transicionar a `CS_PUSHERx`, y bifurcación Normal/Estimado en cada estado `CS_PUSHER`.

#### 4. Extensión de `CLASSIFIER` para configuración dinámica

Para que `SET_THRESH` y `SET_CALIB` funcionen, `CLASSIFIER` necesita dos nuevas funciones:

```c
void CLASSIFIER_SetThresholds(uint8_t h_small, uint8_t h_medium, uint8_t h_big);
void CLASSIFIER_SetRefDist(uint8_t ref_cm);
```

Requiere quitar el `const` de `config` y convertir `REFERENCE_DIST_CM` a variable estática.

---

## Pendientes

- [ ] Implementar tabla de comandos UNER en `COMUNICATION.h` (`#define CMD_*`, `#define ACK_*`)
- [ ] Extender `CLASSIFIER.c/h` con setters para umbrales y distancia de referencia
- [ ] Agregar contadores de cajas en `main.c` + envío automático en eyección
- [ ] Implementar `decodeCMD()` completo
- [ ] Implementar Modo Estimado en `main.c` (timer de espera en `CS_PUSHERx`)
- [ ] Calibrar `REFERENCE_DIST_CM` en `CLASSIFIER.c` con el sensor montado en el banco real
- [ ] Probar los tres servos con osciloscopio o analizador lógico (señal 50 Hz, pulsos de 1 ms y 2 ms en PD7, PB4, PB3)
- [ ] GUI (Qt) — pendiente de distribución de tareas con el grupo

---

## Cómo probar con los botones de simulación

**Hardware necesario:**
- Botón entre D6 y 5V con pull-down a GND (10kΩ) — simula HC-SR04 para caja pequeña
- Botón entre D8 y 5V con pull-down a GND (10kΩ) — simula HC-SR04 para caja mediana
- Sensor IR TCRT5000 conectado en PD3 (IR1) o PD4 (IR2) según el pateador a probar
- SERVO1 en D7 (PD7), SERVO2 en PB4 (Arduino D12)

**Secuencia de prueba SERVO1:**
1. Presionar BTN_SMALL (D6) → sistema pasa a `CS_PUSHER1`, servo quieto
2. Pasar un objeto frente al TCRT5000 en PD3 (IR1) → servo va a 180°
3. A los 500 ms → servo vuelve solo a 0°, listo para nueva caja

**Secuencia de prueba SERVO2:**
- Ídem con BTN_MEDIUM (D8) → `CS_PUSHER2` → sensor en PD4 (IR2) → SERVO2

Compilar, flashear al Arduino con Microchip Studio (Debug → Start Without Debugging o cargar el `.hex` desde el menú).
