#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "TIMERS.h"
#include "SERVO.h"
#include "IR.h"
#include "HCSR04.h"
#include "COMUNICATION.h"
#include "CLASSIFIER.h"
#include "IRTARGET.h"
#include "CONVEYOR.h"

// ===========================================================================
// PINES
// ===========================================================================

#define LED_BUILTIN   PINB5
#define TRIG_PIN      PB1
#define ECHO_PIN      PB2
#define SERVO_HOLD_MS 500

// ===========================================================================
// MEF HCSR04
// ===========================================================================

// Delay entre deteccion IR y disparo de servo (caja aun en transito)
#define SERVO_FIRE_DELAY_MS    150

// Multi-muestra: recolecta medidas mientras IR0 este activo y promedia
#define MAX_MEAS_SAMPLES    8
#define MEAS_RETRY_WAIT_MS  60    // ms entre disparos del HCSR04
#define IR0_IS_LOW()        ((PIND & (1 << PD5)) == 0)

typedef enum {
    HCSR04_SM_IDLE = 0,
    HCSR04_SM_TRIGGER_HIGH,
    HCSR04_SM_WAIT_ECHO_HIGH,
    HCSR04_SM_ECHO_HIGH,
    HCSR04_SM_READY,
    HCSR04_SM_TIMEOUT
} HCSR04_SM_State_t;

#define HCSR04_T2_TICK_US     4UL
#define HCSR04_TRIGGER_TICKS  3UL
#define HCSR04_TIMEOUT_TICKS  10000UL

// ===========================================================================
// VARIABLES
// ===========================================================================

static uint8_t              time100ms            = 100;
static uint16_t             servo_hold[3]        = {0,0,0};
static uint8_t              servo_active[3]      = {0,0,0};
static uint8_t              fire_pending[3]      = {0,0,0};
static uint16_t             fire_delay[3]        = {0,0,0};
static uint16_t             cnt[4]               = {0,0,0,0};  // BOX_SMALL/MEDIUM/BIG/NONE
static _sClassifierConfig   classifier_cfg       = {6, 8, 10, 20};

typedef enum { CMEAS_IDLE=0, CMEAS_COLLECTING } CMeasState_t;
static CMeasState_t cmeas_state  = CMEAS_IDLE;
static uint8_t      meas_buf[MAX_MEAS_SAMPLES];
static uint8_t      meas_count   = 0;
static uint16_t     meas_wait_ms = 0;

static _sHCSR04_t           sensor;
static HCSR04_SM_State_t    hcsr04_state         = HCSR04_SM_IDLE;
static uint32_t             timer2_overflows      = 0;
static uint32_t             hcsr04_trigger_start;
static uint32_t             hcsr04_wait_start;
static uint32_t             hcsr04_echo_start;
static uint32_t             hcsr04_echo_result_us;
static uint16_t             distancia_cm          = 0;

// ===========================================================================
// TIMER2 — contador libre extendido (4µs/tick)
// ===========================================================================

static void TIMER2_UpdateOverflow(void) {
    if (TIFR2 & (1<<TOV2)) { TIFR2 |= (1<<TOV2); timer2_overflows++; }
}
static uint32_t TIMER2_GetTicks(void) {
    TIMER2_UpdateOverflow();
    return ((uint32_t)timer2_overflows << 8) + TCNT2;
}
static uint32_t TIMER2_GetElapsedTicks(uint32_t start) {
    return TIMER2_GetTicks() - start;
}

// ===========================================================================
// MEF HCSR04 — un paso por llamada, sin bloquear
// ===========================================================================

static void HCSR04_Task(void) {
    switch(hcsr04_state) {
        case HCSR04_SM_IDLE:
        case HCSR04_SM_READY:
        case HCSR04_SM_TIMEOUT:
            break;

        case HCSR04_SM_TRIGGER_HIGH:
            if (TIMER2_GetElapsedTicks(hcsr04_trigger_start) >= HCSR04_TRIGGER_TICKS) {
                PORTB &= ~(1<<TRIG_PIN);
                hcsr04_wait_start = TIMER2_GetTicks();
                hcsr04_state = HCSR04_SM_WAIT_ECHO_HIGH;
            }
            break;

        case HCSR04_SM_WAIT_ECHO_HIGH:
            if (PINB & (1<<ECHO_PIN)) {
                hcsr04_echo_start = TIMER2_GetTicks();
                hcsr04_state = HCSR04_SM_ECHO_HIGH;
            } else if (TIMER2_GetElapsedTicks(hcsr04_wait_start) >= HCSR04_TIMEOUT_TICKS) {
                PORTB &= ~(1<<TRIG_PIN);
                hcsr04_state = HCSR04_SM_TIMEOUT;
            }
            break;

        case HCSR04_SM_ECHO_HIGH:
            if (!(PINB & (1<<ECHO_PIN))) {
                hcsr04_echo_result_us = TIMER2_GetElapsedTicks(hcsr04_echo_start) * HCSR04_T2_TICK_US;
                hcsr04_state = HCSR04_SM_READY;
            } else if (TIMER2_GetElapsedTicks(hcsr04_echo_start) >= HCSR04_TIMEOUT_TICKS) {
                hcsr04_state = HCSR04_SM_TIMEOUT;
            }
            break;
    }
}

// ===========================================================================
// CALLBACKS HCSR04
// ===========================================================================

static void MyTrigger(void *context) {
    (void)context;
    if (hcsr04_state != HCSR04_SM_IDLE) return;
    hcsr04_echo_result_us = 0;
    hcsr04_trigger_start  = TIMER2_GetTicks();
    PORTB |= (1<<TRIG_PIN);
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
// CLASSIFY_Task — clasificación y despacho de pateadores (Modo Normal)
// ===========================================================================

static void CLASSIFY_Task(void) {

    /* --- MEF multi-muestra: recolecta medidas mientras IR0 este activo --- */
    switch(cmeas_state) {

        case CMEAS_IDLE:
            if (IR_FallingEdge(IR0)) {
                meas_count   = 0;
                meas_wait_ms = 0;
                cmeas_state  = CMEAS_COLLECTING;
            }
            break;

        case CMEAS_COLLECTING:
            // Disparar nueva medicion si sensor libre, wait expirado, IR0 aun LOW y hay espacio
            if (hcsr04_state == HCSR04_SM_IDLE && meas_wait_ms == 0
                && meas_count < MAX_MEAS_SAMPLES && IR0_IS_LOW()) {
                HCSR04_MeasureCm(&sensor, &distancia_cm);
            }
            // Almacenar resultado valido
            if (hcsr04_state == HCSR04_SM_READY) {
                if (HCSR04_MeasureCm(&sensor, &distancia_cm) == HCSR04_OK) {
                    meas_buf[meas_count++] = (uint8_t)distancia_cm;
                }
                meas_wait_ms = MEAS_RETRY_WAIT_MS;
            }
            // Timeout: descartar muestra, esperar antes de reintentar
            if (hcsr04_state == HCSR04_SM_TIMEOUT) {
                HCSR04_MeasureCm(&sensor, &distancia_cm);
                meas_wait_ms = MEAS_RETRY_WAIT_MS;
            }
            // Finalizar cuando IR0 volvio a HIGH y el sensor esta libre, o buffer lleno
            if ((!IR0_IS_LOW() && hcsr04_state == HCSR04_SM_IDLE)
                || meas_count >= MAX_MEAS_SAMPLES) {
                if (meas_count > 0) {
                    uint16_t sum = 0;
                    uint8_t  j;
                    for(j = 0; j < meas_count; j++) sum += meas_buf[j];
                    uint8_t d_avg = (uint8_t)(sum / meas_count);
                    COM_SendFrame(CMD_DIST_MEAS, &d_avg, 1);
                    _eBoxType tipo = CLASSIFIER_Classify(d_avg);
                    IRTARGET_RegisterBox(tipo);
                    // cnt[] NO se incrementa aqui: se suma cuando el servo patea
                } else {
                    COM_SendFrame(CMD_ERR_SENSOR, 0, 0);  // todas las medidas fallaron
                }
                cmeas_state = CMEAS_IDLE;
            }
            break;
    }

    /* --- Delay 150ms antes de disparar el servo --- */

    // Disparos pendientes: si el delay expiro, ejecutar y sumar contador
    for (uint8_t i = 0; i < 3; i++) {
        if (fire_pending[i] && fire_delay[i] == 0 && !servo_active[i]) {
            fire_pending[i] = 0;
            servo_active[i] = 1;
            servo_hold[i]   = SERVO_HOLD_MS;
            SERVO_Set((_eServoID)i, SERVO_PUSH);
            uint8_t ep;
            if      (i == 0) { ep = (uint8_t)BOX_SMALL;  cnt[BOX_SMALL]++;  }
            else if (i == 1) { ep = (uint8_t)BOX_MEDIUM; cnt[BOX_MEDIUM]++; }
            else             { ep = (uint8_t)BOX_BIG;    cnt[BOX_BIG]++;    }
            COM_SendFrame(CMD_BOX_EJECTED, &ep, 1);
        }
    }

    // IR pateadores → registrar disparo con delay
    if (IR_FallingEdge(IR1) && !servo_active[0] && !fire_pending[0]) {
        if (IRTARGET_Tick(PUSHER_1)) { fire_pending[0]=1; fire_delay[0]=SERVO_FIRE_DELAY_MS; }
    }
    if (IR_FallingEdge(IR2) && !servo_active[1] && !fire_pending[1]) {
        if (IRTARGET_Tick(PUSHER_2)) { fire_pending[1]=1; fire_delay[1]=SERVO_FIRE_DELAY_MS; }
    }
    if (IR_FallingEdge(IR3) && !servo_active[2] && !fire_pending[2]) {
        if (IRTARGET_Tick(PUSHER_3)) { fire_pending[2]=1; fire_delay[2]=SERVO_FIRE_DELAY_MS; }
    }
}

// ===========================================================================
// COM_Task — recepcion y procesamiento de comandos de la GUI
// ===========================================================================

static void ProcessCmd(uint8_t cmd, uint8_t *payload) {
    uint8_t ack[2];

    switch(cmd) {

        case CMD_GET_STATE: {
            uint8_t st = 0;
            if      (servo_active[0]) st = 1;
            else if (servo_active[1]) st = 2;
            else if (servo_active[2]) st = 3;
            COM_SendFrame(CMD_STATE, &st, 1);
            break;
        }

        case CMD_GET_COUNTS: {
            uint8_t p[8] = {
                0, (uint8_t)cnt[BOX_SMALL],
                0, (uint8_t)cnt[BOX_MEDIUM],
                0, (uint8_t)cnt[BOX_BIG],
                0, (uint8_t)cnt[BOX_NONE]
            };
            COM_SendFrame(CMD_COUNTS, p, 8);
            break;
        }

        case CMD_RESET_COUNTS:
            cnt[0] = cnt[1] = cnt[2] = cnt[3] = 0;
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        case CMD_SET_THRESH:
            classifier_cfg.h_small  = payload[0];
            classifier_cfg.h_medium = payload[1];
            classifier_cfg.h_big    = payload[2];
            CLASSIFIER_SetConfig(classifier_cfg);
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        case CMD_SET_CALIB:
            classifier_cfg.ref_dist = payload[0];
            CLASSIFIER_SetConfig(classifier_cfg);
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        case CMD_SET_CONVEYOR:
            if(payload[0]) CONVEYOR_Start();
            else           CONVEYOR_Stop();
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

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

static void COM_Task(void) {
    uint8_t cmd, payload[COM_PAYLOAD_SIZE], len;
    COM_Update();
    if (COM_FrameAvailable()) {
        COM_GetFrame(&cmd, payload, &len);
        ProcessCmd(cmd, payload);
    }
}

// ===========================================================================
// INIT GPIOs
// ===========================================================================

static void ini_GPIOs(void) {
    DDRB |=  (1<<LED_BUILTIN);
    DDRB |=  (1<<TRIG_PIN);
    DDRB &= ~(1<<ECHO_PIN);
    PORTB &= ~(1<<TRIG_PIN);
    PORTB &= ~(1<<ECHO_PIN);
}

// ===========================================================================
// ON1MS — tick periódico 1ms (desde ISR Timer0)
// ===========================================================================

static void On1ms(void) {
    if (!time100ms) time100ms = 100;
    time100ms--;

    IR_Update();

    if (meas_wait_ms > 0) meas_wait_ms--;

    for (uint8_t i = 0; i < 3; i++) {
        if (fire_delay[i] > 0) fire_delay[i]--;
        if (servo_active[i]) {
            if (servo_hold[i] > 0) { servo_hold[i]--; }
            else { servo_active[i] = 0; SERVO_Set((_eServoID)i, SERVO_HOME); }
        }
    }

    GPIOR0 &= ~(1<<GPIOR00);
}

// ===========================================================================
// HEARTBEAT — toggle LED cada 100ms
// ===========================================================================

static void Heartbeat(void) {
    if (!time100ms) { time100ms = 100; PORTB ^= (1<<LED_BUILTIN); }
}

// ===========================================================================
// ISR TIMER0 — genera tick 1ms
// ===========================================================================

ISR(TIMER0_COMPA_vect) {
    OCR0A += 249;
    GPIOR0 |= (1<<GPIOR00);
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
    CLASSIFIER_Init();
    IRTARGET_Init();
    COM_Init();
	CONVEYOR_Init();
	
	CONVEYOR_Start();

    sei();

    while(1) {
        Heartbeat();
        HCSR04_Task();
        if (GPIOR0 & (1<<GPIOR00)) { On1ms(); }
        HCSR04_Task();
        CLASSIFY_Task();
        HCSR04_Task();
        COM_Task();
    }
}
