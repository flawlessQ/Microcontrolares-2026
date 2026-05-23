/*
  HCSR04.h

  Driver portable para el sensor HC-SR04.
  No accede directamente a los registros del microcontrolador.
  Las operaciones de hardware (TRIG, ECHO, timer) se realizan
  mediante punteros a funci�n configurados desde el main.

  Uso t�pico:
      1. Definir las funciones de hardware en main.c.
      2. Cargar los punteros en una estructura _sHCSR04_IO.
      3. Llamar a HCSR04_Init() para vincularlos al driver.
      4. Llamar a HCSR04_Measure() cuando IR0 detecte una caja.
      5. Leer el resultado con HCSR04_GetDistanceCm().
*/

#ifndef HCSR04_H_
#define HCSR04_H_

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include <stdint.h>		// Esta librer�a incluye los tipos de datos est�ndar de ancho fijo.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

/*
    Bits del registro general de banderas del driver (GRHCSR04).
*/
#define RDY0    0   // Medici�n lista: hay un dato v�lido disponible.
#define ERR0    1   // Error o timeout: la medici�n fall�.
#define ODS0    2   // Objeto detectado: la distancia est� dentro del l�mite configurado.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													TYPEDEFS			                                             |
//|__________________________________________________________________________________________________________________|

/*
    Estados internos de la m�quina de estados del driver HC-SR04.
*/
typedef enum
{
    HCSR04_IDLE = 0,          // Sensor en reposo.
    HCSR04_TRIG,              // Pulso TRIG en proceso.
    HCSR04_WAIT_ECHO_HIGH,    // Esperando que ECHO suba.
    HCSR04_WAIT_ECHO_LOW,     // Midiendo ECHO en alto.
    HCSR04_DONE,              // Medici�n finalizada correctamente.
    HCSR04_TIMEOUT            // Error por timeout.

} _eHCSR04_STATE;

/*
    Interfaz de hardware.

    Estas funciones se implementan desde el main.
    El driver no accede directamente a registros del microcontrolador.
    Esto permite reusar el driver en cualquier micro sin modificarlo.
*/
typedef struct
{
    void (*trig_write)(uint8_t level);      // Escribe el nivel del pin TRIG (1=alto, 0=bajo).
    uint8_t (*echo_read)(void);             // Lee el nivel del pin ECHO (1=alto, 0=bajo).

    void (*timer_reset)(void);              // Reinicia el timer a cero.
    void (*timer_start)(void);             // Arranca el timer y deshabilita la ISR de servo.
    void (*timer_stop)(void);              // Detiene el timer y rehabilita la ISR de servo.
    uint16_t (*timer_get_ticks)(void);      // Devuelve los ticks actuales del timer.

} _sHCSR04_IO;

/*
    Estructura principal del driver HC-SR04.
    Contiene la interfaz de hardware, el estado interno y los resultados.
*/
typedef struct
{
    _sHCSR04_IO io;              // Punteros a las funciones de hardware.

    _eHCSR04_STATE state;        // Estado actual del sensor.

    uint16_t counter_us;         // Se conserva por compatibilidad con la versi�n anterior.
    uint8_t period_ms;           // Contador para respetar el per�odo m�nimo entre mediciones.

    uint16_t echo_time_us;       // Tiempo medido del pulso ECHO en microsegundos.
    uint16_t distance_cm;        // Distancia calculada en cent�metros.

    uint16_t max_distance_cm;    // Distancia m�xima v�lida configurada desde el main.

    uint8_t GRHCSR04;            // Registro de banderas del driver (bits RDY0, ERR0, ODS0).

} _sHCSR04;

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											FUNCIONES PROTOTIPO			                                             |
//|__________________________________________________________________________________________________________________|

void HCSR04_Init(_sHCSR04 *sensor, _sHCSR04_IO *io);
/*	Inicializa el driver HC-SR04: vincula la interfaz de hardware y
	deja el sensor en reposo con todas las banderas en cero.
	Debe llamarse antes de iniciar el bucle principal.			*/

void HCSR04_Start(_sHCSR04 *sensor);
/*	Inicia una medici�n. Se conserva por compatibilidad; internamente
	llama a HCSR04_Measure().									*/

void HCSR04_Update(_sHCSR04 *sensor);
/*	Actualiza la m�quina de estados no bloqueante.
	Se conserva por compatibilidad; en esta versi�n no hace nada.	*/

void HCSR04_On1us(_sHCSR04 *sensor);
/*	Funci�n de conteo de 1us.
	En la versi�n con timer por hardware puede no usarse.		*/

void HCSR04_On1ms(_sHCSR04 *sensor);
/*	Incrementa el contador de per�odo entre mediciones.
	Debe llamarse cada 1ms desde On1ms().						*/

void HCSR04_Measure(_sHCSR04 *sensor);
/*	Realiza una medici�n bloqueante usando el timer de hardware.
	Genera el pulso TRIG, mide el ancho del pulso ECHO y calcula
	la distancia en cent�metros. Tiempo t�pico: 1-2ms.			*/

uint8_t HCSR04_IsReady(_sHCSR04 *sensor);
/*	Devuelve 1 si hay una medici�n lista para leer.				*/

uint8_t HCSR04_HasError(_sHCSR04 *sensor);
/*	Devuelve 1 si ocurri� un error o timeout en la �ltima medici�n.	*/

uint16_t HCSR04_GetDistanceCm(_sHCSR04 *sensor);
/*	Devuelve la �ltima distancia calculada en cent�metros.
	Consume la bandera RDY0 al leer.							*/

uint8_t HCSR04_IsObjectDetected(_sHCSR04 *sensor);
/*	Devuelve 1 si la �ltima medici�n corresponde a un objeto v�lido
	(distancia menor o igual a max_distance_cm).				*/

void HCSR04_SetMaxDistanceCm(_sHCSR04 *sensor, uint16_t distance_cm);
/*	Configura la distancia m�xima v�lida para considerar un objeto
	como detectado. Depende del montaje f�sico; se define en main.c.	*/

#endif /* HCSR04_H_ */
