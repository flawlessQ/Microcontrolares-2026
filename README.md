# TP4 — Sistema de Clasificación de Cajas

**Materia:** Microcontroladores  
**Carrera:** Ingeniería en Mecatrónica — UNER FCAL  
**Grupo:** Alexander Medrano / Juan Ignacio  
**Microcontrolador:** ATmega328P (Arduino Uno, 16 MHz)

---

## Descripción

Sistema automatizado de clasificación de cajas en una cinta transportadora. El HC-SR04 mide la altura de cada caja al pasar; el clasificador la categoriza en pequeña, mediana o grande; y el servo SG90 correspondiente la eyecta al canal correcto cuando el IR de posición confirma su llegada. Una GUI en Qt permite supervisión en tiempo real, configuración remota de umbrales y selección de modo de operación.

---

## Estado actual del hardware

| Componente | Estado | Notas |
|------------|--------|-------|
| HC-SR04 | ✅ Montado | TRIG=PB1, ECHO=PB2 |
| SERVO1 (caja pequeña) | ✅ Montado | PD7 |
| SERVO2 (caja mediana) | ✅ Montado | PB3 (D11) |
| SERVO3 (caja grande) | ✅ Montado | PB4 (D12) |
| IR0 — zona de medición | ✅ Montado | PD5 (D5) |
| IR1 — pateador 1 | ✅ Montado | PD2 (D2) |
| IR2 — pateador 2 | ✅ Montado | PD3 (D3) |
| IR3 — pateador 3 | ✅ Montado | PD4 (D4) |

---

## Pinout completo

| Componente | Pin Arduino | Puerto AVR | Función |
|------------|-------------|------------|---------|
| HC-SR04 TRIG | D9 | PB1 | Pulso de disparo |
| HC-SR04 ECHO | D10 | PB2 | Recepción del eco |
| IR0 | D5 | PD5 | Zona de medición |
| IR1 | D2 | PD2 | Posición pateador 1 |
| IR2 | D3 | PD3 | Posición pateador 2 |
| IR3 | D4 | PD4 | Posición pateador 3 |
| SERVO1 | D7 | PD7 | Eyector caja pequeña |
| SERVO2 | D11 | PB3 | Eyector caja mediana |
| SERVO3 | D12 | PB4 | Eyector caja grande |
| LED builtin | D13 | PB5 | Heartbeat (toggle cada 100 ms) |

> **Alimentación servos:** los SG90 requieren ~500 mA. El USB del Arduino es insuficiente para los tres simultáneos — usar fuente externa de 5V con GND común.

---

## Estructura del proyecto

```
TP4 Microcontroladores/
├── Actividad 4/
│   └── Ejercicio/Ejercicio/
│       ├── main.c              <- Máquina de estados principal
│       ├── TIMERS.c/h          <- Timer0 (1ms), Timer1 (servo PWM), Timer2 (1µs)
│       ├── HCSR04.c/h          <- Driver HC-SR04 (medición bloqueante con Timer1)
│       ├── IR.c/h              <- Driver TCRT5000 (debounce 20ms, flanco, velocidad)
│       ├── SERVO.c/h           <- Software PWM 50Hz para tres SG90
│       ├── CLASSIFIER.c/h      <- Clasificación por altura, umbrales configurables
│       ├── COMUNICATION.c/h    <- USART 115200, protocolo UNER, buffers circulares
│       └── Debug/              <- Binarios compilados (.hex, .elf)
├── PruebaQt/                   <- GUI de supervisión en Qt 6
│   ├── mainwindow.cpp/h        <- Ventana principal (toda la UI por código)
│   ├── protocoloUNERQt.cpp/h   <- Decodificador del protocolo UNER para Qt
│   └── ...
├── Actividad Nº 4.pdf
├── SG90.PDF
├── HC-SR04-Ultrasonic.pdf
├── tcrt5000.pdf
└── README.md
```

---

## Arquitectura del firmware

### Timers

| Timer | Modo | Período | Uso |
|-------|------|---------|-----|
| Timer0 | CTC | 1 ms | Flag de sistema `GPIOR00` → tick principal |
| Timer1 | CTC | 1 ms | ISR `SERVO_On1ms()` — software PWM de los tres servos |
| Timer1* | Normal | — | *Prestado durante medición HC-SR04 (prescaler 8 → 0,5 µs/tick); Timer1 se restaura a CTC al terminar* |
| Timer2 | CTC | 1 µs | Polling de flag `TIFR2/OCF2A` en el loop principal |

### Máquina de estados (`main.c`)

```
                    ┌──────────────────────────────────────────┐
                    │                                          │
                    ▼                                          │
              CS_MEASURE                                       │
         IR0 rising edge → HCSR04_Measure()                   │
                    │                                          │
       ┌────────────┼────────────┐                             │
       │            │            │                             │
  BOX_SMALL    BOX_MEDIUM    BOX_SMALL    BOX_MEDIUM    BOX_BIG
       │            │            │                             │
       ▼            ▼            ▼                             │
  CS_PUSHER1   CS_PUSHER2   CS_PUSHER3                         │
  IR1 / timer  IR2 / timer  timer (IR3 en PD2)                 │
       │            │            │                             │
       └─────┬──────────────────┘                              │
             │  SERVO PUSH 500ms → HOME                        │
             └─────────────────────────────────────────────────┘
```

**Modos de operación (configurables desde la GUI):**

| Modo | Comportamiento en CS_PUSHERx |
|------|------------------------------|
| Normal (0) | El servo espera que el IR del pateador detecte la caja |
| Estimado (1) | El servo dispara automáticamente tras `EST_DELAY_MS` (2000 ms) |

### Driver HC-SR04

Medición **bloqueante por hardware**: `HCSR04_Measure()` deshabilita la ISR del servo, configura Timer1 en modo normal (prescaler 8 → 1 tick = 0,5 µs), realiza el ciclo TRIG/ECHO con busy-wait y restaura Timer1 a CTC al finalizar. Tiempo típico de medición: 1–2 ms.

### Driver IR (TCRT5000)

- Debounce por software de 20 ms (llamado cada 1 ms desde `On1ms()`)
- `IR_RisingEdge()`: detección de flanco ascendente, se consume al leer
- `IR_IsDetected()`: estado estable actual
- `IR_UpdateBoxSpeed()`: calcula velocidad en mm/s a partir del tiempo de bloqueo

### Software PWM (SERVO.c)

- Período: 20 ms (50 Hz) — generado en `ISR(TIMER1_COMPA_vect)`
- `SERVO_HOME` = pulso 1 ms → 0° (posición retraída)
- `SERVO_PUSH` = pulso 2 ms → 180° (posición de empuje)
- Los tres servos comparten un contador de 0–19 ms

---

## Protocolo de comunicación

USART0 — 115200 baud, 8N1. Formato de trama:

```
| 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
```

Checksum: XOR de todos los bytes desde CMD hasta el último byte de payload.

### Firmware → GUI

| CMD | Nombre | Payload | Descripción |
|-----|--------|---------|-------------|
| 0xA0 | `CMD_ERR_SENSOR` | — | Error o timeout del HC-SR04 |
| 0xA1 | `CMD_DIST_MEAS` | `d_cm` (1B) | Distancia medida en cm |
| 0xA2 | `CMD_BOX_CLASSIF` | `type` (1B) | Tipo clasificado (definido, pendiente de envío) |
| 0xA3 | `CMD_BOX_EJECTED` | `type` (1B) | Caja eyectada — servo volvió a HOME |
| 0xA4 | `CMD_STATE` | `state` (1B) | Estado actual de la cinta (0–3) |
| 0xA5 | `CMD_COUNTS` | `s_hi s_lo m_hi m_lo b_hi b_lo` (6B) | Contadores acumulados |
| 0xA6 | `CMD_ACK` | `echo_cmd status` (2B) | Confirmación de comando recibido |

### GUI → Firmware

| CMD | Nombre | Payload | Descripción |
|-----|--------|---------|-------------|
| 0xB0 | `CMD_GET_STATE` | — | Solicita estado actual |
| 0xB1 | `CMD_GET_COUNTS` | — | Solicita contadores |
| 0xB2 | `CMD_SET_MODE` | `mode` (1B): 0=Normal 1=Estimado | Cambia modo de operación |
| 0xB3 | `CMD_SET_BOX_MAP` | `s_srv m_srv b_srv` (3B) | Mapeo caja→servo (definido, no implementado) |
| 0xB4 | `CMD_SET_THRESH` | `h_small h_med h_big` (3B) | Umbrales de altura en cm |
| 0xB5 | `CMD_SET_CALIB` | `ref_dist` (1B) | Distancia de referencia en cm |
| 0xB6 | `CMD_RESET_COUNTS` | — | Reinicia los tres contadores |

**Safety lock:** los comandos 0xB2–0xB5 responden `ACK_BUSY` si `state != CS_MEASURE` o `servo_active == TRUE`.

---

## GUI Qt (`PruebaQt/`)

Desarrollada en Qt 6. Toda la UI se construye por código en `buildUI()` (sin `.ui` de Designer).

**Funcionalidades:**
- Conexión/desconexión serie con selector de puerto COM
- Panel de estado: estado de la cinta en tiempo real + última distancia medida
- Contadores LCD por tipo de caja (pequeña / mediana / grande) con reset remoto
- Panel de configuración: modo Normal/Estimado, umbrales de altura, distancia de referencia
- Bloqueo automático del panel de configuración cuando la cinta está en movimiento
- Log de comunicación con timestamps
- Polling automático cada 500 ms (GET_STATE + GET_COUNTS)

---

## Pendientes

- [ ] **Calibrar distancia de referencia** — medir distancia real sensor→cinta en el banco y configurar via GUI o hardcodear en `CLASSIFIER.c`
- [ ] **Habilitar IR3 en lógica del pateador 3** — IR3 ya está mapeado a PD2; falta que el bloque Pusher3 lo use en Modo Normal además del timer
- [ ] **Alimentación externa para servos** — fuente 5V/1A externa con GND común al Arduino
- [ ] **Enviar CMD_BOX_CLASSIF (0xA2)** — el tipo clasificado se calcula pero no se envía a la GUI

---

## Historial

### 01/05/2026 — Inicio del proyecto (Alexander Medrano)

Creación del proyecto base en Microchip Studio. Implementación de `TIMERS.c`, `HCSR04.c` (máquina de estados no bloqueante), `COMUNICATION.c` (protocolo UNER, buffers circulares 128B), y `main.c` base con heartbeat y medición continua HC-SR04.

---

### 15/05/2026 — Servos, clasificación y lógica de pateadores (Juan Ignacio)

- **`SERVO.c/h`**: driver de software PWM 50 Hz para tres SG90 vía ISR de Timer1
- **`CLASSIFIER.c/h`**: módulo de clasificación; extrae `classify_box()` y `_sBoxs` de main.c
- **`IR.c/h`**: driver inicial para TCRT5000 (lectura directa activo-bajo)
- **`main.c`**: máquina de estados CS_MEASURE → CS_PUSHERx completa; simulación de caja con botones BTN_SMALL (PD6) y BTN_MEDIUM (PB0); corrección de Timer1 que nunca corría

---

### 15–16/05/2026 — Protocolo UNER completo y Modo Estimado (Juan Ignacio)

- **`COMUNICATION.h`**: tabla de comandos 0xA0–0xA6 (TX) y 0xB0–0xB6 (RX) con ACK_OK/BUSY/INVALID
- **`decodeCMD()`**: implementación completa — GET_STATE, GET_COUNTS, SET_MODE, SET_THRESH, SET_CALIB, RESET_COUNTS con safety lock
- **`CLASSIFIER.c`**: agregados `CLASSIFIER_SetThresholds()` y `CLASSIFIER_SetRefDist()` para configuración dinámica
- **`main.c`**: contadores `box_count[4]`, envío automático de CMD_BOX_EJECTED, variable `conv_mode`, timer `est_delay_timer`, bifurcación Normal/Estimado en cada CS_PUSHERx
- **`PruebaQt/`**: primera versión de la GUI Qt con conexión serial, log, contadores y panel de configuración

---

### 17/05/2026 — Integración HC-SR04 con hardware real (Juan Ignacio)

- **`HCSR04.c/h`**: reemplazado por driver con medición bloqueante (`HCSR04_Measure()`) usando Timer1 hardware — soluciona lecturas erróneas de 0 cm/1 cm del driver anterior
- **`IR.c/h`**: reescrito con debounce de 20 ms, `IR_RisingEdge()` y `IR_UpdateBoxSpeed()`
- **`TIMERS.c`**: corregido OCR2A de 2 a 1 (daba 1,5 µs en lugar de 1 µs)
- **`main.c`**: eliminados botones de simulación; CS_MEASURE usa `IR_RisingEdge(IR_ID_0)` como trigger; Timer1 se deshabilita durante medición y se restaura a CTC; pull-up de ECHO eliminado (causaba lecturas falsas); IR0 remapeado a PD4; CS_PUSHER3 deshabilitado hasta tener IR3
- **GUI**: fix +/- invisible en QSpinBox (eliminado styling de sub-controles); label de aviso con `setWordWrap`, padding y `setMinimumHeight`; radio buttons con indicador `:checked` azul; ventana mínima 860×680
