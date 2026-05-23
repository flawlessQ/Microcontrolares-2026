#ifndef SERVO_H_
#define SERVO_H_

#include <avr/io.h>

/*
    Driver de software PWM para tres servomotores SG90.
    Usa Timer1 (1ms tick) para generar la señal de 50Hz (periodo 20ms).
    Posiciones disponibles:
        SERVO_HOME = pulso 1ms → posición retraída (0°)
        SERVO_PUSH = pulso 2ms → posición extendida (180°)
*/

typedef enum {
    SERVO_HOME = 1,
    SERVO_PUSH = 2
} _eServoPos;

typedef enum {
    SERVO_ID_1 = 0,   // PD7
    SERVO_ID_2 = 1,   // PB4
    SERVO_ID_3 = 2    // PB3
} _eServoID;

void SERVO_Init(void);
void SERVO_Set(_eServoID id, _eServoPos pos);
void SERVO_On1ms(void);   // Llamar desde ISR(TIMER1_COMPA_vect) cada 1ms.

#endif /* SERVO_H_ */
