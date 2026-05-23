/*
  CLASSIFIER.h

  M�dulo de clasificaci�n de cajas para la cinta transportadora.

  El sensor HC-SR04 est� montado mirando hacia el piso.
  La distancia medida es la distancia desde el sensor hasta la parte
  superior de la caja. La altura real de la caja se calcula como:

      h = reference_dist_cm - d_medido

  Clasificaci�n de ventana cerrada (tolerancia +/-0,5cm = 1cm de ancho):
      h_small  <= h < h_small  + 1cm -> BOX_SMALL  (h = 6cm por defecto)
      h_medium <= h < h_medium + 1cm -> BOX_MEDIUM (h = 8cm por defecto)
      h_big    <= h < h_big    + 1cm -> BOX_BIG    (h = 10cm por defecto)
      cualquier otro valor            -> BOX_NONE   (fuera de rango, se descarta)

  La cola FIFO interna (hasta 8 entradas) registra los tipos de cajas
  detectadas en orden de llegada para que cada servo procese "su" caja
  en el orden correcto.
*/

#ifndef CLASSIFIER_H_
#define CLASSIFIER_H_

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include <avr/io.h>		// Esta librer�a incluye acceso a los registros del microcontrolador.
#include <stdint.h>		// Esta librer�a incluye los tipos de datos est�ndar de ancho fijo.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													TYPEDEFS			                                             |
//|__________________________________________________________________________________________________________________|

typedef enum {
	BOX_NONE   = 0,		// Sin caja o fuera de cualquier rango v�lido.
	BOX_SMALL  = 1,		// Caja peque�a  (h = 6cm por defecto).
	BOX_MEDIUM = 2,		// Caja mediana  (h = 8cm por defecto).
	BOX_BIG    = 3,		// Caja grande   (h = 10cm por defecto).
} _eBoxType;

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|									FUNCIONES PROTOTIPO — CLASIFICACI�N		                                         |
//|__________________________________________________________________________________________________________________|

void      CLASSIFIER_Init(void);
/*	Inicializa el clasificador y vac�a la cola FIFO interna.
	Debe llamarse antes de iniciar el bucle principal.					*/

_eBoxType CLASSIFIER_Classify(uint8_t d_cm);
/*	Clasifica una caja seg�n la distancia medida en cm.
	Devuelve BOX_SMALL, BOX_MEDIUM, BOX_BIG, o BOX_NONE si la
	altura calculada no corresponde a ning�n rango v�lido.				*/

void      CLASSIFIER_SetThresholds(uint8_t h_small, uint8_t h_medium, uint8_t h_big);
/*	Establece los umbrales de altura m�nima para cada categor�a (cm).
	Puede llamarse desde la GUI v�a CMD_SET_THRESH cuando la cinta
	est� detenida.														*/

void      CLASSIFIER_SetRefDist(uint8_t ref_cm);
/*	Establece la distancia de referencia sensor->cinta sin caja (cm).
	Debe medirse f�sicamente en el banco y configurarse antes de operar.
	Puede cambiarse desde la GUI v�a CMD_SET_CALIB.						*/

uint8_t   CLASSIFIER_GetRefDist(void);
/*	Devuelve la distancia de referencia configurada actualmente (cm).
	Se usa en main.c para convertir la distancia bruta en altura de caja
	antes de enviarla a la GUI por CMD_DIST_MEAS.						*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|									FUNCIONES PROTOTIPO — COLA FIFO		                                             |
//|__________________________________________________________________________________________________________________|

void      CLASSIFIER_QueuePush(_eBoxType type, uint16_t est_delay_ms);
/*	Encola un tipo de caja al final de la cola FIFO interna.
	est_delay_ms es el tiempo de cuenta regresiva inicial para el Modo Estimado (calculado
	en main.c seg�n la velocidad de la cinta y la distancia al pateador correspondiente).
	Si la cola est� llena (8 entradas), la caja entrante se descarta.	*/

_eBoxType CLASSIFIER_QueueFrontType(void);
/*	Devuelve el tipo de la caja al frente de la cola.
	Si la cola est� vac�a, devuelve BOX_NONE.							*/

uint16_t  CLASSIFIER_QueueFrontTimer(void);
/*	Devuelve el timer de espera de la entrada al frente de la cola.
	Un valor de 0 indica que el tiempo del Modo Estimado expir�.
	Si la cola est� vac�a, devuelve 1 (nunca 0) para evitar disparos
	falsos de los servos en Modo Estimado.								*/

void      CLASSIFIER_QueuePop(void);
/*	Elimina la entrada al frente de la cola.
	Debe llamarse cuando el servo correspondiente dispara.				*/

void      CLASSIFIER_QueueTick(void);
/*	Decrementa en 1ms el timer de espera de todas las entradas activas.
	Debe llamarse exactamente cada 1ms desde On1ms().					*/

uint8_t   CLASSIFIER_QueueCount(void);
/*	Devuelve la cantidad de cajas actualmente en la cola.				*/

#endif /* CLASSIFIER_H_ */
