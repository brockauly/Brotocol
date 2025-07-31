/*
 * @file brotocol_slave_hal.c
 * @brief Slave Brotocol Hardware Abstraction Layer implementation.
 * @author Brock Auly
 * @date Jul 30, 2025
 *
 * @note This HAL is specific for the STM32F072 and should be reimplemented for other platforms.
 * User should implement the functions defined in brotocol_slave.h in HAL section
 * and call Brotocol_Process_Bus_State_Change() at each rising and falling edge on the bus.
 */

#include "main.h"
#include "brotocol_slave.h"
#include <stdio.h>

// TIM7 is already configured through CubeMX as a One-Pulse-Mode 1ms-tick timer.
extern TIM_HandleTypeDef htim7;

// A single pin cannot act as output open-drain and trigger interrupts at the same time.
// Thus I reconfigure it from one mode to the other as needed
typedef enum {
	DUAL_MODE_OUTPUT_OD,
	DUAL_MODE_EXTI,
} dual_mode_pin_t;
void Set_Dual_Mode_Pin(GPIO_TypeDef  *GPIOx, uint16_t GPIO_Pin, IRQn_Type IRQn, dual_mode_pin_t mode);

// I use my device's unique ID to configure the Brotocol address
static const uint32_t UIDs[30][3] = {
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u},
		{0x00000000u, 0x00000000u, 0x00000000u}
};
static const uint32_t BROTOCOL_ADDRESSES[30] = {
		0x01,
		0x02,
		0x03,
		0x04,
		0x05,
		0x06,
		0x07,
		0x08,
		0x09,
		0x0a,
		0x0b,
		0x0c,
		0x0d,
		0x0e,
		0x0f,
		0x10,
		0x11,
		0x12,
		0x13,
		0x14,
		0x15,
		0x16,
		0x17,
		0x18,
		0x19,
		0x1a,
		0x1b,
		0x1c,
		0x1d,
		0x1e
};

/*
 * Hardware initialisation, performed once at program startup.
 */
void Brotocol_HAL_Init(void)
{
	// Initialisation is done in main by CubeMX
	// TIM7 in OPM with 1ms tick
	// Brotocol GPIO in EXTI mode with NVIC enabled
}

void Set_Dual_Mode_Pin(GPIO_TypeDef  *GPIOx, uint16_t GPIO_Pin, IRQn_Type IRQn, dual_mode_pin_t mode) {
	if (mode == DUAL_MODE_OUTPUT_OD) {
		HAL_NVIC_DisableIRQ(IRQn);
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = GPIO_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
	} else {
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = GPIO_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
		HAL_NVIC_EnableIRQ(IRQn);
	}
}

/*
 * Assigns a unique Brotocol address to this device
 * Only ADDRESS_N bits may be set
 */
uint32_t Brotocol_Get_Address(void)
{
	uint32_t uid[3];
	uint32_t addr = 0;

	// Read unique ID
	uid[0] = HAL_GetUIDw0();
	uid[1] = HAL_GetUIDw1();
	uid[2] = HAL_GetUIDw2();
	printf("[UID] %08lx%08lx%08lx\r\n", uid[0], uid[1], uid[2]);

	for (uint8_t i = 0; i < 30; ++i ) {
		if ( uid[0] == UIDs[i][0] && uid[1] == UIDs[i][1] && uid[2] == UIDs[i][2] ) {
			addr = BROTOCOL_ADDRESSES[i];
			break;
		}
	}
	if (! addr ) { // addresses start at 0x01
		printf("[BRO] Error while matching device ID\r\n");
		Error_Handler();
	} else {
		printf("[BRO] Configured address %02lx\r\n", addr);
	}
	return addr;
}

/*
 * Read and return the bus state, either BUS_LOW or BUS_HIGH
 */
bus_state_t Brotocol_Get_Bus_State(void)
{
	return HAL_GPIO_ReadPin(Brotocol_GPIO_Port, Brotocol_Pin) == GPIO_PIN_RESET ? BUS_LOW : BUS_HIGH;
}

/*
 * Set the bus low by pulling it to ground
 */
void Brotocol_Set_Bus_Low(void)
{
	Set_Dual_Mode_Pin(Brotocol_GPIO_Port, Brotocol_Pin, Brotocol_EXTI_IRQn, DUAL_MODE_OUTPUT_OD);
	HAL_GPIO_WritePin(Brotocol_GPIO_Port, Brotocol_Pin, GPIO_PIN_RESET);
}

/*
 * Release the bus to a high-Z or open-drain state
 */
void Brotocol_Release_Bus(void)
{
	HAL_GPIO_WritePin(Brotocol_GPIO_Port, Brotocol_Pin, GPIO_PIN_SET);
	Set_Dual_Mode_Pin(Brotocol_GPIO_Port, Brotocol_Pin, Brotocol_EXTI_IRQn, DUAL_MODE_EXTI);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == Brotocol_Pin) {
		Brotocol_Process_Bus_State_Change();
	}
}

/*
 * Start a waiting period in milliseconds.
 * At the end of the period, Brotocol_Process_Wait_End() must be called.
 * A blocking implementation should work, but the library is async.
 */
void Brotocol_Wait_Ms(uint32_t duration_ms)
{
	HAL_TIM_Base_Stop_IT(&htim7);
	__HAL_TIM_SET_COUNTER(&htim7, 0);
	__HAL_TIM_SET_AUTORELOAD(&htim7, duration_ms - 1);
	HAL_TIM_Base_Start_IT(&htim7);
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == &htim7) {
		Brotocol_Process_Wait_End();
	}
}
