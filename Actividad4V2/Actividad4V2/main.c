#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "TIMERS.h"
#include "SERVO.h"
#include "IR.h"

// ===========================================================================
// PINES (solo los que ningun modulo inicializa)
// ===========================================================================

#define LED_BUILTIN  PINB5
#define TRIG_PIN     PB1
#define ECHO_PIN     PB2

#define SERVO_HOLD_MS 500

// ===========================================================================
// VARIABLES
// ===========================================================================

uint8_t  time100ms       = 100;
static uint16_t servo_hold[3]   = {0, 0, 0};
static uint8_t  servo_active[3] = {0, 0, 0};

// ===========================================================================
// GPIOs
// ===========================================================================

static void ini_GPIOs(void){
    // LED heartbeat
    DDRB |= (1 << LED_BUILTIN);

    // HC-SR04: TRIG salida, ECHO entrada sin pull-up
    DDRB |=  (1 << TRIG_PIN);
    DDRB &= ~(1 << ECHO_PIN);
    PORTB &= ~(1 << ECHO_PIN);
}

// ===========================================================================
// ON1MS
// ===========================================================================

static void On1ms(void){
    if(!time100ms) time100ms = 100;
    time100ms--;

    IR_Update();

    for(uint8_t i = 0; i < 3; i++){
        if(servo_active[i]){
            if(servo_hold[i] > 0){
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

void Heartbeat(){
    if(!time100ms){
        time100ms = 100;
        PORTB ^= (1 << LED_BUILTIN);
    }
}

// ===========================================================================
// ISR TIMER0 - tick 1ms
// ===========================================================================

ISR(TIMER0_COMPA_vect){
    OCR0A += 249;
    GPIOR0 |= (1 << GPIOR00);
}

// ===========================================================================
// MAIN
// ===========================================================================

int main(void){
    cli();

    ini_TIMER0();
    ini_TIMER1();
    ini_GPIOs();
    SERVO_Init();
    IR_Init();

    sei();

    while(1){

        Heartbeat();

        if(GPIOR0 & (1 << GPIOR00)){
	        	On1ms();
        }

        if(IR_FallingEdge(IR0) && !servo_active[0]){
            servo_active[0] = 1;
            servo_hold[0]   = SERVO_HOLD_MS;
            SERVO_Set(SERVO_1, SERVO_PUSH);
        }

        if(IR_FallingEdge(IR1) && !servo_active[1]){
            servo_active[1] = 1;
            servo_hold[1]   = SERVO_HOLD_MS;
            SERVO_Set(SERVO_2, SERVO_PUSH);
        }

        if(IR_FallingEdge(IR2) && !servo_active[2]){
            servo_active[2] = 1;
            servo_hold[2]   = SERVO_HOLD_MS;
            SERVO_Set(SERVO_3, SERVO_PUSH);
        }
    }
}
