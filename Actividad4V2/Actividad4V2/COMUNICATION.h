/*
 * COMUNICATION.h
 *
 * Created: 23/5/2026 17:25:26
 *  Author: juani
 */

#ifndef COMUNICATION_H_
#define COMUNICATION_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// Comandos Firmware -> GUI
#define CMD_ERR_SENSOR      0xA0
#define CMD_DIST_MEAS       0xA1
#define CMD_BOX_CLASSIF     0xA2
#define CMD_BOX_EJECTED     0xA3
#define CMD_STATE           0xA4
#define CMD_COUNTS          0xA5
#define CMD_ACK             0xA6
#define CMD_BOX_DISCARDED   0xA7
#define CMD_BELT_SPEED      0xA8    // Payload: speed_H, speed_L (mm/s, big-endian)

// Comandos GUI -> Firmware
#define CMD_GET_STATE       0xB0
#define CMD_GET_COUNTS      0xB1
#define CMD_SET_MODE        0xB2
#define CMD_SET_THRESH      0xB4
#define CMD_SET_CALIB       0xB5
#define CMD_RESET_COUNTS    0xB6
#define CMD_SET_SPEED       0xB7    // Payload: delay_p1_H, delay_p1_L, ..._p3_L (ms, big-endian)
#define CMD_SET_SERVO       0xB8    // Payload: servo_id (0-2), home_ms, push_ms
#define CMD_SET_CONVEYOR    0xB9    // Payload: 0=stop, 1=start

// ACK status
#define ACK_OK              0x00
#define ACK_BUSY            0x01
#define ACK_ERROR           0x02

#define RX_BUFFER_SIZE      128
#define COM_PAYLOAD_SIZE    8

void     COM_Init();
void     COM_SendFrame(uint8_t cmd, uint8_t *payload, uint8_t len);
void     COM_Update();
uint8_t  COM_FrameAvailable();
void     COM_GetFrame(uint8_t *cmd, uint8_t *payload, uint8_t *len);

#endif /* COMUNICATION_H_ */
