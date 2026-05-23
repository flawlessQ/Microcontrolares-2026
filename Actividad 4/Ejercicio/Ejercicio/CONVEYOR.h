#ifndef CONVEYOR_CONTROL_H_
#define CONVEYOR_CONTROL_H_

#include <avr/io.h>
#include <stdint.h>

/* A0 en ATmega328P corresponde a PC0 */
#define CONVEYOR_CTRL_PIN    PC0

#define CONVEYOR_OFF         0u
#define CONVEYOR_ON          1u

void ConveyorControl_Init(void);
void ConveyorControl_Start(void);
void ConveyorControl_Stop(void);
void ConveyorControl_Update(void);
uint8_t ConveyorControl_IsRunning(void);

#endif /* CONVEYOR_CONTROL_H_ */
