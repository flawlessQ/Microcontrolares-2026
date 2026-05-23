#ifndef HCSR04_H_
#define HCSR04_H_
/*
    Interfaz de hardware del driver HC-SR04.

    El driver no accede directamente a pines ni registros.
    El usuario debe pasarle funciones externas para:
    - escribir el pin TRIG
    - leer el pin ECHO
*/

#include <stdint.h>   // Se incluye para usar tipos enteros de tamaño fijo (uint8_t, uint32_t, etc.)

typedef struct
{
	// Puntero a función que permite escribir el pin TRIG del sensor.
	// level = 1 → TRIG en alto
	// level = 0 → TRIG en bajo
	void (*trig_write)(uint8_t level);

	// Puntero a función que permite leer el estado del pin ECHO.
	// Debe devolver:
	// 0 → nivel bajo
	// 1 → nivel alto
	uint8_t  (*echo_read)(void);
	
} _sHCSR04_IO;

typedef enum
{
	// En este enum se enumeran los estados del HCSR04 para la máquina de estados.

	HCSR04_IDLE = 0,		// El sensor está quieto, sin medición en curso.
	
	HCSR04_TRIG,			// Se dispara el trigger y se espera que pasen los 10 us.
	
	HCSR04_WAIT_ECHO_HIGH,	// Ya se bajó TRIG y ahora se espera que el ECHO suba.
	
	HCSR04_WAIT_ECHO_LOW,	// El ECHO ya subió y ahora se mide cuánto dura en alto.
	
	HCSR04_DONE,			// La medición terminó y hay dato listo.
	
	HCSR04_TIMEOUT			// Algo falló: el sensor no respondió dentro del tiempo esperado.
	
} _eHCSR04_STATE;

typedef struct
{
	// En esta estructura se guardan todos los datos internos
	// que necesita el driver para controlar el sensor.

	_sHCSR04_IO io;				// Guarda las funciones que le presta el hardware (GPIO y tiempo).

	_eHCSR04_STATE state;		// Indica en qué parte de la secuencia está el driver.

    uint16_t counter_us;		// Contador en microsegundos.
								// Se usa para TRIG, timeout y medición de ECHO.

	uint8_t period_ms;			// Contador en milisegundos.
								// Se usa para respetar los 60 ms entre mediciones.

	uint16_t echo_time_us;		// Tiempo que ECHO estuvo en alto.

	uint16_t distance_cm;		// Distancia calculada en centímetros a partir del tiempo medido.			
	
	uint16_t max_distance_cm;	// Distancia máxima para considerar que hay una caja.
	
	uint8_t GRHCSR04; 			/* 
								   General Register HCSR04:
								   _____________________________________
								  |  0  |  1  |  2  | 3 | 4 | 5 | 6 | 8 | 
								  |RDY0 |ERR0 |ODS0 | -	| - | - | - | - |	
								  |_____|_____|_____|___|___|___|___|___|
								  
								  RDY0: Bandera que indica que hay una medición lista.
								  ERR0: Bandera que indica que hubo timeout o error.
								  ODS0: Bandera que indica si la medición corresponde a una caja.
								  
								*/ 
} _sHCSR04;

#define	RDY0 0	// Bandera "READY 0" que indica que hay una medición lista del registro "GRHCSR04".
#define	ERR0 1	// Bandera "ERROR 0" que indica que hubo timeout o error del registro "GRHCSR04".
#define	ODS0 2	// Bandera "OBJECT DETECTED SENSOR 0" que indica si la medición corresponde a una caja del registro "GRHCSR04".

/*	
    Inicializa el driver HC-SR04.
    Recibe:
    - sensor: estructura principal del driver.
    - io: funciones externas para manejar TRIG y ECHO.
*/
void HCSR04_Init(_sHCSR04 *sensor, _sHCSR04_IO *io);

/*
    Inicia una nueva medición.
    Si el sensor está ocupado o todavía no pasaron los 60 ms
    mínimos entre mediciones, la función no hace nada.
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
void HCSR04_Start(_sHCSR04 *sensor);

/*
    Actualiza la máquina de estados del driver.
    Esta función debe llamarse constantemente desde el bucle while(1)
    del programa principal.
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
void HCSR04_Update(_sHCSR04 *sensor);

/*
    Función de tiempo en microsegundos.
    Debe llamarse cada vez que pasan 1us del sistema.
    Se usa para:
    - contar los 10 us del TRIG
    - medir el tiempo en alto de ECHO
    - controlar timeout
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
void HCSR04_On1us(_sHCSR04 *sensor);

/*
    Función de tiempo en milisegundos.
    Debe llamarse cada vez que pasan 1ms del sistema.
    Se usa para respetar el período mínimo de 60 ms
    entre mediciones.
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
void HCSR04_On1ms(_sHCSR04 *sensor);

/*
    Devuelve 1 si hay una medición lista.
    Devuelve 0 si todavía no hay dato nuevo.
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
uint8_t HCSR04_IsReady(_sHCSR04 *sensor);

/*
    Devuelve 1 si ocurrió un error o timeout.
    Devuelve 0 si no hay error.
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
uint8_t HCSR04_HasError(_sHCSR04 *sensor);

/*
    Devuelve la última distancia calculada en centímetros.
	 Recibe:
	 - sensor: puntero a la estructura del driver HC-SR04.
*/
uint16_t HCSR04_GetDistanceCm(_sHCSR04 *sensor);

/*
    Configura la distancia máxima válida para considerar
    que hay un objeto detectado.
    Recibe:
    - sensor: puntero a la estructura del driver HC-SR04.
    - distance_cm: distancia máxima válida expresada en centímetros.
*/
void HCSR04_SetMaxDistanceCm(_sHCSR04 *sensor, uint16_t distance_cm);

/*
    Indica si se detectó un objeto dentro de la distancia válida.
    Devuelve:
    - 1: se detectó un objeto, por ejemplo una caja.
    - 0: no se detectó objeto válido, por ejemplo se midió la cinta
         o hubo una medición fuera del rango configurado.
	Recibe:
	- sensor: puntero a la estructura del driver HC-SR04.
*/
uint8_t HCSR04_IsObjectDetected(_sHCSR04 *sensor);

#endif