#ifndef THINGSPEAK_H
#define THINGSPEAK_H

#include "main.h"
#include <stdint.h>

uint8_t ThingSpeak_InitWiFi(UART_HandleTypeDef *huart, const char *ssid, const char *pass);
const char *ThingSpeak_GetIP(void);

uint8_t ThingSpeak_Publish(const char *write_api_key,
                           uint8_t temperature_valid, float temperature,
                           uint8_t humidity_valid, float humidity,
                           uint8_t light_valid, uint8_t light_percent,
                           uint16_t light_adc, uint8_t light_status,
                           uint8_t alert_tier);
const char *ThingSpeak_GetLastResult(void);

#endif
