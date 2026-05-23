#include "IR.h"

// Pines fisicos de cada sensor IR (todos en puerto D).
static const uint8_t ir_pin[4] = { PIND2, PIND3, PIND4, PIND5 };

void IR_Init(void)
{
	// PD2-PD5 como entradas con pull-up interno.
	// Pull-up mantiene la linea en HIGH cuando el sensor no esta conectado,
	// evitando falsas detecciones por flotacion.
	DDRD  &= ~((1 << PIND2) | (1 << PIND3) | (1 << PIND4) | (1 << PIND5));
	PORTD |=   (1 << PIND2) | (1 << PIND3) | (1 << PIND4) | (1 << PIND5);
}

uint8_t IR_IsDetected(_eIRID id)
{
	// TCRT5000: salida S va a LOW cuando detecta un objeto (activo bajo).
	return !(PIND & (1 << ir_pin[id]));
}
