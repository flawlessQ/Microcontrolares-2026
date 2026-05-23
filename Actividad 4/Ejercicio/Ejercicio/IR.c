/*
  IR.c

  Driver para los sensores infrarrojos TCRT5000.

  El sensor es activo-bajo: detecta caja cuando la salida digital va a '0'.
  El debounce de 20ms filtra rebotes mec�nicos y falsas lecturas cortas.
  Cada sensor mantiene un contador de tiempo de bloqueo que se usa para
  calcular la velocidad de la caja al pasar.

  IR_UpdateDebounce() debe llamarse exactamente cada 1ms desde On1ms().
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include "IR.h"			// Se incluye el archivo de cabecera propio.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

#define IR_COUNT          4u		// Cantidad de sensores IR en el sistema.
#define IR_DEBOUNCE_MS    20u		// Tiempo de debounce en milisegundos.
#define IR_BOX_WIDTH_MM   100u		// Ancho estimado de la caja en mm (para c�lculo de velocidad).

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|												VARIABLES INTERNAS		                                             |
//|__________________________________________________________________________________________________________________|

// Tabla de pines f�sicos asociada a cada ID de sensor IR.
static const uint8_t ir_pin[IR_COUNT] =
{
	PIND5,		// IR0: zona de medici�n (D5).
	PIND2,		// IR1: posici�n del pateador 1 (D2).
	PIND3,		// IR2: posici�n del pateador 2 (D3).
	PIND4		// IR3: posici�n del pateador 3 (D4).
};

static uint8_t ir_stable_state[IR_COUNT]   = {0};	// Estado estable (post-debounce) de cada sensor.
static uint8_t ir_last_raw[IR_COUNT]       = {0};	// �ltimo valor crudo le�do, para detectar cambios.
static uint8_t ir_debounce_count[IR_COUNT] = {0};	// Contador de ms sin cambio de estado (debounce).

static uint8_t ir_rising_edge[IR_COUNT]    = {0};	// Bandera de flanco ascendente estable pendiente de leer.
static uint8_t ir_falling_edge[IR_COUNT]   = {0};	// Bandera de flanco descendente estable pendiente de leer.

static uint16_t ir_time_ms[IR_COUNT]       = {0};	// Tiempo en ms que el sensor lleva detectando una caja.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											C�DIGO DE FUNCIONES			                                             |
//|__________________________________________________________________________________________________________________|

void IR_Init(void)
{
// En esta funci�n se inicializan los pines de entrada y se limpian todos los estados internos del driver.

	uint8_t i;

	// Configuro los pines PD2-PD5 como entradas, sin pull-up interno.
	// El TCRT5000 maneja activamente su salida: no se necesita pull-up.
	DDRD &= ~((1 << PIND2) | (1 << PIND3) | (1 << PIND4) | (1 << PIND5));	// PD2-PD5 como entradas (sin cambio de m�scara).

	for(i = 0; i < IR_COUNT; i++)				// Recorro todos los sensores.
	{
		ir_stable_state[i]   = 0;				// Estado estable inicial: sin caja detectada.
		ir_last_raw[i]       = 0;				// �ltimo valor crudo inicial: sin caja.
		ir_debounce_count[i] = 0;				// Contador de debounce en cero.
		ir_rising_edge[i]    = 0;				// Sin flanco ascendente pendiente.
		ir_falling_edge[i]   = 0;				// Sin flanco descendente pendiente.
		ir_time_ms[i]        = 0;				// Tiempo de bloqueo en cero.
	}
}

static uint8_t IR_ReadRaw(_eIRID id)
{
// En esta funci�n se lee el pin f�sico del sensor y se devuelve el estado l�gico.
// El TCRT5000 es activo-bajo: pin en '0' cuando detecta un objeto.
// Se invierte la lectura para que '1' signifique "caja detectada".

	return !(PIND & (1 << ir_pin[id]));			// Invierte la lectura: activo-bajo -> '1' al detectar.
}

void IR_UpdateDebounce(void)
{
// En esta funci�n se actualiza el debounce, los flancos y el tiempo de bloqueo de cada sensor.
// Debe llamarse exactamente cada 1ms desde On1ms().

	uint8_t i;
	uint8_t raw;

	for(i = 0; i < IR_COUNT; i++)				// Recorro todos los sensores.
	{
		raw = IR_ReadRaw((_eIRID)i);			// Leo el estado crudo del sensor actual.

		if(raw == ir_last_raw[i])				// Compruebo si el estado no cambi� respecto a la �ltima lectura.
		{
			if(ir_debounce_count[i] < IR_DEBOUNCE_MS)	// Si el contador a�n no alcanz� el tiempo de debounce...
			{
				ir_debounce_count[i]++;			// ...incremento el contador de estabilidad.
			}
			else								// El estado lleva m�s de IR_DEBOUNCE_MS ms estable: lectura v�lida.
			{
				if(ir_stable_state[i] != raw)	// Compruebo si el estado estable cambi� respecto al confirmado anterior.
				{
					ir_stable_state[i] = raw;	// Actualizo el estado estable con el nuevo valor confirmado.

					if(raw)
					{
						// Flanco ascendente estable: la caja acaba de aparecer frente al sensor.
						ir_rising_edge[i] = 1;	// Marco la bandera de flanco ascendente.
						ir_time_ms[i]     = 0;	// Reinicio el contador de tiempo de bloqueo.
					}
					else
					{
						// Flanco descendente estable: la caja acaba de pasar y ya no bloquea el sensor.
						ir_falling_edge[i] = 1;	// Marco la bandera de flanco descendente.
					}
				}
			}
		}
		else									// El estado cambi�: reinicio el debounce para este sensor.
		{
			ir_last_raw[i]       = raw;			// Guardo el nuevo valor como referencia para la pr�xima comparaci�n.
			ir_debounce_count[i] = 0;			// Reinicio el contador: hay que esperar otros IR_DEBOUNCE_MS ms estables.
		}

		// Si la caja est� detectada de forma estable, incremento el tiempo de bloqueo.
		if(ir_stable_state[i])
		{
			if(ir_time_ms[i] < 60000u)			// Limito el contador a 60000ms para evitar desbordamiento.
			{
				ir_time_ms[i]++;				// Incremento el tiempo de bloqueo en 1ms.
			}
		}
	}
}

uint8_t IR_IsDetected(_eIRID id)
{
// En esta funci�n se devuelve el estado estable del sensor indicado.

	return ir_stable_state[id];				// Devuelvo 1 si hay caja detectada, 0 si no.
}

uint8_t IR_RisingEdge(_eIRID id)
{
// En esta funci�n se devuelve y consume la bandera de flanco ascendente del sensor indicado.
// La bandera se resetea al leerla: devuelve 1 una �nica vez por cada evento de llegada de caja.

	if(ir_rising_edge[id])
	{
		ir_rising_edge[id] = 0;				// Consumo la bandera para que no se repita en la pr�xima lectura.
		return 1;							// Devuelvo 1: hubo un flanco ascendente estable.
	}

	return 0;								// No hab�a flanco ascendente pendiente.
}

uint8_t IR_UpdateBoxSpeed(_eIRID id, uint16_t *speed_mm_s)
{
// En esta funci�n se calcula la velocidad de la caja cuando el sensor detecta que ya pas�.
// Devuelve 1 si hay un dato de velocidad nuevo; escribe el resultado en *speed_mm_s.

	if(ir_falling_edge[id])							// Compruebo si el sensor acaba de liberar la caja.
	{
		ir_falling_edge[id] = 0;					// Consumo la bandera de flanco descendente.

		if(ir_time_ms[id] > 0)						// Verifico que el tiempo de bloqueo sea v�lido (distinto de cero).
		{
			// Calculo velocidad: v = ancho_caja / tiempo_de_paso.
			// Se multiplica por 1000UL para convertir de mm/ms a mm/s antes de dividir.
			*speed_mm_s = (uint16_t)(((uint32_t)IR_BOX_WIDTH_MM * 1000UL) / ir_time_ms[id]);
		}
		else
		{
			*speed_mm_s = 0;						// Tiempo nulo: velocidad indeterminada, devuelvo cero.
		}

		return 1;									// Devuelvo 1: hay un dato de velocidad nuevo disponible.
	}

	return 0;										// No hay dato nuevo de velocidad.
}

uint16_t IR_GetBlockTimeMs(_eIRID id)
{
// En esta funci�n se devuelve el tiempo en ms que el sensor estuvo bloqueado.
// El valor es v�lido desde el flanco descendente hasta el pr�ximo flanco ascendente.

	return ir_time_ms[id];		// Devuelvo el tiempo acumulado desde el �ltimo flanco ascendente.
}
