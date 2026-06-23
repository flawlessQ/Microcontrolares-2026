/*
 * SERVO.h
 *
 * Created: 23/5/2026 17:23:49
 *  Author: juani
 */ 


#ifndef SERVO_H_
#define SERVO_H_

#include <stdint.h>
#include <avr/io.h>

typedef enum{
	SERVO_1,
	SERVO_2,
	SERVO_3	
}_eServoID;

typedef enum{
	SERVO_HOME,
	SERVO_PUSH
}_eServoPos;

typedef struct{
	uint8_t home_ms;
	uint8_t push_ms;
}_sServoConfig;

void SERVO_Init();
void SERVO_Set(_eServoID servo, _eServoPos posicion);
void SERVO_SetConfig(_eServoID servo, uint8_t home_ms, uint8_t push_ms);




#endif /* SERVO_H_ */