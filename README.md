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

## Pendientes

- [ ] Calibrar `REFERENCE_DIST_CM` con el sensor montado en el banco real (medir la distancia al transportador sin caja y actualizar el valor en `main.c`)
- [ ] Implementar `decodeCMD()` cuando se integre la GUI (Qt) — comandos para configurar el mapeo caja→servo y consultar estado
- [ ] Probar los tres servos con osciloscopio o analizador lógico (señal 50 Hz, pulsos de 1 ms y 2 ms en PD7, PB4, PB3)
- [ ] GUI (Qt) — pendiente de distribución de tareas con el grupo

---

## Cómo probar el servo (test básico)

Agregar temporalmente en `main()` antes del `sei()`:

```c
SERVO_Set(SERVO_ID_1, SERVO_PUSH);   // El servo debe moverse a 180°
```

Retirar la línea para volver a HOME, o agregar:

```c
SERVO_Set(SERVO_ID_1, SERVO_HOME);   // El servo debe volver a 0°
```

Compilar, flashear al Arduino con Microchip Studio (Debug → Start Without Debugging o cargar el `.hex` desde el menú).
