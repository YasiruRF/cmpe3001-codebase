#ifndef DHT22_H
#define DHT22_H

#include "stm32l0xx_hal.h"

/* DHT22 / AM2302 on PA6, open-drain output + external/onboard pull-up.
 * Reuses the same TIM2 1us-tick timer set up for the DS18B20 (call
 * OW_Init(&htim2) once at startup — that also starts TIM2 and delay_us()). */

#define DHT_PORT  GPIOA
#define DHT_PIN   GPIO_PIN_6

typedef struct {
    float humidity;     /* %RH */
    float temperature;  /* Celsius */
} DHT22_Data;

/* returns 1 on a successful read (checksum OK), 0 on failure/timeout.
 * DHT22 needs >=2 seconds between calls — don't poll it faster than that. */
uint8_t DHT22_Read(DHT22_Data *out);

#endif /* DHT22_H */
