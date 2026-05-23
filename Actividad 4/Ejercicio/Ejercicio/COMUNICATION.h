#ifndef COMUNICATION_H_
#define COMUNICATION_H_

#include <avr/io.h>	//Esta librería incluye acceso a los registros del microcontrolador.

#define BUFRXSIZE	128		  // Tamaño del buffer de recepción.
#define BUFTXSIZE	128		  // Tamaño del buffer de transmisión.

#define PAYLOADMAX  16		  // Tamaño máximo del payload.

typedef struct{
// Estructura del buffer circular.

	uint8_t *buf;				// Puntero al arreglo real donde se guardan los datos.
	uint8_t iw;					// Índice de escritura. Indica en qué posición se va a guardar el próximo dato recibido.
	uint8_t ir;					// Índice de lectura. Indica desde qué posición se va a leer el próximo dato.
	uint8_t size;				// Tamaño total del buffer.
	
}_sRingBuff;

typedef struct{
// Estructura de recepción.

	_sRingBuff rBuf;				// El buffer circular de recepción.
	uint8_t timeout;				// Controla tiempo máximo entre bytes.
	uint8_t hdrState;				// Estado del decodificador de cabecera del protocolo. Sirve para saber en qué parte de la trama estás.
	uint8_t nBytes;					// Cantidad de bytes esperados o recibidos.
	uint8_t bodyIndex;				// Indica en que parte del Length estás
	uint8_t cmd;					// Byte de CMD.
	uint8_t payload[PAYLOADMAX];	// Datos útiles que guardar.
	uint8_t payloadLen;				// Tamaño del Payload.
	uint8_t cks;					// Checksum.
}_sRX;

typedef struct{
// Estructura de transmisión.

	_sRingBuff rBuf;				// El buffer circular de recepción.
	uint8_t cmd;					// Byte de CMD.
	uint8_t length;					// Tamaño de los datos (cmd + payload + cks).
	uint8_t payload[PAYLOADMAX];	// Datos útiles que guardar.
	uint8_t payloadLen;				// Tamaño del Payload.
	uint8_t cks;					// Checksum.
	
}_sTX;

void ini_USART0 ();						// Función de inicialización de la comunicación USART, configurada en 115200 8N1.

void ini_COM(_sRX *srx, _sTX *stx);		/*	Función de inicialización del buffer y protocolo.
											Se debe de enviar:
											- Un puntero a una estructura de recepción.
											- Un puntero a una estructura de transmisión.
											Se la debe declarar antes de comenzar el bucle principal.	*/

_Bool decodeHeader(_sRX *srx);			/*	En esta función de valida la trama recibida por USART.
											El formato de trama esperado por el protocolo de comunicación:
											 _________________________________________________________________
											|     |     |     |     |        |     |     |         |          |
											| 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
											|_____|_____|_____|_____|________|_____|_____|_________|__________|
											Se debe de pasar como parámetro un puntero a una estructura de recepción.
											Esta función debe de llamarse constantemente en el bucle principal.
										*/	


void USART_SendByte(_sTX *stx);			// En esta función se envía 1 byte mediante USART.
										// Se debe pasar como parámetro Un puntero a una estructura de transmisión.

void buildCMD(_sTX *stx);				/* En esta función se arma el mensaje con el protocolo, el comando y el payload. 
										   El formato de trama creado por la función:
										   _________________________________________________________________
										   |     |     |     |     |        |     |     |         |          |
										   | 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
										   |_____|_____|_____|_____|________|_____|_____|_________|__________|
										   Se debe pasar como parámetro Un puntero a una estructura de transmisión.
								 	    */
#endif