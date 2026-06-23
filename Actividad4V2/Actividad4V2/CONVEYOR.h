/*
 * CONVEYOR.h
 *
 * Created: 23/5/2026 17:24:31
 *  Author: juani
 */

#ifndef CONVEYOR_H_
#define CONVEYOR_H_

#include <avr/io.h>
#include <stdint.h>

#define CONVEYOR_PIN    PC0

void CONVEYOR_Init();
void CONVEYOR_Start();
void CONVEYOR_Stop();
uint8_t CONVEYOR_IsRunning();

#endif /* CONVEYOR_H_ */
