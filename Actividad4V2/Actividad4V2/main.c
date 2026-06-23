/*
 * Actividad4V2.c
 *
 * Created: 23/5/2026 15:33:39
 * Author : juani
 *
 * Sistema de clasificacion de cajas en cinta transportadora.
 * Plataforma: ATmega328P @ 16MHz (Arduino Uno R3)
 *
 * Modulos:
 *   TIMERS      - Timer0 1ms tick | Timer1 PWM servos | Timer2 1us polling
 *   IR          - Sensores TCRT5000 con debounce por software (20ms)
 *   HCSR04      - Sensor ultrasonico, medicion con Timer1 prestado
 *   SERVO       - PWM por software para 3 servos SG90 via ISR Timer1
 *   CLASSIFIER  - Clasifica cajas por altura (small/medium/big/none)
 *   IRTARGET    - Cola FIFO por pateador: cuantas cajas ignorar antes de disparar
 *   CONVEYOR    - Habilitacion/deshabilitacion de la cinta (PC0)
 *   COMUNICATION- UART 115200 8N1 + protocolo UNER (trama + checksum XOR)
 *
 * Pinout:
 *   PD2 = IR1 (pateador 1)    PD3 = IR2 (pateador 2)
 *   PD4 = IR3 (pateador 3)    PD5 = IR0 (clasificador)
 *   PD7 = SERVO1              PB3 = SERVO2
 *   PB4 = SERVO3              PB1 = TRIG HC-SR04
 *   PB2 = ECHO HC-SR04        PC0 = CONVEYOR enable
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "CLASSIFIER.h"
#include "COMUNICATION.h"
#include "CONVEYOR.h"
#include "HCSR04.h"
#include "IR.h"
#include "IRTARGET.h"
#include "SERVO.h"
#include "TIMERS.h"

// ===========================================================================
// DEFINICIONES
// ===========================================================================

#define TRIG_PIN        PB1     // Pin de disparo del HC-SR04
#define ECHO_PIN        PB2     // Pin de eco del HC-SR04
#define LED_BUILTIN     PB5     // Led builtin del Arduino (PB5 = pin 13)

#define MODE_NORMAL     0       // Modo normal: pateador espera deteccion IR
#define MODE_ESTIMATED  1       // Modo estimado: pateador dispara por timer fijo

#define EST_DELAY_DEFAULT   2000    // Delay inicial hasta que se mida la velocidad real
#define SERVO_HOLD_MS       500     // Tiempo que el servo permanece en posicion PUSH
#define POLL_GUI_MS         500     // Intervalo de envio automatico de contadores a la GUI

// Distancias entre sensores IR (cm)
// Usadas para extrapolar delays de IR2 e IR3 a partir del tiempo medido IR0->IR1
#define DIST_IR0_IR1    44          // IR0 a IR1: 44cm
#define DIST_IR0_IR2    87          // IR0 a IR2: 44+43cm
#define DIST_IR0_IR3    133         // IR0 a IR3: 44+43+46cm

// ===========================================================================
// VARIABLES GLOBALES
// ===========================================================================

static _sHCSR04_t sensor;

static uint8_t  conv_mode       = MODE_NORMAL;
static uint8_t  servo_active[3] = {0,0,0};
static uint16_t servo_hold[3]   = {0,0,0};
static uint16_t box_count[4]    = {0,0,0,0};
static uint16_t poll_timer      = 0;
static uint8_t  time100ms       = 100;

static uint16_t est_delay[3]    = {EST_DELAY_DEFAULT, EST_DELAY_DEFAULT, EST_DELAY_DEFAULT};
static uint16_t est_timer[3]    = {0, 0, 0};
static uint32_t ms_counter      = 0;
static uint32_t speed_t0        = 0;

// ===========================================================================
// FUNCIONES AUXILIARES DEL HC-SR04
// ===========================================================================

static void delay_us(uint8_t us){
    while(us--){
        TIFR2 |= (1 << OCF2A);
        while(!(TIFR2 & (1 << OCF2A)));
    }
}

static void MyTrigger(void *context){
    (void)context;
    DDRB  |=  (1 << TRIG_PIN);
    PORTB |=  (1 << TRIG_PIN);
    delay_us(10);
    PORTB &= ~(1 << TRIG_PIN);
}

static uint8_t MyReadEcho(void *context, uint32_t *echo_us){
    uint16_t timeout;
    (void)context;

    DDRB &= ~(1 << ECHO_PIN);

    TIMSK1 &= ~(1 << OCIE1A);
    TCCR1A  = 0;
    TCCR1B  = (1 << CS11);
    TCNT1   = 0;

    timeout = 10000;
    while(!(PINB & (1 << ECHO_PIN))){
        if(--timeout == 0) goto restore;
    }

    TCNT1 = 0;

    timeout = 60000;
    while(PINB & (1 << ECHO_PIN)){
        if(--timeout == 0) goto restore;
    }

    *echo_us = (uint32_t)TCNT1 / 2;

    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1AH = 0;
    OCR1AL = 249;
    TCNT1H = 0;
    TCNT1L = 0;
    TIMSK1 |= (1 << OCIE1A);
    return 1;

restore:
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1AH = 0;
    OCR1AL = 249;
    TCNT1H = 0;
    TCNT1L = 0;
    TIMSK1 |= (1 << OCIE1A);
    return 0;
}

// ===========================================================================
// HELPER: DISPARAR UN PATEADOR
// ===========================================================================
static void FirePusher(uint8_t pusher){
    if(servo_active[pusher]) return;
    servo_active[pusher] = 1;
    servo_hold[pusher]   = SERVO_HOLD_MS;
    SERVO_Set((_eServoID)pusher, SERVO_PUSH);
}

// ===========================================================================
// HELPER: ENVIAR CONTADORES A LA GUI
// ===========================================================================
static void SendCounts(void){
    uint8_t p[8];
    p[0] = (uint8_t)(box_count[BOX_SMALL]  >> 8);
    p[1] = (uint8_t)(box_count[BOX_SMALL]  & 0xFF);
    p[2] = (uint8_t)(box_count[BOX_MEDIUM] >> 8);
    p[3] = (uint8_t)(box_count[BOX_MEDIUM] & 0xFF);
    p[4] = (uint8_t)(box_count[BOX_BIG]    >> 8);
    p[5] = (uint8_t)(box_count[BOX_BIG]    & 0xFF);
    p[6] = (uint8_t)(box_count[BOX_NONE]   >> 8);
    p[7] = (uint8_t)(box_count[BOX_NONE]   & 0xFF);
    COM_SendFrame(CMD_COUNTS, p, 8);
}

// ===========================================================================
// HELPER: PROCESAR COMANDO UART RECIBIDO
// ===========================================================================
static void ProcessCmd(uint8_t cmd, uint8_t *payload){
    uint8_t busy = servo_active[0] || servo_active[1] || servo_active[2];
    uint8_t ack[2];

    switch(cmd){

        case CMD_GET_STATE:{
            uint8_t st = 0;
            COM_SendFrame(CMD_STATE, &st, 1);
            break;
        }

        case CMD_GET_COUNTS:
            SendCounts();
            break;

        case CMD_SET_MODE:
            if(busy){ ack[0]=cmd; ack[1]=ACK_BUSY; COM_SendFrame(CMD_ACK,ack,2); break; }
            conv_mode = payload[0];
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        case CMD_SET_THRESH:{
            if(busy){ ack[0]=cmd; ack[1]=ACK_BUSY; COM_SendFrame(CMD_ACK,ack,2); break; }
            _sClassifierConfig cfg;
            cfg.h_small  = payload[0];
            cfg.h_medium = payload[1];
            cfg.h_big    = payload[2];
            cfg.ref_dist = 20;
            CLASSIFIER_SetConfig(cfg);
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;
        }

        case CMD_SET_CALIB:{
            if(busy){ ack[0]=cmd; ack[1]=ACK_BUSY; COM_SendFrame(CMD_ACK,ack,2); break; }
            _sClassifierConfig cfg;
            cfg.ref_dist = payload[0];
            cfg.h_small  = 6;
            cfg.h_medium = 8;
            cfg.h_big    = 10;
            CLASSIFIER_SetConfig(cfg);
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;
        }

        case CMD_RESET_COUNTS:
            box_count[0]=0; box_count[1]=0;
            box_count[2]=0; box_count[3]=0;
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        case CMD_SET_SPEED:{
            if(busy){ ack[0]=cmd; ack[1]=ACK_BUSY; COM_SendFrame(CMD_ACK,ack,2); break; }
            est_delay[0] = ((uint16_t)payload[0] << 8) | payload[1];
            est_delay[1] = ((uint16_t)payload[2] << 8) | payload[3];
            est_delay[2] = ((uint16_t)payload[4] << 8) | payload[5];
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;
        }

        case CMD_SET_SERVO:
            SERVO_SetConfig((_eServoID)payload[0], payload[1], payload[2]);
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        case CMD_SET_CONVEYOR:
            if(payload[0]) CONVEYOR_Start(); else CONVEYOR_Stop();
            ack[0]=cmd; ack[1]=ACK_OK;
            COM_SendFrame(CMD_ACK, ack, 2);
            break;

        default: break;
    }
}

// ===========================================================================
// HEARTBEAT
// ===========================================================================
static void Heartbeat(void){
    if(!time100ms){
        time100ms = 100;
        PORTB ^= (1 << LED_BUILTIN);
    }
}

// ===========================================================================
// ISR TIMER0 COMPARE A - TICK DE 1ms
// ===========================================================================
ISR(TIMER0_COMPA_vect){
    GPIOR0 |= (1 << 0);
    ms_counter++;
}

// ===========================================================================
// MAIN
// ===========================================================================
int main(void){

    CLASSIFIER_Init();
    COM_Init();
    CONVEYOR_Init();
    IR_Init();
    IRTARGET_Init();
    SERVO_Init();
    ini_TIMER0();
    ini_TIMER1();
    ini_TIMER2();
    HCSR04_Init(&sensor, MyTrigger, MyReadEcho, 0);

    DDRB |= (1 << LED_BUILTIN);

    CONVEYOR_Start();
    sei();

    while(1){

        Heartbeat();

        if(GPIOR0 & (1 << 0)){
            GPIOR0 &= ~(1 << 0);

            IR_Update();

            uint8_t i;
            for(i = 0; i < 3; i++){
                if(servo_active[i] && servo_hold[i] > 0){
                    servo_hold[i]--;
                    if(servo_hold[i] == 0){
                        SERVO_Set((_eServoID)i, SERVO_HOME);
                        servo_active[i] = 0;
                        box_count[i]++;
                        uint8_t p = i + 1;
                        COM_SendFrame(CMD_BOX_EJECTED, &p, 1);
                    }
                }
            }

            if(conv_mode == MODE_ESTIMATED){
                for(i = 0; i < 3; i++){
                    if(est_timer[i] > 0){
                        est_timer[i]--;
                        if(est_timer[i] == 0) FirePusher(i);
                    }
                }
            }

            if(!time100ms) time100ms = 100;
            time100ms--;

            if(poll_timer > 0) poll_timer--;
            if(poll_timer == 0){
                poll_timer = POLL_GUI_MS;
                SendCounts();
            }
        }

        if(IR_FallingEdge(IR0)){
            speed_t0 = ms_counter;
            uint16_t d_cm;
            _sHCSR04_status_t st = HCSR04_MeasureCm(&sensor, &d_cm);

            if(st == HCSR04_OK){
                uint8_t h_payload = (uint8_t)d_cm;
                COM_SendFrame(CMD_DIST_MEAS, &h_payload, 1);

                _eBoxType tipo = CLASSIFIER_Classify((uint8_t)d_cm);

                if(tipo == BOX_NONE){
                    box_count[BOX_NONE]++;
                    COM_SendFrame(CMD_BOX_DISCARDED, 0, 0);
                } else {
                    if(conv_mode == MODE_NORMAL){
                        IRTARGET_RegisterBox(tipo);
                    } else {
                        uint8_t pusher = (tipo == BOX_SMALL)  ? 0 :
                                         (tipo == BOX_MEDIUM) ? 1 : 2;
                        est_timer[pusher] = est_delay[pusher];
                    }
                }
            } else {
                COM_SendFrame(CMD_ERR_SENSOR, 0, 0);
            }
        }

        if(conv_mode == MODE_NORMAL){
            if(IR_FallingEdge(IR1)){
                uint16_t t01 = (uint16_t)(ms_counter - speed_t0);
                est_delay[0] = t01;
                est_delay[1] = (uint16_t)((uint32_t)t01 * DIST_IR0_IR2 / DIST_IR0_IR1);
                est_delay[2] = (uint16_t)((uint32_t)t01 * DIST_IR0_IR3 / DIST_IR0_IR1);

                if(t01 > 0){
                    uint16_t speed_mm_s = (uint16_t)(440000UL / t01);
                    uint8_t sp[2];
                    sp[0] = (uint8_t)(speed_mm_s >> 8);
                    sp[1] = (uint8_t)(speed_mm_s & 0xFF);
                    COM_SendFrame(CMD_BELT_SPEED, sp, 2);
                }

                if(IRTARGET_Tick(PUSHER_1)) FirePusher(0);
            }
            if(IR_FallingEdge(IR2)) if(IRTARGET_Tick(PUSHER_2)) FirePusher(1);
            if(IR_FallingEdge(IR3)) if(IRTARGET_Tick(PUSHER_3)) FirePusher(2);
        }

        COM_Update();
        if(COM_FrameAvailable()){
            uint8_t cmd, payload[COM_PAYLOAD_SIZE], len;
            COM_GetFrame(&cmd, payload, &len);
            ProcessCmd(cmd, payload);
        }
    }
}
