/*
  CONVEYOR.c

  Driver de control de la cinta transportadora.
  Maneja el pin PC0 (A0) que habilita/deshabilita el motor de la cinta.
  El modulo arranca automaticamente en ConveyorControl_Init().
*/

#include "CONVEYOR.h"

static uint8_t conveyor_running = CONVEYOR_OFF;

void ConveyorControl_Init(void)
{
// En esta funcion se configura PC0 como salida y se arranca la cinta.

    DDRC |= (1 << CONVEYOR_CTRL_PIN);  // Configuro PC0 como salida.
    ConveyorControl_Start();           // Arranco la cinta automaticamente al inicializar.
}

void ConveyorControl_Start(void)
{
// En esta funcion se activa la cinta transportadora (HIGH en PC0).

    PORTC |= (1 << CONVEYOR_CTRL_PIN);  // Nivel alto: modulo en marcha.
    conveyor_running = CONVEYOR_ON;
}

void ConveyorControl_Stop(void)
{
// En esta funcion se detiene la cinta transportadora (LOW en PC0).

    PORTC &= ~(1 << CONVEYOR_CTRL_PIN);  // Nivel bajo: modulo detenido.
    conveyor_running = CONVEYOR_OFF;
}

void ConveyorControl_Update(void)
{
// Reservado para logica futura de control de velocidad o estado.
}

uint8_t ConveyorControl_IsRunning(void)
{
// Devuelve 1 si la cinta esta en marcha, 0 si esta detenida.

    return conveyor_running;
}
