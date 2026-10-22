#include "dht22.h"
#include "onewire.h"   /* reuses delay_us / OW_GetCounter / OW_ResetCounter (TIM2) */

static inline void DHT_Low(void)  { HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET); }
static inline void DHT_High(void) { HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET); }
static inline uint8_t DHT_Read(void) { return HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN); }

/* Waits for the pin to reach `level`, giving up after `timeout_us`.
 * Returns 1 if it reached that level in time, 0 on timeout. */
static uint8_t DHT_WaitForLevel(GPIO_PinState level, uint16_t timeout_us)
{
    OW_ResetCounter();
    while (DHT_Read() != level) {
        if (OW_GetCounter() > timeout_us) return 0;
    }
    return 1;
}

uint8_t DHT22_Read(DHT22_Data *out)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};

    /* --- start signal: host pulls line low for >=1ms, then releases --- */
    DHT_Low();
    HAL_Delay(2);        /* 2ms is comfortably inside the 1-18ms spec */
    DHT_High();
    delay_us(30);        /* release, then switch to listening for the response */

    /* --- sensor response: low ~80us, then high ~80us --- */
    if (!DHT_WaitForLevel(GPIO_PIN_RESET, 100)) return 0;  /* sensor pulls low */
    if (!DHT_WaitForLevel(GPIO_PIN_SET, 100))   return 0;  /* sensor releases */
    if (!DHT_WaitForLevel(GPIO_PIN_RESET, 100)) return 0;  /* first data bit starts */

    /* --- 40 data bits: each bit = ~50us low, then high for 26-28us (=0) or ~70us (=1) --- */
    for (uint8_t i = 0; i < 40; i++) {
        if (!DHT_WaitForLevel(GPIO_PIN_SET, 100)) return 0;   /* wait out the 50us low */

        OW_ResetCounter();
        if (!DHT_WaitForLevel(GPIO_PIN_RESET, 100)) return 0; /* time the high pulse */
        uint16_t highTime = OW_GetCounter();

        data[i / 8] <<= 1;
        if (highTime > 40) data[i / 8] |= 0x01;   /* ~70us high => 1, ~27us high => 0 */
    }

    /* --- checksum: low byte of the sum of the first 4 bytes --- */
    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) return 0;

    uint16_t rawHumidity = (data[0] << 8) | data[1];
    uint16_t rawTempMag  = ((data[2] & 0x7F) << 8) | data[3];

    out->humidity = rawHumidity / 10.0f;
    out->temperature = rawTempMag / 10.0f;
    if (data[2] & 0x80) out->temperature = -out->temperature;  /* sign bit */

    return 1;
}
