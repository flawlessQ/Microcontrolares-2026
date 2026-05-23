#ifndef IR_H_
#define IR_H_

#include <avr/io.h>
#include <stdint.h>

typedef enum {
	IR_ID_0 = 0,	// PD2 - zona de medicion
	IR_ID_1 = 1,	// PD3 - pateador 1
	IR_ID_2 = 2,	// PD4 - pateador 2
	IR_ID_3 = 3,	// PD5 - pateador 3
} _eIRID;

void IR_Init(void);

/*
	Se llama cada 1ms.
	Actualiza debounce, flancos y tiempo de bloqueo.
*/
void IR_UpdateDebounce(void);

/*
	Devuelve estado estable:
	1 = detecta caja
	0 = no detecta caja
*/
uint8_t IR_IsDetected(_eIRID id);

/*
	Devuelve 1 una sola vez cuando aparece una caja.
*/
uint8_t IR_RisingEdge(_eIRID id);

/*
	Calcula velocidad cuando la caja sale del sensor.
	Devuelve 1 si hay velocidad nueva.
*/
uint8_t IR_UpdateBoxSpeed(_eIRID id, uint16_t *speed_mm_s);

#endif /* IR_H_ */