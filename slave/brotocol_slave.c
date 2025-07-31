/*
 * @file brotocol_slave.c
 * @brief Slave Brotocol implementation.
 * @author Brock Auly
 * @date Jul 30, 2025
 *
 * @note This file should not be modified, only the HAL.
 */

#include "brotocol_slave.h"

volatile brotocol_slave_t hbro;

void Brotocol_Slave_Start(void)
{
	hbro.my_address = Brotocol_Get_Address();
	Brotocol_HAL_Init();
	Brotocol_Reset();
}

void Brotocol_Reset(void)
{
	hbro.address = (uint32_t) 0;
	hbro.data = (uint32_t) 0;
	hbro.brotocol_state = BROTOCOL_IDLE;
	hbro.is_waiting = 0;
	hbro.is_acking = 0;
	hbro.bus_sampled_at_wait_end = BUS_HIGH;
	hbro.current_bit = 0;
	Brotocol_Release_Bus();
}

void Brotocol_Process_Bus_State_Change(void)
{
	Brotocol_Process_Brotocol(Brotocol_Get_Bus_State());
}

void Brotocol_Process_Wait_End(void)
{
	hbro.bus_sampled_at_wait_end = Brotocol_Get_Bus_State();
	hbro.is_waiting = 0;
	if (hbro.is_acking) {
		Brotocol_Release_Bus();
		hbro.is_acking = 0;
	}
	if (hbro.brotocol_state == BROTOCOL_END) {
		Brotocol_Rx_Callback(hbro.data);
		Brotocol_Reset();
	}
}

void Brotocol_Process_Brotocol(bus_state_t bus_state)
{
	switch (hbro.brotocol_state) {
	case BROTOCOL_IDLE:
		if (bus_state == BUS_LOW) {
			if (! hbro.is_waiting) {
				// we caught the beginning of a Start symbol
				hbro.is_waiting = 1;
				Brotocol_Wait_Ms(15 * TIME_UNIT_MS);
			} else {
				// what we thought to be the Start symbol was not, resetting all to default
				Brotocol_Reset();
			}
		} else { // bus_state == BUS_HIGH
			if (! hbro.is_waiting) {
				if (hbro.bus_sampled_at_wait_end == BUS_LOW) {
					// we caught the end of the Start symbol !
					hbro.brotocol_state = BROTOCOL_ADDRESS;
				} else {
					// it was not a Start symbol (not long enough), resetting
					// actually this should never happen, never be too careful
					Brotocol_Reset();
				}
			} else {
				// still waiting, the Start symbol was not long enough, resetting
				Brotocol_Reset();
			}
		}
		break;
	case BROTOCOL_ADDRESS:
		if (bus_state == BUS_LOW) {
			if (! hbro.is_waiting) {
				// start of a symbol
				if (hbro.current_bit < ADDRESS_N) {
					hbro.is_waiting = 1;
					Brotocol_Wait_Ms(5 * TIME_UNIT_MS);
				} else {
					// check address match
					if (hbro.address == hbro.my_address) {
						// it's me ! perform ACK
						hbro.is_acking = 1;
						Brotocol_Set_Bus_Low();
						hbro.is_waiting = 1;
						Brotocol_Wait_Ms(8 * TIME_UNIT_MS);
						hbro.current_bit = 0;
						hbro.brotocol_state = BROTOCOL_DATA;
					} else {
						// it's not for me, reset
						Brotocol_Reset();
					}
				}
			} else {
				// error, resetting
				Brotocol_Reset();
			}
		} else { // bus_state == BUS_HIGH
			if (! hbro.is_waiting) {
				// it must be a 1, but we can still verify
				if ( hbro.bus_sampled_at_wait_end == BUS_LOW) {
					// it's a 1 !
					hbro.address |= 0x1 << hbro.current_bit;
					hbro.current_bit++;
				} else {
					// error should be a 1, resetting
					Brotocol_Reset();
				}
			} else {
				// we assume it's a 0 and cannot verify because not sampled yet !
				// no need to write the bit, it's already 0 ...
				hbro.current_bit++;
			}
		}
		break;
	case BROTOCOL_DATA: // actually nearly the same as ADDRESS
		if (bus_state == BUS_LOW) {
			if (! hbro.is_waiting) {
				// start of a symbol
				if (hbro.current_bit < DATA_N) {
					hbro.is_waiting = 1;
					Brotocol_Wait_Ms(5 * TIME_UNIT_MS);
				} else {
					// perform ACK if data accepted
					// TODO how to manage end of transmission ?
					hbro.is_acking = 1;
					Brotocol_Set_Bus_Low();
					hbro.is_waiting = 1;
					Brotocol_Wait_Ms(8 * TIME_UNIT_MS);
					hbro.brotocol_state = BROTOCOL_END;
				}
			} else {
				// error, resetting
				Brotocol_Reset();
			}
		} else { // bus_state == BUS_HIGH
			if (! hbro.is_waiting) {
				// it must be a 1, but we can still verify
				if ( hbro.bus_sampled_at_wait_end == BUS_LOW) {
					// it's a 1 !
					hbro.data |= 0x1 << hbro.current_bit;
					hbro.current_bit++;
				} else {
					// error should be a 1, resetting
					Brotocol_Reset();
				}
			} else {
				// we assume it's a 0 and cannot verify because not sampled yet !
				// no need to write the bit, it's already 0 ...
				hbro.current_bit++;
			}
		}
		break;
	case BROTOCOL_END:
		// should never reach here
		Brotocol_Reset();
		break;
	}
}

__attribute__((weak)) void Brotocol_Rx_Callback(uint32_t data)
{
	(void)data;
	// This function should be overwritten in user file
}
