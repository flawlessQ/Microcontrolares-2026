/*
  CLASSIFIER.c

  M�dulo de clasificaci�n de cajas para la cinta transportadora.

  El HC-SR04 mide la distancia desde el sensor hasta la parte superior
  de la caja. La altura real de la caja se obtiene restando la distancia
  medida a la distancia de referencia (sensor -> piso sin caja):

      h = reference_dist_cm - d_medido

  La clasificaci�n usa ventana cerrada de 1cm (+/-0,5cm) por categor�a.
  Solo se acepta la caja si su altura cae exactamente en una de las tres ventanas
  configuradas. Cualquier otra altura devuelve BOX_NONE (descartada).

  La cola FIFO interna (8 entradas m�ximo) registra las cajas detectadas en
  orden de llegada. Cada entrada lleva un timer de cuenta regresiva para el
  Modo Estimado (disparo autom�tico sin sensor IR de posici�n).
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include "CLASSIFIER.h"		// Se incluye el archivo de cabecera propio.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

#define BOX_QUEUE_SIZE    8u	// Capacidad m�xima de la cola FIFO (entradas).
#define CLASSIFIER_TOL_CM 1u	// Ventana de tolerancia por categor�a: +/-0,5cm = 1cm total.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|												VARIABLES INTERNAS		                                             |
//|__________________________________________________________________________________________________________________|

// Distancia de referencia: sensor al transportador sin caja (cm).
// Ajustar seg�n la instalaci�n real antes de flashear al banco.
static uint8_t reference_dist_cm = 20;

typedef struct {
	uint8_t h_small;	// Altura m�nima para caja peque�a  (cm).
	uint8_t h_medium;	// Altura m�nima para caja mediana  (cm).
	uint8_t h_big;		// Altura m�nima para caja grande   (cm).
} _sBoxConfig;

static _sBoxConfig config = {
	.h_small  = 6,		// Caja peque�a:  6cm de altura por defecto.
	.h_medium = 8,		// Caja mediana:  8cm de altura por defecto.
	.h_big    = 10,		// Caja grande:  10cm de altura por defecto.
};

typedef struct {
	_eBoxType type;			// Tipo de caja encolada.
	uint16_t  est_timer;	// Cuenta regresiva en ms; 0 = listo para Modo Estimado.
} _sBoxQueueEntry;

static _sBoxQueueEntry box_queue[BOX_QUEUE_SIZE];	// Buffer circular de la cola FIFO.
static uint8_t queue_head  = 0;		// �ndice de lectura (frente de la cola).
static uint8_t queue_tail  = 0;		// �ndice de escritura (final de la cola).
static uint8_t queue_count = 0;		// Cantidad de entradas actualmente en la cola.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|								C�DIGO DE FUNCIONES — CLASIFICACI�N		                                             |
//|__________________________________________________________________________________________________________________|

void CLASSIFIER_Init(void)
{
// En esta funci�n se inicializa el m�dulo y se vac�a la cola FIFO interna.

	queue_head  = 0;		// Reinicio el �ndice de lectura.
	queue_tail  = 0;		// Reinicio el �ndice de escritura.
	queue_count = 0;		// La cola comienza vac�a.
}

_eBoxType CLASSIFIER_Classify(uint8_t d_cm)
{
// En esta funci�n se clasifica una caja seg�n la distancia medida por el HC-SR04.
// Se calcula la altura real de la caja y se compara con una ventana de 1cm por categor�a.
// Solo se acepta la caja si su altura cae exactamente en el rango configurado (+/-0,5cm).

	uint8_t h;

	if(d_cm >= reference_dist_cm)		// Si la distancia medida es mayor o igual a la referencia...
		return BOX_NONE;				// ...no hay caja (el sensor ve el piso directamente).

	h = reference_dist_cm - d_cm;		// Calculo la altura real de la caja en cm.

	// Clasificaci�n de ventana cerrada: cada categor�a acepta exactamente 1cm de ancho.
	// Cualquier altura fuera de las tres ventanas configuradas es descartada (BOX_NONE).
	if(h >= config.h_small  && h < config.h_small  + CLASSIFIER_TOL_CM) return BOX_SMALL;
	if(h >= config.h_medium && h < config.h_medium + CLASSIFIER_TOL_CM) return BOX_MEDIUM;
	if(h >= config.h_big    && h < config.h_big    + CLASSIFIER_TOL_CM) return BOX_BIG;
	return BOX_NONE;					// Altura fuera de todas las ventanas v�lidas.
}

void CLASSIFIER_SetThresholds(uint8_t h_small, uint8_t h_medium, uint8_t h_big)
{
// En esta funci�n se actualizan los umbrales de altura m�nima para cada categor�a.
// Se usa desde decodeCMD() cuando llega el comando CMD_SET_THRESH de la GUI.

	config.h_small  = h_small;		// Altura m�nima para caja peque�a.
	config.h_medium = h_medium;		// Altura m�nima para caja mediana.
	config.h_big    = h_big;		// Altura m�nima para caja grande.
}

void CLASSIFIER_SetRefDist(uint8_t ref_cm)
{
// En esta funci�n se actualiza la distancia de referencia sensor->cinta sin caja.
// Se usa desde decodeCMD() cuando llega el comando CMD_SET_CALIB de la GUI.

	reference_dist_cm = ref_cm;		// Guardo la nueva distancia de referencia en cm.
}

uint8_t CLASSIFIER_GetRefDist(void)
{
// En esta funci�n se devuelve la distancia de referencia actual.
// Se usa en main.c para convertir la distancia bruta del sensor en altura de caja.

	return reference_dist_cm;		// Devuelvo la distancia de referencia en cm.
}

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|									C�DIGO DE FUNCIONES — COLA FIFO		                                             |
//|__________________________________________________________________________________________________________________|

void CLASSIFIER_QueuePush(_eBoxType t, uint16_t est_delay_ms)
{
// En esta funci�n se encola un nuevo tipo de caja al final de la cola FIFO.
// est_delay_ms es el tiempo calculado por main.c seg�n velocidad de cinta y distancia al pateador.
// Si la cola est� llena, la caja se descarta para evitar desbordamiento.

	if(queue_count < BOX_QUEUE_SIZE)					// Solo encolo si hay espacio disponible.
	{
		box_queue[queue_tail].type      = t;				// Guardo el tipo de caja detectada.
		box_queue[queue_tail].est_timer = est_delay_ms;		// Cargo el timer de espera calculado para Modo Estimado.
		queue_tail = (queue_tail + 1) % BOX_QUEUE_SIZE;		// Avanzo el �ndice de escritura (circular).
		queue_count++;								// Incremento el contador de entradas activas.
	}
}

_eBoxType CLASSIFIER_QueueFrontType(void)
{
// En esta funci�n se devuelve el tipo de la caja al frente de la cola.
// Si la cola est� vac�a, devuelve BOX_NONE para que ning�n servo dispare.

	return (queue_count > 0) ? box_queue[queue_head].type : BOX_NONE;
}

uint16_t CLASSIFIER_QueueFrontTimer(void)
{
// En esta funci�n se devuelve el timer de espera de la entrada al frente de la cola.
// Devuelve 1 si la cola est� vac�a: evita que los servos en Modo Estimado disparen con la cola vac�a.

	return (queue_count > 0) ? box_queue[queue_head].est_timer : 1u;
}

void CLASSIFIER_QueuePop(void)
{
// En esta funci�n se elimina la entrada al frente de la cola (la que ya fue procesada por un servo).

	if(queue_count > 0)								// Solo opero si la cola no est� vac�a.
	{
		queue_head = (queue_head + 1) % BOX_QUEUE_SIZE;	// Avanzo el �ndice de lectura (circular).
		queue_count--;									// Decremento el contador de entradas activas.
	}
}

void CLASSIFIER_QueueTick(void)
{
// En esta funci�n se decrementa en 1ms el timer de todas las entradas activas de la cola.
// Debe llamarse exactamente cada 1ms desde On1ms().

	uint8_t i;
	for(i = 0; i < queue_count; i++)					// Recorro todas las entradas activas.
	{
		uint8_t idx = (queue_head + i) % BOX_QUEUE_SIZE;			// Calculo el �ndice absoluto en el buffer circular.
		if(box_queue[idx].est_timer > 0) box_queue[idx].est_timer--;	// Decremento si el timer no lleg� a cero.
	}
}

uint8_t CLASSIFIER_QueueCount(void)
{
// En esta funci�n se devuelve la cantidad de cajas actualmente en la cola.

	return queue_count;		// Devuelvo el contador de entradas activas.
}
