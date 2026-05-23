/*
  IR.h

  Driver para los sensores infrarrojos TCRT5000.
  Implementa debounce por software de 20ms, detecci�n de flancos
  y c�lculo de velocidad de caja a partir del tiempo de bloqueo.

  Conexiones f�sicas:
      IR0 → PD5  (zona de medici�n)
      IR1 → PD2  (posici�n del pateador 1)
      IR2 → PD3  (posici�n del pateador 2)
      IR3 → PD4  (posici�n del pateador 3)
*/

#ifndef IR_H_
#define IR_H_

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
	IR_ID_0 = 0,	// PD5 (D5) — zona de medici�n.
	IR_ID_1 = 1,	// PD2 (D2) — posici�n del pateador 1.
	IR_ID_2 = 2,	// PD3 (D3) — posici�n del pateador 2.
	IR_ID_3 = 3,	// PD4 (D4) — posici�n del pateador 3.
} _eIRID;

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											FUNCIONES PROTOTIPO			                                             |
//|__________________________________________________________________________________________________________________|

void IR_Init(void);
/*	Inicializa el driver IR: configura los pines como entradas y
	limpia todos los estados internos.
	Debe llamarse antes de iniciar el bucle principal.				*/

void IR_UpdateDebounce(void);
/*	Actualiza el debounce (20ms), los flancos y el tiempo de bloqueo
	de cada sensor. Debe llamarse exactamente cada 1ms desde On1ms().	*/

uint8_t IR_IsDetected(_eIRID id);
/*	Devuelve el estado estable del sensor indicado:
	1 = caja detectada (sensor bloqueado).
	0 = sin caja.													*/

uint8_t IR_RisingEdge(_eIRID id);
/*	Devuelve 1 una �nica vez cuando se detecta un flanco ascendente
	estable (caja llega al sensor). La bandera se consume al leer.	*/

uint8_t IR_UpdateBoxSpeed(_eIRID id, uint16_t *speed_mm_s);
/*	Calcula la velocidad de la caja en mm/s a partir del tiempo que
	el sensor estuvo bloqueado. Devuelve 1 si hay un dato nuevo y
	escribe el resultado en *speed_mm_s.							*/

uint16_t IR_GetBlockTimeMs(_eIRID id);
/*	Devuelve el tiempo en ms que el sensor estuvo bloqueado por la
	�ltima caja. V�lido desde el flanco descendente hasta el pr�ximo
	flanco ascendente (no se resetea al leer).					*/

#endif /* IR_H_ */
