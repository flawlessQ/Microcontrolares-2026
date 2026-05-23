#include "IR.h"

#define IR_COUNT          4u
#define IR_DEBOUNCE_MS    20u
#define IR_BOX_WIDTH_MM   100u

static const uint8_t ir_pin[IR_COUNT] =
{
	PIND2,
	PIND3,
	PIND4,
	PIND5
};

static uint8_t ir_stable_state[IR_COUNT] = {0};
static uint8_t ir_last_raw[IR_COUNT] = {0};
static uint8_t ir_debounce_count[IR_COUNT] = {0};

static uint8_t ir_rising_edge[IR_COUNT] = {0};
static uint8_t ir_falling_edge[IR_COUNT] = {0};

static uint16_t ir_time_ms[IR_COUNT] = {0};

void IR_Init(void)
{
	uint8_t i;

	DDRD &= ~((1 << PIND2) | (1 << PIND3) | (1 << PIND4) | (1 << PIND5));

	for(i = 0; i < IR_COUNT; i++)
	{
		ir_stable_state[i] = 0;
		ir_last_raw[i] = 0;
		ir_debounce_count[i] = 0;

		ir_rising_edge[i] = 0;
		ir_falling_edge[i] = 0;

		ir_time_ms[i] = 0;
	}
}

static uint8_t IR_ReadRaw(_eIRID id)
{
	// TCRT5000: activo bajo.
	return !(PIND & (1 << ir_pin[id]));
}

void IR_UpdateDebounce(void)
{
	uint8_t i;
	uint8_t raw;

	for(i = 0; i < IR_COUNT; i++)
	{
		raw = IR_ReadRaw((_eIRID)i);

		if(raw == ir_last_raw[i])
		{
			if(ir_debounce_count[i] < IR_DEBOUNCE_MS)
			{
				ir_debounce_count[i]++;
			}
			else
			{
				if(ir_stable_state[i] != raw)
				{
					ir_stable_state[i] = raw;

					if(raw)
					{
						// Flanco ascendente estable.
						ir_rising_edge[i] = 1;
						ir_time_ms[i] = 0;
					}
					else
					{
						// Flanco descendente estable.
						ir_falling_edge[i] = 1;
					}
				}
			}
		}
		else
		{
			ir_last_raw[i] = raw;
			ir_debounce_count[i] = 0;
		}

		// Si la caja está detectada estable, cuento el tiempo bloqueado.
		if(ir_stable_state[i])
		{
			if(ir_time_ms[i] < 60000u)
			{
				ir_time_ms[i]++;
			}
		}
	}
}

uint8_t IR_IsDetected(_eIRID id)
{
	return ir_stable_state[id];
}

uint8_t IR_RisingEdge(_eIRID id)
{
	if(ir_rising_edge[id])
	{
		ir_rising_edge[id] = 0;
		return 1;
	}

	return 0;
}

uint8_t IR_UpdateBoxSpeed(_eIRID id, uint16_t *speed_mm_s)
{
	if(ir_falling_edge[id])
	{
		ir_falling_edge[id] = 0;

		if(ir_time_ms[id] > 0)
		{
			*speed_mm_s = (uint16_t)(((uint32_t)IR_BOX_WIDTH_MM * 1000UL) / ir_time_ms[id]);
		}
		else
		{
			*speed_mm_s = 0;
		}

		return 1;
	}

	return 0;
}