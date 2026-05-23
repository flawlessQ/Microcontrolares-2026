/*
 * Ejercicio.c
 *
 * Created: 1/5/2026 17:30:45
* Author : Alexander Medrano
 */ 

/*
 Este ejercicio consiste en el dise�o, desarrollo e implementaci�n del firmware para un sistema de clasificaci�n de objetos
  en una cinta transportadora, utilizando el microcontrolador de 8 bits ATMega328P. 
 El sistema es capaz de identificar dimensiones, gestionar el flujo de piezas
  y permitir la supervisi�n remota mediante una interfaz gr�fica de usuario (GUI).
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include <avr/io.h>			// Esta librer�a incluye acceso a los registros del microcontrolador.

#include <avr/interrupt.h>	/*	
								Esta librer�a incluye:
								- Macros para habilitar y deshabilitar interrupciones.
								- Definiciones para crear rutinas de interrupci�n.
								- Acceso al registro global de interrupciones.
							*/

#include <avr/pgmspace.h>	/* 
							  Esta librer�a incluye el manejo de datos almacenados 
							  en la memoria FLASH (memoria de programa) en lugar de la RAM.
							*/   

#include <stdbool.h>		// Esta librer�a incluye utilizar operaciones booleanas.

#include "TIMERS.h"			/* Esta librer�a incluye la inicializaci�n de los timers:
								-TIMER0: configurado a 1ms.
								-TIMER1: configurado a 1ms.
								-TIMER2: configurado a 1us.
							*/

#include "COMUNICATION.h"	// Esta librer�a incluye el control de la comunicaci�n USART, tanto para la recepci�n como para la transmisi�n.

#include "HCSR04.h"			// Esta librer�a incluye una interfaz de hardware del driver HC-SR04.
#include "IR.h"			// Esta libreria incluye el driver de los sensores infrarrojos TCRT5000.
#include "CLASSIFIER.h"		// Esta libreria incluye la logica de clasificacion de cajas.

#include "SERVO.h"			// Esta libreria incluye el driver de software PWM para los tres servomotores SG90.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													TYPDEFS 			                                             |
//|__________________________________________________________________________________________________________________|

typedef enum{
	CS_MEASURE,				// Estado que indica que se est� midiendo una caja. 
	CS_PUSHER1,				// Estado que indica que hay una caja que debe ser empujado por el pateador 1.  
	CS_PUSHER2,				// Estado que indica que hay una caja que debe ser empujado por el pateador 2.  
	CS_PUSHER3, 			// Estado que indica que hay una caja que debe ser empujado por el pateador 3.  
}_eConveyorState;


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


#define BTN_SMALL   PIND6	  // Boton simulacion caja pequena  (Arduino D6).
#define BTN_MEDIUM  PINB0	  // Boton simulacion caja mediana  (Arduino D8).

#define BTN_SMALL_PRESSED   ((PIND) & (1 << BTN_SMALL))	// Activo alto (pull-down externo, boton conecta pin a 5V).
#define BTN_MEDIUM_PRESSED  ((PINB) & (1 << BTN_MEDIUM))	// Activo alto (pull-down externo, boton conecta pin a 5V).

#define TRUE 1				  // Defino el estado "TRUE" como 1.
#define FALSE 0				  // Defino el estado "FALSE" como 0.

#define SERVO_HOLD_MS		500		  // Tiempo en ms que el servo permanece en posicion PUSH.

#define MODE_NORMAL		0		  // Modo normal: servo espera deteccion IR del pateador.
#define MODE_ESTIMATED	1		  // Modo estimado: servo dispara por timer fijo.
#define EST_DELAY_MS	2000	  // Retardo estimado de transporte cinta (ms).


// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|                                   DECLARACI�N DE FUNCIONES PROTOT�PO                                             |
//|__________________________________________________________________________________________________________________|

void On1ms();							// Funci�n que ejecuta eventos cada 1ms.
void On1us();							// Funci�n que ejecuta eventos cada 10us.

void ini_GPIOs ();						// Funci�n de inicializaci�n de GPIOs.	
void Heartbeat();						// Funci�n de control del heartbeat.

void decodeCMD();						// Funci�n que decodifica el comando recibido.


void HCSR04_TrigWrite(uint8_t level);
uint8_t HCSR04_EchoRead(void);
void    HCSR04_TimerReset(void);
void    HCSR04_TimerStart(void);
void    HCSR04_TimerStop(void);
uint16_t HCSR04_TimerGetTicks(void);

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											  VARIABLES GLOBALES	                                                 |
//|__________________________________________________________________________________________________________________|

uint8_t time100ms = 100;	// Contador auxiliar que se utiliza para indicar cuando pasaron 10ms.

_sRX srx;					// Variable que contendr� los datos para manejar la recepci�n.
_sTX stx;					// Variable que contendr� los datos para manejar la transmisi�n.
		
_eConveyorState state;		// Variable que contendr� los estados de la cinta transportadora.


_sHCSR04 sensor;

_sHCSR04_IO sensor_io =
{
	.trig_write      = HCSR04_TrigWrite,
	.echo_read       = HCSR04_EchoRead,
	.timer_reset     = HCSR04_TimerReset,
	.timer_start     = HCSR04_TimerStart,
	.timer_stop      = HCSR04_TimerStop,
	.timer_get_ticks = HCSR04_TimerGetTicks
};

uint8_t d_cm;				// Distancia medida por el sensor.

uint16_t servo_hold_timer = 0;	// Contador de tiempo para mantener el servo en posicion PUSH.
uint8_t  servo_active     = FALSE;	// Bandera que indica si el servo esta activo.
_eBoxType  detected_type  = BOX_NONE;	// Tipo de caja detectada por el clasificador.
uint16_t box_count[4]    = {0, 0, 0, 0};	// Contadores de cajas eyectadas por tipo (_eBoxType).
uint8_t  conv_mode       = MODE_NORMAL;		// Modo de operacion (0=Normal, 1=Estimado).
uint16_t est_delay_timer = 0;				// Timer de espera en modo estimado (ms).

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
// Vector interrupci�n Timer/Counter0 Compare Match A.

	OCR0A += 249;				// Programo la pr�xima interrupci�n 249 ticks despu�s de la anterior.
								// En vez de esperar otra vuelta del timer, se programa el pr�ximo evento relativo al actual.
	GPIOR0 |= (1 << GPIOR00);	// Activo la bandera "GPIOR00", que me indica cuando sucedi� una interrupci�n.

}

ISR(USART_RX_vect)
{
// Vector interrupci�n USART Rx Complete.	

	srx.rBuf.buf[srx.rBuf.iw++] = UDR0;	// Lee el byte recibido de "UDR0". Lo guarda en el buffer RX en la posici�n "iw". Luego incrementa "iw".
	srx.rBuf.iw &= (srx.rBuf.size-1);	// Si buf size = 2^n. size - 1 = 127. Aplicar & 127 hace que el �ndice quede entre 0 y 127.
	
}

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|                                   C�DIGO DE FUNCIONES PROTOT�PO		                                             |
//|__________________________________________________________________________________________________________________|

void ini_GPIOs ()
{
// En esta funci�n se inicializan los puertos de entrada y salida.

	DDRB |= (1 << LEDBUILTIN);										// Defino el pin correspondiente al led builtin (PINB5) como salida. 
	DDRB |= (1 << TRIGGER) | (1 << SERVO3) | (1 << SERVO2);			// Defino los pines correspondientes DE PINB como salidas.
	DDRB &= ~(1 << ECHO);											// Establezco los pines correspondientes de PINB como entradas.
	PORTB &= ~(1 << ECHO);											// Sin pull-up: el HC-SR04 maneja ECHO activamente.
	DDRD |= (1 << SERVO1);											// Establezco los pines correspondientes de PIND como salidas.
}

void On1ms ()
{
// En esta funci�n se realizan los eventos correspondientes cada 1ms.
	
	if (!time100ms)				// Compruebo si ya pas� 100ms.
	time100ms = 100;			// Reinicio el contador de 100ms.
	
	time100ms--;				// Descuento 1 al contador de time10ms.
	
	if (servo_hold_timer > 0)
		servo_hold_timer--;	// Decrementa el temporizador del servo.

	if (est_delay_timer > 0)
		est_delay_timer--;

	HCSR04_On1ms(&sensor);
	IR_UpdateDebounce();

	GPIOR0 &= ~(1 << GPIOR00);	// Limpio la bandera "GPIOR00" del registro GPIOR0.
}

void On1us()
{
	TIFR2 |= (1 << OCF2A);
}

void Heartbeat ()
{
/* En esta funci�n se realiza la secuencia del led builtin que indica el cont�nuo funcionamiento del sistema.	

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

uint8_t HCSR04_EchoRead(void)
{
	return (PINB & (1 << ECHO)) ? 1 : 0;
}

void HCSR04_TimerReset(void)
{
	TCNT1 = 0;
}

void HCSR04_TimerStart(void)
{
	TIMSK1 &= ~(1 << OCIE1A);		// Deshabilita ISR de servo durante la medicion.
	TCCR1A = 0x00;
	TCCR1B = (1 << CS11);			// Modo normal, prescaler 8: 1 tick = 0.5us.
	TCNT1  = 0;
}

void HCSR04_TimerStop(void)
{
	TCCR1B = 0x00;
	// Restaura Timer1 para el servo: CTC, prescaler 64, OCR1A=249 (1ms).
	TCCR1A = 0x00;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1AH = 0;
	OCR1AL = 249;
	TCNT1H = 0;
	TCNT1L = 0;
	TIMSK1 |= (1 << OCIE1A);		// Rehabilita ISR de servo.
}

uint16_t HCSR04_TimerGetTicks(void)
{
	return TCNT1;
}

void decodeCMD()
{
	uint8_t busy = (state != CS_MEASURE || servo_active);

	switch (srx.cmd)
	{
		case CMD_GET_STATE:
			stx.cmd        = CMD_STATE;
			stx.payload[0] = (uint8_t)state;
			stx.payloadLen = 1;
			buildCMD(&stx);
		break;

		case CMD_GET_COUNTS:
			stx.cmd        = CMD_COUNTS;
			stx.payload[0] = (box_count[BOX_SMALL]  >> 8) & 0xFF;
			stx.payload[1] =  box_count[BOX_SMALL]        & 0xFF;
			stx.payload[2] = (box_count[BOX_MEDIUM] >> 8) & 0xFF;
			stx.payload[3] =  box_count[BOX_MEDIUM]       & 0xFF;
			stx.payload[4] = (box_count[BOX_BIG]    >> 8) & 0xFF;
			stx.payload[5] =  box_count[BOX_BIG]          & 0xFF;
			stx.payloadLen = 6;
			buildCMD(&stx);
		break;

		case CMD_SET_MODE:
			if (!busy) conv_mode = srx.payload[0];
			stx.cmd        = CMD_ACK;
			stx.payload[0] = CMD_SET_MODE;
			stx.payload[1] = busy ? ACK_BUSY : ACK_OK;
			stx.payloadLen = 2;
			buildCMD(&stx);
		break;

		case CMD_SET_THRESH:
			if (!busy) CLASSIFIER_SetThresholds(srx.payload[0], srx.payload[1], srx.payload[2]);
			stx.cmd        = CMD_ACK;
			stx.payload[0] = CMD_SET_THRESH;
			stx.payload[1] = busy ? ACK_BUSY : ACK_OK;
			stx.payloadLen = 2;
			buildCMD(&stx);
		break;

		case CMD_SET_CALIB:
			if (!busy) CLASSIFIER_SetRefDist(srx.payload[0]);
			stx.cmd        = CMD_ACK;
			stx.payload[0] = CMD_SET_CALIB;
			stx.payload[1] = busy ? ACK_BUSY : ACK_OK;
			stx.payloadLen = 2;
			buildCMD(&stx);
		break;

		case CMD_RESET_COUNTS:
			box_count[BOX_SMALL]  = 0;
			box_count[BOX_MEDIUM] = 0;
			box_count[BOX_BIG]    = 0;
			stx.cmd        = CMD_ACK;
			stx.payload[0] = CMD_RESET_COUNTS;
			stx.payload[1] = ACK_OK;
			stx.payloadLen = 2;
			buildCMD(&stx);
		break;
	}
}

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|                                              C�DIGO PRINCIPAL		                                             |
//|__________________________________________________________________________________________________________________|




int main(void)
{
	cli ();										// Deshabilito interrupciones globales.
	
	ini_TIMER0 ();								// Inicio el Timer 0.
	ini_TIMER2();								// Inicio el Timer 2.
	ini_TIMER1 ();							// Inicio el Timer 1 (necesario para el PWM de los servos).
	
	ini_USART0 ();								// Inicio la comunicaci�n USART.
	ini_COM(&srx, &stx);						// Inicio las variables de comunicaci�n.
	
	ini_GPIOs ();								// Inicio puertos.

	SERVO_Init ();							// Inicio los servomotores (todos en posicion HOME).
	IR_Init();								// Inicio los sensores infrarrojos.
	CLASSIFIER_Init();						// Inicio el clasificador de cajas.
	
	HCSR04_Init(&sensor, &sensor_io);			// Inicio el sensor HCSR04.
	HCSR04_SetMaxDistanceCm(&sensor, 20);		// Establezco la distancia m�xima a medir.
	
	state = CS_MEASURE;
	
	sei ();										// Habilito interrupciones globales.
	
	while (1)
	{
		
		USART_SendByte(&stx);					// Env�o un byte por USART.
		if(decodeHeader(&srx))					// Verifico si el mensaje cumple con el protocolo.
		decodeCMD();							// Si el mensaje cumple, decodifico el comando.
		
		Heartbeat();							// Secuencia del heartbeat.
		
		switch (state)							// Maquina de estado de la cinta transportadora.
		{
			case CS_MEASURE:					// CS_MEASURE: Estado donde se realiza la medicion.
				
				HCSR04_Start(&sensor);			// Dispara medicion bloqueante (retorna inmediato si no paso el periodo).
				
				if(HCSR04_IsReady(&sensor))		// El sensor esta listo con una medicion nueva.
				{
				   d_cm = HCSR04_GetDistanceCm(&sensor);	// Medicion obtenida por el HC-SR04.
				   
				   stx.cmd = 0xA1;				// Le asigno el comando al mensaje.
				   stx.payload[0] = d_cm;		// Agrego la distancia medida al payload.
				   stx.payloadLen = 1;			// Establezco el tamano del payload del mensaje.
				   
				   buildCMD(&stx);				// Armo el mensaje para la transmision.
				   
				   if(HCSR04_IsObjectDetected(&sensor))	// Hay una caja en la zona de medicion.
				   {
				   		detected_type = CLASSIFIER_Classify(d_cm);	// Clasifico la caja segun su altura.
				   		
				   		// BOX_BIG: deshabilitado hasta tener IR3 montado.
				   		if      (detected_type == BOX_MEDIUM) { state = CS_PUSHER2; est_delay_timer = EST_DELAY_MS; }
				   		else if (detected_type == BOX_SMALL)  { state = CS_PUSHER1; est_delay_timer = EST_DELAY_MS; }
				   }
				}
				
			   if(HCSR04_HasError(&sensor))		// Ocurrio un error con el sensor.
			    {
					sensor.GRHCSR04 &= ~(1 << ERR0);  // Consume la bandera: envia el error solo una vez por ciclo.
					stx.cmd = 0xA0;
					stx.payloadLen = 0;
					buildCMD(&stx);
				}
				
				
			break;
			
			case CS_PUSHER1:					// CS_PUSHER1: Una caja pequena debe ser eyectada por SERVO1.
			
				if(!servo_active && conv_mode == MODE_NORMAL && IR_IsDetected(IR_ID_1))
				{
					SERVO_Set(SERVO_ID_1, SERVO_PUSH);
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}

				if(!servo_active && conv_mode == MODE_ESTIMATED && !est_delay_timer)
				{
					SERVO_Set(SERVO_ID_1, SERVO_PUSH);
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}
			
				if(servo_active && !servo_hold_timer)	// El tiempo de empuje termino.
				{
					SERVO_Set(SERVO_ID_1, SERVO_HOME);
					servo_active = FALSE;
					box_count[BOX_SMALL]++;
					stx.cmd        = CMD_BOX_EJECTED;
					stx.payload[0] = BOX_SMALL;
					stx.payloadLen = 1;
					buildCMD(&stx);
					state = CS_MEASURE;
				}
			
			break;
			
			case CS_PUSHER2:					// CS_PUSHER2: Una caja mediana debe ser eyectada por SERVO2.
			
				if(!servo_active && conv_mode == MODE_NORMAL && IR_IsDetected(IR_ID_2))
				{
					SERVO_Set(SERVO_ID_2, SERVO_PUSH);
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}

				if(!servo_active && conv_mode == MODE_ESTIMATED && !est_delay_timer)
				{
					SERVO_Set(SERVO_ID_2, SERVO_PUSH);
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}
			
				if(servo_active && !servo_hold_timer)	// El tiempo de empuje termino.
				{
					SERVO_Set(SERVO_ID_2, SERVO_HOME);
					servo_active = FALSE;
					box_count[BOX_MEDIUM]++;
					stx.cmd        = CMD_BOX_EJECTED;
					stx.payload[0] = BOX_MEDIUM;
					stx.payloadLen = 1;
					buildCMD(&stx);
					state = CS_MEASURE;
				}
			
			break;
			
			case CS_PUSHER3:					// CS_PUSHER3: Una caja grande debe ser eyectada por SERVO3.
			
				if(!servo_active && !est_delay_timer)
				{
					SERVO_Set(SERVO_ID_3, SERVO_PUSH);
					servo_hold_timer = SERVO_HOLD_MS;
					servo_active = TRUE;
				}
			
				if(servo_active && !servo_hold_timer)	// El tiempo de empuje termino.
				{
					SERVO_Set(SERVO_ID_3, SERVO_HOME);
					servo_active = FALSE;
					box_count[BOX_BIG]++;
					stx.cmd        = CMD_BOX_EJECTED;
					stx.payload[0] = BOX_BIG;
					stx.payloadLen = 1;
					buildCMD(&stx);
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

