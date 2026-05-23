/*
 * Ejercicio.c
 *
 * Created: 1/5/2026 17:30:45
* Author : Alexander Medrano
 */ 

/*
 Este ejercicio consiste en el diseño, desarrollo e implementación del firmware para un sistema de clasificación de objetos
  en una cinta transportadora, utilizando el microcontrolador de 8 bits ATMega328P. 
 El sistema es capaz de identificar dimensiones, gestionar el flujo de piezas
  y permitir la supervisión remota mediante una interfaz gráfica de usuario (GUI).
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include <avr/io.h>			// Esta librería incluye acceso a los registros del microcontrolador.

#include <avr/interrupt.h>	/*	
								Esta librería incluye:
								- Macros para habilitar y deshabilitar interrupciones.
								- Definiciones para crear rutinas de interrupción.
								- Acceso al registro global de interrupciones.
							*/

#include <avr/pgmspace.h>	/* 
							  Esta librería incluye el manejo de datos almacenados 
							  en la memoria FLASH (memoria de programa) en lugar de la RAM.
							*/   

#include <stdbool.h>		// Esta librería incluye utilizar operaciones booleanas.

#include "TIMERS.h"			/* Esta librería incluye la inicialización de los timers:
								-TIMER0: configurado a 1ms.
								-TIMER1: configurado a 1ms.
								-TIMER2: configurado a 1us.
							*/

#include "COMUNICATION.h"	// Esta librería incluye el control de la comunicación USART, tanto para la recepción como para la transmisión.

#include "HCSR04.h"			// Esta librería incluye una interfaz de hardware del driver HC-SR04.

#include "SERVO.h"			// Esta libreria incluye el driver de software PWM para los tres servomotores SG90.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													TYPDEFS 			                                             |
//|__________________________________________________________________________________________________________________|

typedef enum{
	CS_MEASURE,				// Estado que indica que se está midiendo una caja. 
	CS_PUSHER1,				// Estado que indica que hay una caja que debe ser empujado por el pateador 1.  
	CS_PUSHER2,				// Estado que indica que hay una caja que debe ser empujado por el pateador 2.  
	CS_PUSHER3, 			// Estado que indica que hay una caja que debe ser empujado por el pateador 3.  
}_eConveyorState;

typedef struct{
	uint8_t h_small;	// Altura de la caja pequeña.
	uint8_t h_medium;	// Altura de la caja mediana.
	uint8_t h_big;		// Altura de la caja grande.
	uint8_t w;			// Ancho de todas las cajas.
	uint8_t t;			// Espesor de todas las cajas.
}_sBoxs;

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

#define F_CPU	16000000UL	  // Frecuencia de la CPU (16MHz).

#define	LEDBUILTIN	PINB5	  // Led builtin en el PINB5.

#define TRIGGER PINB1		  // Trigger del sensor HC-SR04 en el PINB1.
#define ECHO PINB2			  // Echo del sensor HC-SR04 en el PINB2.

#define SERVO3 PINB3		  // Servomotor 3 en el PINB3.
#define SERVO2 PINB4		  // Servomotor 2 en el PINB4.
#define	SERVO1	PIND7	      // Servomotor 1 en el PIND7.

#define	IR0	PIND2	          // IR 0 en el PIND2.
#define	IR1	PIND3	          // IR 1 en el PIND3.
#define	IR2	PIND4	          // IR 2 en el PIND4.
#define	IR3	PIND5	          // IR 3 en el PIND5.

#define TRUE 1				  // Defino el estado "TRUE" como 1.
#define FALSE 0				  // Defino el estado "FALSE" como 0.

#define REFERENCE_DIST_CM	20		  // Distancia de referencia (sensor al transportador sin caja). Ajustar segun la instalacion real.
#define SERVO_HOLD_MS		500		  // Tiempo en ms que el servo permanece en posicion PUSH.

#define IR_DETECTED(pin)	(!((PIND) & (1 << (pin))))	// TCRT5000: LOW = objeto detectado.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|                                   DECLARACIÓN DE FUNCIONES PROTOTÍPO                                             |
//|__________________________________________________________________________________________________________________|

void On1ms();							// Función que ejecuta eventos cada 1ms.
void On1us();							// Función que ejecuta eventos cada 10us.

void ini_GPIOs ();						// Función de inicialización de GPIOs.	
void Heartbeat();						// Función de control del heartbeat.

void decodeCMD();						// Función que decodifica el comando recibido.

uint8_t classify_box(uint8_t d_cm);			// Funcion que clasifica la caja segun la distancia medida.

void HCSR04_TrigWrite(uint8_t level);	// Función que activa el sensor HCSR04 para las lecturas.
uint8_t HCSR04_EchoRead();				// Función que lee los pulsos recibidos por el sensor HCSR04. 

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											  VARIABLES GLOBALES	                                                 |
//|__________________________________________________________________________________________________________________|

uint8_t time100ms = 100;	// Contador auxiliar que se utiliza para indicar cuando pasaron 10ms.

_sRX srx;					// Variable que contendrá los datos para manejar la recepción.
_sTX stx;					// Variable que contendrá los datos para manejar la transmisión.
		
_eConveyorState state;		// Variable que contendrá los estados de la cinta transportadora.

_sBoxs boxs = {
	.h_small = 6,
	.h_medium = 8,
	.h_big = 10,
	.w = 10,
	.t = 3
};

_sHCSR04 sensor;

_sHCSR04_IO sensor_io =
{
	.trig_write = HCSR04_TrigWrite,
	.echo_read = HCSR04_EchoRead
};

uint8_t d_cm;				// Distancia medida por el sensor.

uint16_t servo_hold_timer = 0;	// Contador de tiempo para mantener el servo en posicion PUSH.
uint8_t  servo_active     = FALSE;	// Bandera que indica si el servo esta activo.
uint8_t  detected_type    = 0;	// Tipo de caja detectada: 1=small, 2=medium, 3=big.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|												FUNCIONES ISR			                                             |
//|__________________________________________________________________________________________________________________|

ISR(TIMER1_COMPA_vect)
{
// Vector interrupcion Timer/Counter1 Compare Match A.
	SERVO_On1ms();		// Genera la senal PWM de los tres servos cada 1ms.
}

ISR(TIMER0_COMPA_vect)
{
// Vector interrupción Timer/Counter0 Compare Match A.

	OCR0A += 249;				// Programo la próxima interrupción 249 ticks después de la anterior.
								// En vez de esperar otra vuelta del timer, se programa el próximo evento relativo al actual.
	GPIOR0 |= (1 << GPIOR00);	// Activo la bandera "GPIOR00", que me indica cuando sucedió una interrupción.

}

ISR(USART_RX_vect)
{
// Vector interrupción USART Rx Complete.	

	srx.rBuf.buf[srx.rBuf.iw++] = UDR0;	// Lee el byte recibido de "UDR0". Lo guarda en el buffer RX en la posición "iw". Luego incrementa "iw".
	srx.rBuf.iw &= (srx.rBuf.size-1);	// Si buf size = 2^n. size - 1 = 127. Aplicar & 127 hace que el índice quede entre 0 y 127.
	
}

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|                                   CÓDIGO DE FUNCIONES PROTOTÍPO		                                             |
//|__________________________________________________________________________________________________________________|

void ini_GPIOs ()
{
// En esta función se inicializan los puertos de entrada y salida.

	DDRB |= (1 << LEDBUILTIN);										// Defino el pin correspondiente al led builtin (PINB5) como salida. 
	DDRB |= (1 << TRIGGER) | (1 << SERVO3) | (1 << SERVO2);			// Defino los pines correspondientes DE PINB como salidas.
	DDRB &= ~(1 << ECHO);											// Establezco los pines correspondientes de PINB como entradas. 
	DDRD |= (1 << SERVO1);											// Establezco los pines correspondientes de PIND como salidas.			
	DDRD &= ~((1 << IR0) | (1 << IR1) | (1 << IR2) | (1 << IR3) );	// Establezco los pines correspondientes de PIND como entradas.
}

void On1ms ()
{
// En esta función se realizan los eventos correspondientes cada 1ms.
	
	if (!time100ms)				// Compruebo si ya pasó 100ms.
	time100ms = 100;			// Reinicio el contador de 100ms.
	
	time100ms--;				// Descuento 1 al contador de time10ms.
	
	if (servo_hold_timer > 0)
		servo_hold_timer--;	// Decrementa el temporizador del servo.

	HCSR04_On1ms(&sensor);		// Inicio el contador interno de 1ms del sensor HCSR04.
	
	GPIOR0 &= ~(1 << GPIOR00);	// Limpio la bandera "GPIOR00" del registro GPIOR0.
}

void On1us()
{
// En esta función se realizan los eventos correspondientes cada 1us.

	//OCR2A += 2;				// Programo la próxima interrupción 20 ticks después de la anterior.
							// En vez de esperar otra vuelta del timer, se programa el próximo evento relativo al actual.
	
	HCSR04_On1us(&sensor);  // Inicio el contador interno de 1us del sensor HCSR04.
	
	TIFR2 |= (1 << OCF2A);  // Limpio la bandera "OCF2A" correspondiente al registro "TIFR2".
}

void Heartbeat ()
{
/* En esta función se realiza la secuencia del led builtin que indica el contínuo funcionamiento del sistema.	

	El led builtin se enciende 100ms y se apaga 100ms.
		 ____	    ____	   ____
		|    |     |    |     |    |
   _____|    |_____|    |_____|    |_____
	off - on - off - on - off - on - off
  100 - 200 - 300 - 400 - 500 - 600 - 700 (ms)	
*/	
	if	(!time100ms)					// Compruebo si pasaron 100ms. 
	{
		time100ms = 100;
		PORTB ^= (1 << LEDBUILTIN);		// Toggleo el pin de salida del led LEDBUILTIN.
	}

}

void HCSR04_TrigWrite(uint8_t level)
{
	if(level)
	PORTB |= (1 << TRIGGER);
	else
	PORTB &= ~(1 << TRIGGER);
}

uint8_t HCSR04_EchoRead()
{
	return (PINB & (1 << ECHO)) ? 1 : 0;	
}

void decodeCMD()
{
	
}

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|                                              CÓDIGO PRINCIPAL		                                             |
//|__________________________________________________________________________________________________________________|



uint8_t classify_box(uint8_t d_cm)
{
// Calcula la altura de la caja y la clasifica. Retorna: 1=small, 2=medium, 3=big.
// Si d_cm >= REFERENCE_DIST_CM no hay caja detectable (retorna 0).

	if (d_cm >= REFERENCE_DIST_CM)
		return 0;

	uint8_t h = REFERENCE_DIST_CM - d_cm;	// Altura de la caja en cm.

	if (h >= boxs.h_big)    return 3;		// Caja grande.
	if (h >= boxs.h_medium) return 2;		// Caja mediana.
	return 1;							// Caja pequena.
}

int main(void)
{
	cli ();										// Deshabilito interrupciones globales.
	
	ini_TIMER0 ();								// Inicio el Timer 0.
	ini_TIMER2();								// Inicio el Timer 2.
	ini_TIMER1 ();							// Inicio el Timer 1 (necesario para el PWM de los servos).
	
	ini_USART0 ();								// Inicio la comunicación USART.
	ini_COM(&srx, &stx);						// Inicio las variables de comunicación.
	
	ini_GPIOs ();								// Inicio puertos.

	SERVO_Init ();							// Inicio los servomotores (todos en posicion HOME).
	
	HCSR04_Init(&sensor, &sensor_io);			// Inicio el sensor HCSR04.
	HCSR04_SetMaxDistanceCm(&sensor, 20);		// Establezco la distancia máxima a medir.
	
	state = CS_MEASURE;
	
	sei ();										// Habilito interrupciones globales.
	
	while (1)
	{
		
		USART_SendByte(&stx);					// Envío un byte por USART.
		
		if(decodeHeader(&srx))					// Verifico si el mensaje cumple con el protocolo.
		decodeCMD();							// Si el mensaje cumple, decodifico el comando.
		
		Heartbeat();							// Secuencia del heartbeat.
		
		switch (state)							// Maquina de estado de la cinta transportadora.
		{
			case CS_MEASURE:					// CS_MEASURE: Estado donde se realiza la medicion.
				
				HCSR04_Start(&sensor);			// Inicio el sensor HCSR04.
				
				HCSR04_Update(&sensor);			// Actualizo la maquina de estados del sensor.
				
				if(HCSR04_IsReady(&sensor))		// El sensor esta listo con una medicion nueva.
				{
				   d_cm = HCSR04_GetDistanceCm(&sensor);	// Medicion obtenida por el HC-SR04.
				   
				   stx.cmd = 0xA1;				// Le asigno el comando al mensaje.
				   stx.payload[0] = d_cm;		// Agrego la distancia medida al payload.
				   stx.payloadLen = 1;			// Establezco el tamano del payload del mensaje.
				   
				   buildCMD(&stx);				// Armo el mensaje para la transmision.
				   
				   if(IR_DETECTED(IR0))			// Hay una caja en la zona de medicion.
				   {
				   		detected_type = classify_box(d_cm);	// Clasifico la caja segun su altura.
				   		
				   		if      (detected_type == 3) state = CS_PUSHER3;
				   		else if (detected_type == 2) state = CS_PUSHER2;
				   		else if (detected_type == 1) state = CS_PUSHER1;
				   }
				}
				
			   if(HCSR04_HasError(&sensor))		// Ocurrio un error con el sensor.
			    {
					stx.cmd = 0xA0;				// Le asigno el comando al mensaje.
					stx.payloadLen = 0;			// Establezco el tamano del payload.
					   				   
					buildCMD(&stx);				// Armo el mensaje para la transmision.
				}
				
			break;
			
			case CS_PUSHER1:					// CS_PUSHER1: Una caja pequena debe ser eyectada por SERVO1.
			
				if(!servo_active && IR_DETECTED(IR1))	// La caja llego a la posicion del pateador 1.
				{
					SERVO_Set(SERVO_ID_1, SERVO_PUSH);	// Extiendo el servo 1.
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}
			
				if(servo_active && !servo_hold_timer)	// El tiempo de empuje termino.
				{
					SERVO_Set(SERVO_ID_1, SERVO_HOME);	// Retraigo el servo 1.
					servo_active = FALSE;
					state = CS_MEASURE;
				}
			
			break;
			
			case CS_PUSHER2:					// CS_PUSHER2: Una caja mediana debe ser eyectada por SERVO2.
			
				if(!servo_active && IR_DETECTED(IR2))	// La caja llego a la posicion del pateador 2.
				{
					SERVO_Set(SERVO_ID_2, SERVO_PUSH);	// Extiendo el servo 2.
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}
			
				if(servo_active && !servo_hold_timer)	// El tiempo de empuje termino.
				{
					SERVO_Set(SERVO_ID_2, SERVO_HOME);	// Retraigo el servo 2.
					servo_active = FALSE;
					state = CS_MEASURE;
				}
			
			break;
			
			case CS_PUSHER3:					// CS_PUSHER3: Una caja grande debe ser eyectada por SERVO3.
			
				if(!servo_active && IR_DETECTED(IR3))	// La caja llego a la posicion del pateador 3.
				{
					SERVO_Set(SERVO_ID_3, SERVO_PUSH);	// Extiendo el servo 3.
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}
			
				if(servo_active && !servo_hold_timer)	// El tiempo de empuje termino.
				{
					SERVO_Set(SERVO_ID_3, SERVO_HOME);	// Retraigo el servo 3.
					servo_active = FALSE;
					state = CS_MEASURE;
				}
			
			break;
		}
		
		if(GPIOR0 & (1 << GPIOR00))				// Compruebo si pasaron 1ms.
		On1ms();								// Ejecuto eventos de 1ms.
		
		if(TIFR2 & (1 << OCF2A))				// Compruebo si pasaron 1us.
		On1us();								// Ejecuto eventos de 1us.
	
	}
}

