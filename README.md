# TP4 — Sistema de Clasificación de Cajas

**Materia:** Microcontroladores  
**Carrera:** Ingeniería en Mecatrónica — UNER FCAL  
**Grupo:** Alexander Medrano / Juan Ignacio  
**Microcontrolador:** ATmega328P (Arduino Uno R3, 16 MHz)

---

## Descripción general

Sistema embebido de clasificación automática de cajas sobre una cinta transportadora. Un sensor ultrasónico mide la altura de cada caja al pasar por la zona de medición; el firmware la clasifica en tres categorías (pequeña, mediana, grande) y activa el servomotor eyector correspondiente cuando el sensor IR de posición confirma que la caja llegó al pateador correcto. Una GUI de escritorio permite supervisión en tiempo real y configuración remota del sistema.

---

## Diagrama de contexto

```
                          ┌─────────────────────────────────────────┐
                          │           CINTA TRANSPORTADORA           │
                          │                                          │
   CAJA ──►  [IR0/D5] ──► [HC-SR04] ──► [IR1/D2] ──► [IR2/D3] ──► [IR3/D4] ──► salida
             zona medir    mide altura   pateador1     pateador2     pateador3
                  │             │             │             │             │
                  │             ▼             │             │             │
                  │       h_cm = ref - d      │             │             │
                  │       Classify(h_cm)      │             │             │
                  │             │             ▼             ▼             ▼
                  │         FIFO / IRTarget  [SERVO1/D7] [SERVO2/D11] [SERVO3/D12]
                  │                          eyecta Peq   eyecta Med   eyecta Big
                  │
                  │                      ┌───────────────────┐
                  └──────── USART ───────►   GUI Qt (PC)      │
                            115200       │  supervisión +     │
                            8N1          │  configuración     │
                                         └───────────────────┘
```

---

## Flujo de operación

```
Caja llega a IR0
      │
      ▼
  IR_RisingEdge(IR0)
      │
      ▼
  HCSR04_Measure()          ← Timer1 prestado (prescaler 8, 0.5 µs/tick)
      │
      ▼
  h_cm = ref_dist - d_cm
      │
      ├── h fuera de ventana → BOX_NONE → descartada (CMD_BOX_DISCARDED)
      │
      └── h en ventana ±0.5cm →  BOX_SMALL / BOX_MEDIUM / BOX_BIG
                  │
                  ▼
         [MODO NORMAL]                    [MODO ESTIMADO]
         IRTarget_RegisterBox(t)          CLASSIFIER_QueuePush(t, 2000ms)
                  │                                │
                  │  Caja llega a IR1              │  Timer expira
                  ▼                                ▼
         IRTarget_Tick(PUSHER_1)         QueueFrontType() == tipo
         ¿es su turno?                   QueueFrontTimer() == 0
                  │                                │
                  └──────────── fire ──────────────┘
                                  │
                                  ▼
                           SERVO_Set(PUSH)
                           servo_hold_timer = 500ms
                                  │
                           (500ms después)
                                  │
                                  ▼
                           SERVO_Set(HOME)
                           box_count[tipo]++
                           CMD_BOX_EJECTED → GUI
```

---

## Arquitectura del firmware

### Módulos

| Módulo | Archivo | Responsabilidad |
|--------|---------|-----------------|
| Loop principal + IRTarget | `main.c` | Orquesta todos los módulos; implementa el registro de paso por sensor |
| Timers | `TIMERS.c/h` | Configura Timer0 (1ms), Timer1 (PWM servo / HC-SR04), Timer2 (1µs) |
| Sensor ultrasónico | `HCSR04.c/h` | Medición bloqueante con Timer1 por hardware |
| Sensores IR | `IR.c/h` | Debounce 20ms, flancos, tiempo de bloqueo |
| Clasificador | `CLASSIFIER.c/h` | Clasificación por ventana ±0.5cm + cola FIFO (Modo Estimado) |
| Servomotores | `SERVO.c/h` | Software PWM 50Hz para tres SG90 via ISR de Timer1 |
| Comunicación | `COMUNICATION.c/h` | USART 115200 8N1, protocolo UNER, buffers circulares 128B |
| Cinta | `CONVEYOR.c/h` | Control del motor de la cinta via PC0 (A0) |

---

### Uso de los timers

| Timer | Modo | Período | Rol |
|-------|------|---------|-----|
| Timer0 CTC | Interrupción | 1 ms | Setea flag `GPIOR00` → dispara `On1ms()` en el loop |
| Timer1 CTC | ISR | 1 ms | `SERVO_On1ms()` — genera PWM 50Hz de los tres servos |
| Timer1 Normal | Busy-wait | prestado | Medición HC-SR04 (prescaler 8 → 1 tick = 0.5 µs) |
| Timer2 CTC | Polling | 1 µs | Flag `OCF2A` — base de tiempo de alta resolución |

> Timer1 es compartido: durante `HCSR04_Measure()` la ISR de servo se deshabilita, Timer1 se reconfigura en modo Normal para capturar el pulso ECHO, y al terminar se restaura a CTC para el PWM.

---

### Sistema IRTarget (Modo Normal)

El problema central de un clasificador multi-pateador es saber **cuántas cajas debe ignorar cada IR antes de disparar**. Si vienen tres cajas seguidas (MEDIUM, BIG, SMALL), el IR1 del pateador 1 debe dejar pasar las dos primeras y disparar recién con la tercera.

**Estructura:**
```c
typedef struct {
    uint8_t count[8];  // cuántas cajas pasar antes de la objetivo
    uint8_t head, tail, qty;
} _sIRTargetQueue;

_sIRTargetQueue ir_target[3];  // una cola por pateador
```

**Lógica de registro (`IRTarget_RegisterBox`):**

Al medir una caja en IR0, se actualiza un acumulador `ir_pass_acc[]` por pateador:

```
BOX_SMALL  → encola ir_pass_acc[1] en PUSHER_1, resetea acc[1]
             acc[2]++ y acc[3]++      (pasa por IR2 e IR3)

BOX_MEDIUM → acc[1]++               (pasa por IR1)
             encola ir_pass_acc[2] en PUSHER_2, resetea acc[2]
             acc[3]++               (pasa por IR3)

BOX_BIG    → acc[1]++ , acc[2]++
             encola ir_pass_acc[3] en PUSHER_3, resetea acc[3]

BOX_NONE   → acc[1]++ , acc[2]++ , acc[3]++  (pasa por todos)
```

**Disparo (`IRTarget_Tick`):**
Cada rising edge de IR1/IR2/IR3 llama `IRTarget_Tick(pusher)`. Si el contador al frente de la cola es 0, es el turno de esa caja → `pusher_fire_pending = TRUE`. Si no, decrementa en 1.

---

### Clasificación por ventana cerrada

El HC-SR04 apunta hacia el piso a `reference_dist_cm` de distancia. La altura de la caja es:

```
h_cm = reference_dist_cm - d_medido
```

La clasificación usa una ventana de **1 cm por categoría** (±0.5 cm):

```
h_small  ≤ h < h_small  + 1  →  BOX_SMALL   (default: 6 cm)
h_medium ≤ h < h_medium + 1  →  BOX_MEDIUM  (default: 8 cm)
h_big    ≤ h < h_big    + 1  →  BOX_BIG     (default: 10 cm)
cualquier otro valor          →  BOX_NONE    (descartada)
```

Los umbrales son configurables desde la GUI via `CMD_SET_THRESH`.

---

### Modos de operación

| Modo | Trigger de disparo | Caso de uso |
|------|--------------------|-------------|
| **Normal** | Rising edge del IR de posición (IR1/IR2/IR3) confirma la caja | Hardware completo montado |
| **Estimado** | Timer de 2000 ms desde la medición | Sin sensores IR de posición |

Configurable desde la GUI via `CMD_SET_MODE`.

---

## Pinout

| Señal | Arduino | Puerto AVR | Función |
|-------|---------|------------|---------|
| HC-SR04 TRIG | D9 | PB1 | Pulso de disparo |
| HC-SR04 ECHO | D10 | PB2 | Recepción del eco |
| IR0 | D5 | PD5 | Zona de medición |
| IR1 | D2 | PD2 | Posición pateador 1 (SMALL) |
| IR2 | D3 | PD3 | Posición pateador 2 (MEDIUM) |
| IR3 | D4 | PD4 | Posición pateador 3 (BIG) |
| SERVO1 | D7 | PD7 | Eyector caja pequeña |
| SERVO2 | D11 | PB3 | Eyector caja mediana |
| SERVO3 | D12 | PB4 | Eyector caja grande |
| CONVEYOR | A0 | PC0 | Enable motor cinta |
| LED builtin | D13 | PB5 | Heartbeat (toggle cada 100 ms) |

> **Alimentación servos:** los SG90 consumen ~500 mA en total. El USB del Arduino no es suficiente — usar fuente externa 5V con GND común al Arduino.

---

## Protocolo de comunicación (UNER)

USART0 — 115200 baud, 8N1. Trama:

```
┌───┬───┬───┬───┬────────┬─────┬─────┬─────────┬──────────┐
│ U │ N │ E │ R │ LENGTH │ ':' │ CMD │ PAYLOAD │ CHECKSUM │
└───┴───┴───┴───┴────────┴─────┴─────┴─────────┴──────────┘
```

`CHECKSUM` = XOR acumulado desde `CMD` hasta el último byte de payload.

### Firmware → GUI

| CMD | Nombre | Payload | Descripción |
|-----|--------|---------|-------------|
| 0xA0 | `CMD_ERR_SENSOR` | — | Error o timeout del HC-SR04 |
| 0xA1 | `CMD_DIST_MEAS` | `h_cm` (1B) | Altura de caja medida en cm |
| 0xA3 | `CMD_BOX_EJECTED` | `type` (1B) | Caja eyectada correctamente |
| 0xA4 | `CMD_STATE` | `state` (1B) | Estado de la cinta (0=midiendo, 1-3=pateador activo) |
| 0xA5 | `CMD_COUNTS` | `s_hi s_lo m_hi m_lo b_hi b_lo d_hi d_lo` (8B) | Contadores: pequeña / mediana / grande / descartadas |
| 0xA6 | `CMD_ACK` | `echo_cmd status` (2B) | Respuesta a comandos RX |
| 0xA7 | `CMD_BOX_DISCARDED` | — | Caja fuera de rango descartada |

### GUI → Firmware

| CMD | Nombre | Payload | Descripción |
|-----|--------|---------|-------------|
| 0xB0 | `CMD_GET_STATE` | — | Solicita estado actual |
| 0xB1 | `CMD_GET_COUNTS` | — | Solicita contadores |
| 0xB2 | `CMD_SET_MODE` | `mode` (1B) | 0=Normal, 1=Estimado |
| 0xB4 | `CMD_SET_THRESH` | `h_s h_m h_b` (3B) | Umbrales de altura en cm |
| 0xB5 | `CMD_SET_CALIB` | `ref_dist` (1B) | Distancia referencia sensor→cinta en cm |
| 0xB6 | `CMD_RESET_COUNTS` | — | Reinicia todos los contadores |

> **Safety lock:** los comandos de configuración (0xB2–0xB5) responden `ACK_BUSY` si algún servo está activo o hay cajas en cola.

---

## GUI Qt

Desarrollada en Qt 6. Toda la interfaz se construye por código en `buildUI()`.

```
┌──────────────────────────────────────────────────┐
│  UNER FCAL  |  Sistema de Clasificación de Cajas │
├──────────────┬──────────────────────────────────────┤
│ Conexión     │ Puerto COM ▼  [↻]  [CONECTAR]       │
├──────────────┴──────────────────────────────────────┤
│ Estado    │ Contadores        │ Configuración        │
│ MIDIENDO  │ Pequeña  [0000]   │ ● Normal  ○ Velocidad│
│ Altura:-- │ Mediana  [0000]   │ Pequeña:  6 cm       │
│           │ Grande   [0000]   │ Mediana:  8 cm       │
│           │ Descard. [0000]   │ Grande:  10 cm       │
│           │ [RESET CONTADORES]│ Ref:     20 cm       │
│           │                   │ [APLICAR CONFIG]     │
├───────────────────────────────────────────────────  │
│ Log de comunicación                                  │
│ [12:34:56]  [A1] Altura: 6 cm                        │
│ [12:34:57]  [A3] ✓ Caja eyectada: PEQUEÑA            │
└──────────────────────────────────────────────────────┘
```

- Polling automático cada 500 ms (`GET_STATE` + `GET_COUNTS`)
- Panel de configuración bloqueado mientras la cinta está en movimiento
- Log con timestamps de todos los eventos

---

## Estructura del repositorio

```
TP4 Microcontroladores/
├── Actividad 4/
│   └── Ejercicio/Ejercicio/
│       ├── main.c              ← loop principal + IRTarget
│       ├── TIMERS.c/h          ← Timer0/1/2
│       ├── HCSR04.c/h          ← driver HC-SR04 bloqueante
│       ├── IR.c/h              ← driver TCRT5000
│       ├── SERVO.c/h           ← software PWM 50Hz
│       ├── CLASSIFIER.c/h      ← clasificación + cola FIFO
│       ├── COMUNICATION.c/h    ← USART + protocolo UNER
│       └── CONVEYOR.c/h        ← control motor cinta
├── PruebaQt/
│   ├── mainwindow.cpp/h        ← UI completa por código
│   ├── protocoloUNERQt.cpp/h   ← decodificador del protocolo
│   └── resources.qrc
├── Actividad Nº 4.pdf
├── SG90.PDF
├── HC-SR04-Ultrasonic.pdf
└── tcrt5000.pdf
```
