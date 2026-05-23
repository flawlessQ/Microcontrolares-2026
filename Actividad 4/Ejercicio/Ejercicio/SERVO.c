/*
    SERVO.c

    Driver de software PWM para tres servomotores SG90 sobre ATmega328P.
    Señal: 50Hz (periodo 20ms), pulso 1ms (HOME/0°) o 2ms (PUSH/180°).

    Conexiones:
        SERVO1 → PD7
        SERVO2 → PB4
        SERVO3 → PB3

    Los pines deben estar configurados como salida antes de llamar a SERVO_Init().
*/

#include "SERVO.h"

#define SERVO_PERIOD_MS  20    // Periodo del PWM en ms (50Hz).

static uint8_t pwm_tick = 0;
static uint8_t pulse_width[3] = {1, 1, 1};  // Ancho de pulso en ms por servo (HOME por defecto).

void SERVO_Init(void)
{
    pulse_width[0] = (uint8_t)SERVO_HOME;
    pulse_width[1] = (uint8_t)SERVO_HOME;
    pulse_width[2] = (uint8_t)SERVO_HOME;
    pwm_tick = 0;
}

void SERVO_Set(_eServoID id, _eServoPos pos)
{
    pulse_width[id] = (uint8_t)pos;
}

void SERVO_On1ms(void)
{
    pwm_tick++;
    if (pwm_tick >= SERVO_PERIOD_MS)
        pwm_tick = 0;

    // SERVO1 — PD7
    if (pwm_tick == 0)
        PORTD |=  (1 << PIND7);
    if (pwm_tick == pulse_width[SERVO_ID_1])
        PORTD &= ~(1 << PIND7);

    // SERVO2 — PB4
    if (pwm_tick == 0)
        PORTB |=  (1 << PINB4);
    if (pwm_tick == pulse_width[SERVO_ID_2])
        PORTB &= ~(1 << PINB4);

    // SERVO3 — PB3
    if (pwm_tick == 0)
        PORTB |=  (1 << PINB3);
    if (pwm_tick == pulse_width[SERVO_ID_3])
        PORTB &= ~(1 << PINB3);
}
