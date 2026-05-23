#ifndef COMUNICATION_H_
#define COMUNICATION_H_

#include <avr/io.h>	//Esta librer�a incluye acceso a los registros del microcontrolador.

#define BUFRXSIZE	128		  // Tama�o del buffer de recepci�n.
#define BUFTXSIZE	128		  // Tama�o del buffer de transmisi�n.

#define PAYLOADMAX  16		  // Tama�o m�ximo del payload.

typedef struct{
// Estructura del buffer circular.

	uint8_t *buf;				// Puntero al arreglo real donde se guardan los datos.
	uint8_t iw;					// �ndice de escritura. Indica en qu� posici�n se va a guardar el pr�ximo dato recibido.
	uint8_t ir;					// �ndice de lectura. Indica desde qu� posici�n se va a leer el pr�ximo dato.
	uint8_t size;				// Tama�o total del buffer.
	
}_sRingBuff;

typedef struct{
// Estructura de recepci�n.

	_sRingBuff rBuf;				// El buffer circular de recepci�n.
	uint8_t timeout;				// Controla tiempo m�ximo entre bytes.
	uint8_t hdrState;				// Estado del decodificador de cabecera del protocolo. Sirve para saber en qu� parte de la trama est�s.
	uint8_t nBytes;					// Cantidad de bytes esperados o recibidos.
	uint8_t bodyIndex;				// Indica en que parte del Length est�s
	uint8_t cmd;					// Byte de CMD.
	uint8_t payload[PAYLOADMAX];	// Datos �tiles que guardar.
	uint8_t payloadLen;				// Tama�o del Payload.
	uint8_t cks;					// Checksum.
}_sRX;

typedef struct{
// Estructura de transmisi�n.

	_sRingBuff rBuf;				// El buffer circular de recepci�n.
	uint8_t cmd;					// Byte de CMD.
	uint8_t length;					// Tama�o de los datos (cmd + payload + cks).
	uint8_t payload[PAYLOADMAX];	// Datos �tiles que guardar.
	uint8_t payloadLen;				// Tama�o del Payload.
	uint8_t cks;					// Checksum.
	
}_sTX;

void ini_USART0 ();						// Funci�n de inicializaci�n de la comunicaci�n USART, configurada en 115200 8N1.

void ini_COM(_sRX *srx, _sTX *stx);		/*	Funci�n de inicializaci�n del buffer y protocolo.
											Se debe de enviar:
											- Un puntero a una estructura de recepci�n.
											- Un puntero a una estructura de transmisi�n.
											Se la debe declarar antes de comenzar el bucle principal.	*/

_Bool decodeHeader(_sRX *srx);			/*	En esta funci�n de valida la trama recibida por USART.
											El formato de trama esperado por el protocolo de comunicaci�n:
											 _________________________________________________________________
											|     |     |     |     |        |     |     |         |          |
											| 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
											|_____|_____|_____|_____|________|_____|_____|_________|__________|
											Se debe de pasar como par�metro un puntero a una estructura de recepci�n.
											Esta funci�n debe de llamarse constantemente en el bucle principal.
										*/	


void USART_SendByte(_sTX *stx);			// En esta funci�n se env�a 1 byte mediante USART.
										// Se debe pasar como par�metro Un puntero a una estructura de transmisi�n.

void buildCMD(_sTX *stx);				/* En esta funci�n se arma el mensaje con el protocolo, el comando y el payload. 
										   El formato de trama creado por la funci�n:
										   _________________________________________________________________
										   |     |     |     |     |        |     |     |         |          |
										   | 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
										   |_____|_____|_____|_____|________|_____|_____|_________|__________|
										   Se debe pasar como par�metro Un puntero a una estructura de transmisi�n.
								 	    */

// Comandos firmware -> GUI (TX)
#define CMD_ERR_SENSOR   0xA0	// Error en HC-SR04.
#define CMD_DIST_MEAS    0xA1	// Medicion de distancia (payload: d_cm).
#define CMD_BOX_CLASSIF  0xA2	// Caja clasificada (payload: type).
#define CMD_BOX_EJECTED  0xA3	// Caja eyectada (payload: type).
#define CMD_STATE        0xA4	// Estado de la cinta (payload: state).
#define CMD_COUNTS       0xA5	// Contadores de cajas (payload: 6 bytes).
#define CMD_ACK          0xA6	// ACK/NAK de comando (payload: echo_cmd, status).

// Comandos GUI -> firmware (RX)
#define CMD_GET_STATE    0xB0	// Consulta estado actual.
#define CMD_GET_COUNTS   0xB1	// Consulta contadores.
#define CMD_SET_MODE     0xB2	// Cambia modo: 0=Normal, 1=Estimado.
#define CMD_SET_BOX_MAP  0xB3	// Mapeo caja->servo (payload: s_srv m_srv b_srv).
#define CMD_SET_THRESH   0xB4	// Umbrales de altura (payload: h_small h_med h_big).
#define CMD_SET_CALIB    0xB5	// Calibracion distancia de referencia (payload: ref_dist).
#define CMD_RESET_COUNTS 0xB6	// Reinicia contadores.

// Status de ACK
#define ACK_OK           0x00	// Comando procesado correctamente.
#define ACK_BUSY         0x01	// Cinta en movimiento, comando rechazado.
#define ACK_INVALID      0x02	// Comando no reconocido o payload invalido.

#endif