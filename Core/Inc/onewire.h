#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "stm32l0xx_hal.h"

/* TIM2 is assumed configured as a free-running 1us-tick timer
 * (Prescaler = (TimerClock/1e6)-1, Period = 65535, Up counter).
 *
 * Call OW_Init(&htim2) once in main() after MX_TIM2_Init(). DHT22 uses its
 * timer helpers; the DS18B20 GPIO protocol must use a separate pin from PA7
 * when PA7 is configured for the LDR ADC input.
 */

#define OW_PORT   GPIOA
#define OW_PIN    GPIO_PIN_7

void    OW_Init(TIM_HandleTypeDef *timer);
void    delay_us(uint16_t us);

/* Raw counter access, so other drivers (e.g. DHT22) can time variable-length
 * pulses using the same free-running TIM2 tick instead of fixed delays. */
void     OW_ResetCounter(void);
uint16_t OW_GetCounter(void);

uint8_t OW_Reset(void);          /* returns 1 if a device responded, 0 if not */
void    OW_WriteBit(uint8_t bit);
uint8_t OW_ReadBit(void);
void    OW_WriteByte(uint8_t byte);
uint8_t OW_ReadByte(void);

#endif /* ONEWIRE_H */
