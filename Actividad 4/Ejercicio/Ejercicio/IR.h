#ifndef IR_H_
#define IR_H_

#include <avr/io.h>

typedef enum {
	IR_ID_0 = 0,	// PD2 - zona de medicion (IR0)
	IR_ID_1 = 1,	// PD3 - pateador 1      (IR1)
	IR_ID_2 = 2,	// PD4 - pateador 2      (IR2)
	IR_ID_3 = 3,	// PD5 - pateador 3      (IR3)
} _eIRID;

void    IR_Init(void);
uint8_t IR_IsDetected(_eIRID id);	// Retorna 1 si hay objeto, 0 si no hay.

#endif /* IR_H_ */
