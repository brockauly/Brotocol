/*
 * @file brotocol_slave.h
 * @brief Slave Brotocol header.
 * @author Brock Auly
 * @date Jul 30, 2025
 */

#ifndef INC_BROTOCOL_SLAVE_H_
#define INC_BROTOCOL_SLAVE_H_

#include <stdint.h>

/////////////////////////
///  BROTOCOL CONFIG  ///
#define ADDRESS_N       5
#define DATA_N          1
#define TIME_UNIT_MS   10
/////////////////////////

typedef enum {
	BUS_LOW,
	BUS_HIGH
} bus_state_t;

typedef enum {
	BROTOCOL_IDLE,
	BROTOCOL_ADDRESS,
	BROTOCOL_DATA,
	BROTOCOL_END
} brotocol_state_t;

typedef struct brotocol_slave {
	uint32_t address;
	uint32_t my_address;
	uint32_t data;
	brotocol_state_t brotocol_state;
	uint8_t is_waiting;
	uint8_t is_acking;
	bus_state_t bus_sampled_at_wait_end;
	uint8_t current_bit;
} brotocol_slave_t;

void Brotocol_Slave_Start(void);
void Brotocol_Reset(void);
void Brotocol_Process_Bus_State_Change(void);
void Brotocol_Process_Wait_End(void);
void Brotocol_Process_Brotocol(bus_state_t bus_state);
void Brotocol_Rx_Callback(uint32_t data);

/////////////////////////
/////      HAL      /////
void Brotocol_HAL_Init(void);
uint32_t Brotocol_Get_Address(void);
bus_state_t Brotocol_Get_Bus_State(void);
void Brotocol_Set_Bus_Low(void);
void Brotocol_Release_Bus(void);
void Brotocol_Wait_Ms(uint32_t duration_ms);
/////////////////////////

#endif /* INC_BROTOCOL_SLAVE_H_ */
