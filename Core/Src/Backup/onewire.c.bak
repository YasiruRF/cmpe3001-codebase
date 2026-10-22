#include "onewire.h"

static TIM_HandleTypeDef *ow_tim;

void OW_Init(TIM_HandleTypeDef *timer)
{
    ow_tim = timer;
    HAL_TIM_Base_Start(ow_tim);          /* start the free-running 1us tick */
    HAL_GPIO_WritePin(OW_PORT, OW_PIN, GPIO_PIN_SET); /* idle = released/high */
}

/* Busy-wait microsecond delay using TIM2 as a free-running 1us counter */
void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(ow_tim, 0);
    while (__HAL_TIM_GET_COUNTER(ow_tim) < us) { }
}

void OW_ResetCounter(void)
{
    __HAL_TIM_SET_COUNTER(ow_tim, 0);
}

uint16_t OW_GetCounter(void)
{
    return __HAL_TIM_GET_COUNTER(ow_tim);
}

/* Pull the line low / release it (open-drain: "high" = release, pulled up externally) */
static inline void OW_Low(void)  { HAL_GPIO_WritePin(OW_PORT, OW_PIN, GPIO_PIN_RESET); }
static inline void OW_High(void) { HAL_GPIO_WritePin(OW_PORT, OW_PIN, GPIO_PIN_SET); }
static inline uint8_t OW_Read(void) { return HAL_GPIO_ReadPin(OW_PORT, OW_PIN); }

uint8_t OW_Reset(void)
{
    uint8_t presence;

    OW_Low();
    delay_us(480);          /* reset pulse: hold low 480us */
    OW_High();
    delay_us(70);           /* wait for the device's presence pulse */
    presence = (OW_Read() == 0) ? 1 : 0;   /* 0 = device pulled it low = present */
    delay_us(410);          /* finish out the 480us+ reset slot */

    return presence;
}

void OW_WriteBit(uint8_t bit)
{
    if (bit) {
        OW_Low();
        delay_us(6);
        OW_High();
        delay_us(64);
    } else {
        OW_Low();
        delay_us(60);
        OW_High();
        delay_us(10);
    }
}

uint8_t OW_ReadBit(void)
{
    uint8_t bit;

    OW_Low();
    delay_us(2);
    OW_High();
    delay_us(10);
    bit = OW_Read();
    delay_us(50);

    return bit;
}

void OW_WriteByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        OW_WriteBit(byte & 0x01);
        byte >>= 1;
    }
}

uint8_t OW_ReadByte(void)
{
    uint8_t byte = 0;
    for (uint8_t i = 0; i < 8; i++) {
        byte >>= 1;
        if (OW_ReadBit()) byte |= 0x80;
    }
    return byte;
}
