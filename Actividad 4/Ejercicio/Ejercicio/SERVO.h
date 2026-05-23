/*
  SERVO.h

  Driver de software PWM para tres servomotores SG90 sobre ATmega328P.
  Se�al de control: per�odo 20ms (50Hz).
      SERVO_HOME -> pulso 1ms -> posici�n retra�da  (0�).
      SERVO_PUSH -> pulso 2ms -> posici�n extendida (180�).

  Conexiones f�sicas:
      SERVO1 -> PD7  (eyector caja peque�a)
      SERVO2 -> PB3  (eyector caja mediana)
      SERVO3 -> PB4  (eyector caja grande)

  Los pines deben estar configurados como salida antes de llamar a SERVO_Init().
*/

#ifndef SERVO_H_
#define SERVO_H_

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include <avr/io.h>		// Esta librer�a incluye acceso a los registros del microcontrolador.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													TYPEDEFS			                                             |
//|__________________________________________________________________________________________________________________|

typedef enum {
	SERVO_HOME = 1,		// Posici�n retra�da:  pulso de 1ms -> 0�.
	SERVO_PUSH = 2		// Posici�n de empuje: pulso de 2ms -> 180�.
} _eServoPos;

typedef enum {
	SERVO_ID_1 = 0,		// Servo 1 - PD7 (D7)  (eyector caja peque�a).
	SERVO_ID_2 = 1,		// Servo 2 - PB3 (D11) (eyector caja mediana).
	SERVO_ID_3 = 2		// Servo 3 - PB4 (D12) (eyector caja grande).
} _eServoID;

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											FUNCIONES PROTOTIPO			                                             |
//|__________________________________________________________________________________________________________________|

void SERVO_Init(void);
/*	Inicializa los tres servos en posici�n HOME (pulso 1ms).
	Debe llamarse antes de iniciar el bucle principal.			*/

void SERVO_Set(_eServoID id, _eServoPos pos);
/*	Establece la posici�n del servo indicado.
	El cambio se aplica en el siguiente ciclo PWM (m�ximo 20ms).	*/

void SERVO_On1ms(void);
/*	Genera la se�al PWM de los tres servos.
	Debe llamarse desde ISR(TIMER1_COMPA_vect) exactamente cada 1ms.	*/

#endif /* SERVO_H_ */
