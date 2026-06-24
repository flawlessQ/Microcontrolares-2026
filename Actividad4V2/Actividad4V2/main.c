#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "TIMERS.h"
#include "SERVO.h"
#include "IR.h"
#include "HCSR04.h"
#include "COMUNICATION.h"

// ===========================================================================
// PINES
// ===========================================================================

#define LED_BUILTIN  PINB5
#define TRIG_PIN     PB1
#define ECHO_PIN     PB2

#define SERVO_HOLD_MS 500

// ===========================================================================
// MEF HCSR04 — estados
// ===========================================================================

typedef enum {
    HCSR04_SM_IDLE = 0,
    HCSR04_SM_TRIGGER_HIGH,
    HCSR04_SM_WAIT_ECHO_HIGH,
    HCSR04_SM_ECHO_HIGH,
    HCSR04_SM_READY,
    HCSR04_SM_TIMEOUT
} HCSR04_SM_State_t;

// Timer2 Normal, prescaler 64 -> 1 tick = 4us
#define HCSR04_T2_TICK_US       4UL
#define HCSR04_TRIGGER_TICKS    3UL       // 3 x 4us = 12us >= 10us minimo
#define HCSR04_TIMEOUT_TICKS    10000UL   // 10000 x 4us = 40ms timeout

// ===========================================================================
// VARIABLES
// ===========================================================================

uint8_t  time100ms          = 100;
static uint16_t servo_hold[3]   = {0, 0, 0};
static uint8_t  servo_active[3] = {0, 0, 0};

static _sHCSR04_t         sensor;
static HCSR04_SM_State_t  hcsr04_state        = HCSR04_SM_IDLE;
static uint32_t           timer2_overflows     = 0;
static uint32_t           hcsr04_trigger_start = 0;
static uint32_t           hcsr04_wait_start    = 0;
static uint32_t           hcsr04_echo_start    = 0;
static uint32_t           hcsr04_echo_result_us = 0;

static uint16_t           distancia_cm         = 0;

// ===========================================================================
// PROCESADOR DE COMANDOS UART
// ===========================================================================

static void ProcessCmd(uint8_t cmd, uint8_t *payload){
    uint8_t ack[2];

    switch(cmd){

        case CMD_GET_STATE:{
            uint8_t st = 0;
            if     (servo_active[0]) st = 1;
            else if(servo_active[1]) st = 2;
            else if(servo_active[2]) st = 3;
            COM_SendFrame(CMD_STATE, &st, 1);
            break;
        }

        case CMD_GET_COUNTS:{
            uint8_t p[8] = {0,0, 0,0, 0,0, 0,0};
            COM_SendFrame(CMD_COUNTS, p, 8);
            break;
        }

        case CMD_SET_SERVO:
            SERVO_SetConfig((_eServoID)payload[0], payload[1], payload[2]);
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        default:
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;
    }
}

// ===========================================================================
// TIMER2 — contador libre extendido (4us/tick)
// ===========================================================================

static void TIMER2_UpdateOverflow(void) {
    if (TIFR2 & (1 << TOV2)) {
        TIFR2 |= (1 << TOV2);
        timer2_overflows++;
    }
}

static uint32_t TIMER2_GetTicks(void) {
    TIMER2_UpdateOverflow();
    return ((uint32_t)timer2_overflows << 8) + TCNT2;
}

static uint32_t TIMER2_GetElapsedTicks(uint32_t start_ticks) {
    return TIMER2_GetTicks() - start_ticks;
}

// ===========================================================================
// MEF HCSR04 — avanza un paso sin bloquear
// ===========================================================================

static void HCSR04_Task(void) {
    switch (hcsr04_state) {

        case HCSR04_SM_IDLE:
        case HCSR04_SM_READY:
        case HCSR04_SM_TIMEOUT:
            break;

        case HCSR04_SM_TRIGGER_HIGH:
            if (TIMER2_GetElapsedTicks(hcsr04_trigger_start) >= HCSR04_TRIGGER_TICKS) {
                PORTB &= ~(1 << TRIG_PIN);
                hcsr04_wait_start = TIMER2_GetTicks();
                hcsr04_state = HCSR04_SM_WAIT_ECHO_HIGH;
            }
            break;

        case HCSR04_SM_WAIT_ECHO_HIGH:
            if (PINB & (1 << ECHO_PIN)) {
                hcsr04_echo_start = TIMER2_GetTicks();
                hcsr04_state = HCSR04_SM_ECHO_HIGH;
            } else if (TIMER2_GetElapsedTicks(hcsr04_wait_start) >= HCSR04_TIMEOUT_TICKS) {
                PORTB &= ~(1 << TRIG_PIN);
                hcsr04_state = HCSR04_SM_TIMEOUT;
            }
            break;

        case HCSR04_SM_ECHO_HIGH:
            if (!(PINB & (1 << ECHO_PIN))) {
                hcsr04_echo_result_us =
                    TIMER2_GetElapsedTicks(hcsr04_echo_start) * HCSR04_T2_TICK_US;
                hcsr04_state = HCSR04_SM_READY;
            } else if (TIMER2_GetElapsedTicks(hcsr04_echo_start) >= HCSR04_TIMEOUT_TICKS) {
                hcsr04_state = HCSR04_SM_TIMEOUT;
            }
            break;
    }
}

// ===========================================================================
// CALLBACKS HCSR04 (llamados por HCSR04_MeasureCm)
// ===========================================================================

static void MyTrigger(void *context) {
    (void)context;
    if (hcsr04_state != HCSR04_SM_IDLE) return;
    hcsr04_echo_result_us = 0;
    hcsr04_trigger_start  = TIMER2_GetTicks();
    PORTB |= (1 << TRIG_PIN);
    hcsr04_state = HCSR04_SM_TRIGGER_HIGH;
}

static uint8_t MyReadEcho(void *context, uint32_t *echo_us) {
    (void)context;
    if (!echo_us) return 0;
    if (hcsr04_state == HCSR04_SM_READY) {
        *echo_us     = hcsr04_echo_result_us;
        hcsr04_state = HCSR04_SM_IDLE;
        return 1;
    }
    if (hcsr04_state == HCSR04_SM_TIMEOUT) {
        hcsr04_state = HCSR04_SM_IDLE;
        return 0;
    }
    return 0;
}

// ===========================================================================
// GPIOs
// ===========================================================================

static void ini_GPIOs(void) {
    DDRB |= (1 << LED_BUILTIN);
    DDRB |=  (1 << TRIG_PIN);
    DDRB &= ~(1 << ECHO_PIN);
    PORTB &= ~(1 << TRIG_PIN);
    PORTB &= ~(1 << ECHO_PIN);
}

// ===========================================================================
// ON1MS
// ===========================================================================

static void On1ms(void) {
    if (!time100ms) time100ms = 100;
    time100ms--;

    IR_Update();

    for (uint8_t i = 0; i < 3; i++) {
        if (servo_active[i]) {
            if (servo_hold[i] > 0) {
                servo_hold[i]--;
            } else {
                servo_active[i] = 0;
                SERVO_Set((_eServoID)i, SERVO_HOME);
            }
        }
    }

    GPIOR0 &= ~(1 << GPIOR00);
}

// ===========================================================================
// HEARTBEAT
// ===========================================================================

void Heartbeat(void) {
    if (!time100ms) {
        time100ms = 100;
        PORTB ^= (1 << LED_BUILTIN);
    }
}

// ===========================================================================
// ISR TIMER0 — tick 1ms
// ===========================================================================

ISR(TIMER0_COMPA_vect) {
    OCR0A += 249;
    GPIOR0 |= (1 << GPIOR00);
}

// ===========================================================================
// MAIN
// ===========================================================================

int main(void) {
    cli();

    ini_TIMER0();
    ini_TIMER1();
    ini_TIMER2();
    ini_GPIOs();
    SERVO_Init();
    IR_Init();
    HCSR04_Init(&sensor, MyTrigger, MyReadEcho, 0);
    COM_Init();

    sei();

    while (1) {

        Heartbeat();

        HCSR04_Task();

        if (GPIOR0 & (1 << GPIOR00)) {
            On1ms();
        }

        HCSR04_Task();

        // IR0: llegó una caja → disparar medición
        if (IR_FallingEdge(IR0) && hcsr04_state == HCSR04_SM_IDLE) {
            HCSR04_MeasureCm(&sensor, &distancia_cm);
        }

        HCSR04_Task();

        // HCSR04 lista → leer resultado y enviar a GUI
        if (hcsr04_state == HCSR04_SM_READY) {
            if(HCSR04_MeasureCm(&sensor, &distancia_cm) == HCSR04_OK){
                uint8_t d = (distancia_cm > 255) ? 255 : (uint8_t)distancia_cm;
                COM_SendFrame(CMD_DIST_MEAS, &d, 1);
            }
        }

        HCSR04_Task();

        // HCSR04 timeout → liberar sensor y avisar a GUI
        if (hcsr04_state == HCSR04_SM_TIMEOUT) {
            HCSR04_MeasureCm(&sensor, &distancia_cm);
            COM_SendFrame(CMD_ERR_SENSOR, 0, 0);
        }

        HCSR04_Task();

        // IR1: pateador 1
        if (IR_FallingEdge(IR1) && !servo_active[0]) {
            servo_active[0] = 1;
            servo_hold[0]   = SERVO_HOLD_MS;
            SERVO_Set(SERVO_1, SERVO_PUSH);
        }

        // IR2: pateador 2
        if (IR_FallingEdge(IR2) && !servo_active[1]) {
            servo_active[1] = 1;
            servo_hold[1]   = SERVO_HOLD_MS;
            SERVO_Set(SERVO_2, SERVO_PUSH);
        }

        // UART: procesar bytes recibidos de la GUI
        COM_Update();
        if(COM_FrameAvailable()){
            uint8_t cmd, payload[COM_PAYLOAD_SIZE], len;
            COM_GetFrame(&cmd, payload, &len);
            ProcessCmd(cmd, payload);
        }
    }
}
