/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "onewire.h"
#include "dht22.h"
#include <stdio.h>
#include <string.h>
#include "lcd.h"
#include "thingspeak.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    float dht_temp;
    float dht_hum;
    uint8_t dht_valid;
} SensorData;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define WIFI_SSID     "R_wifi"          /* 2.4 GHz only */
#define WIFI_PASS     "mrt123456"

#define THINGSPEAK_WRITE_API_KEY "7FL5YVBHLZ00P084"

#define LIGHT_ADC_MAX 4095U
#define BUZZER_TONE_DUTY 568U
#define BUZZER_BEEP_INTERVALS 12U
#define THINGSPEAK_UPDATE_INTERVAL_MS 15000U

static SensorData sensors;
static volatile uint8_t buzzer_beep_tick_count;

static uint8_t ReadLightLevel(uint16_t *adc_value, uint8_t *percentage)
{
    if (HAL_ADC_Start(&hadc) != HAL_OK) {
        return 0;
    }

    if (HAL_ADC_PollForConversion(&hadc, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc);
        return 0;
    }

    *adc_value = (uint16_t)HAL_ADC_GetValue(&hadc);
    if (HAL_ADC_Stop(&hadc) != HAL_OK) {
        return 0;
    }

    *percentage = (uint8_t)(((*adc_value * 100U) + (LIGHT_ADC_MAX / 2U)) / LIGHT_ADC_MAX);
    return 1;
}

static const char *LightCategory(uint8_t percentage)
{
    if (percentage <= 10U) return "DARK";
    if (percentage <= 30U) return "SHADED";
    if (percentage <= 60U) return "INDOOR";
    if (percentage <= 85U) return "BRIGHT";
    return "SUNNY";
}

static uint8_t EvaluateAlertTier(uint8_t temperature_valid, float temperature,
                                 uint8_t humidity_valid, float humidity,
                                 uint8_t light_valid, uint8_t light_percent)
{
    if ((temperature_valid && temperature > 34.0f) ||
        (humidity_valid && humidity < 40.0f) ||
        (light_valid && light_percent > 50U)) {
        return 3U;
    }
    if ((temperature_valid && temperature >= 30.0f && temperature <= 34.0f) ||
        (humidity_valid && humidity < 50.0f) ||
        (light_valid && light_percent > 60U)) {
        return 2U;
    }
    if ((temperature_valid && temperature >= 27.0f && temperature <= 30.0f) ||
        (humidity_valid && humidity >= 50.0f && humidity <= 60.0f) ||
        (light_valid && light_percent > 70U)) {
        return 1U;
    }
    return 0U;
}

static const char *AlertTierName(uint8_t tier)
{
    switch (tier) {
    case 1U: return "Tier 1: Watch";
    case 2U: return "Tier 2: Alert";
    case 3U: return "Tier 3: Critical";
    default: return "No alert";
    }
}

static const char *AlertRecommendedAction(uint8_t tier)
{
    switch (tier) {
    case 1U:
        return "Increase scouting: inspect middle canopy leaves. Ensure adequate soil moisture to prevent plant stress. No shade net adjustment required unless light exceeds 80%.";
    case 2U:
        return "Deploy 30-40% shade nets to reduce leaf temperature by 3-5 C. Increase ventilation to lower internal heat, but monitor RH to avoid excessive drying.";
    case 3U:
        return "Deploy 40-50% shade nets immediately. Implement gentle overhead misting or syringing to raise local RH and dislodge mites. Reduce ventilation if external air is extremely dry.";
    default:
        return "Conditions are outside the configured alert tiers.";
    }
}

static const char *AlertDisplayText(uint8_t tier)
{
    switch (tier) {
    case 1U: return "TIER 1 WATCH\nSCOUT CANOPY";
    case 2U: return "TIER 2 ALERT\nDEPLOY SHADE";
    case 3U: return "TIER 3 CRITICAL\nSHADE + MIST";
    default: return "";
    }
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM21_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_ADC_Init();
  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) {
      Error_Handler();
  }
  /* USER CODE BEGIN 2 */
  OW_Init(&htim2);   /* DHT22 pulse timing uses the free-running TIM2 counter. */


  char msg[320];
  sprintf(msg, "Environmental logger starting...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

  uint8_t oled_ok = OLED_Init();
  if (oled_ok) {
      OLED_Clear();
      OLED_WriteCenteredString("Connecting\nWiFi");
      snprintf(msg, sizeof(msg), "OLED initialized on I2C\r\n");
  } else {
      snprintf(msg, sizeof(msg), "OLED I2C init FAILED (check power, wiring, address)\r\n");
  }
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

  uint8_t wifi_ok = ThingSpeak_InitWiFi(&huart1, WIFI_SSID, WIFI_PASS);

  if (wifi_ok) {
      snprintf(msg, sizeof(msg), "WiFi OK, IP %s\r\n", ThingSpeak_GetIP());
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
      if (oled_ok) {
          OLED_Clear();
          OLED_WriteCenteredString("WiFi OK\nThingSpeak\nready");
      }
  } else {
      snprintf(msg, sizeof(msg), "ESP-01S / WiFi init FAILED (check wiring, baud, SSID)\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
      if (oled_ok) {
          OLED_Clear();
          OLED_WriteCenteredString("WiFi init\nfailed");
      }
  }
  HAL_Delay(1500);
  if (oled_ok) {
      OLED_Clear();
  }

  uint32_t t_dht = 0, t_light = 0, t_oled = 0;
  uint32_t t_publish = HAL_GetTick() - THINGSPEAK_UPDATE_INTERVAL_MS;
  uint16_t light_adc = 0;
  uint8_t light_percent = 0;
  uint8_t light_valid = 0;
  uint8_t previous_alert_tier = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
   {
     uint32_t now = HAL_GetTick();

     /* 1) LDR on PA7: sample every second. With the LDR above the 10 kOhm
      * resistor, a higher ADC percentage corresponds to brighter light. */
     if ((now - t_light) >= 1000) {
         t_light = now;
         light_valid = ReadLightLevel(&light_adc, &light_percent);
         if (light_valid) {
             snprintf(msg, sizeof(msg), "LDR ADC: %u, Light: %u%% (%s)\r\n",
                      (unsigned)light_adc, (unsigned)light_percent,
                      LightCategory(light_percent));
         } else {
             snprintf(msg, sizeof(msg), "LDR ADC read failed\r\n");
         }
         HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
     }

    /* 2) DHT22 (PA6) every 2 s + UART log */
     if ((now - t_dht) >= 2000) {
         t_dht = now;
         DHT22_Data dht;

         if (DHT22_Read(&dht)) {
             sensors.dht_temp  = dht.temperature;
             sensors.dht_hum   = dht.humidity;
             sensors.dht_valid = 1;

             sprintf(msg, "DHT22 Temp: %.1f C  Humidity: %.1f %%\r\n", dht.temperature, dht.humidity);
         } else {
             sensors.dht_valid = 0;
             sprintf(msg, "DHT22 read failed (checksum/timeout)\r\n");
         }
         HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
     }

      {
          uint8_t alert_tier = EvaluateAlertTier(
              sensors.dht_valid, sensors.dht_temp,
              sensors.dht_valid, sensors.dht_hum,
              light_valid, light_percent);
          if (alert_tier != previous_alert_tier) {
              int alert_length = snprintf(msg, sizeof(msg), "[Alert] %s | %s\r\n",
                                          AlertTierName(alert_tier),
                                          AlertRecommendedAction(alert_tier));
              if (alert_length > 0) {
                  HAL_UART_Transmit(&huart2, (uint8_t *)msg,
                                    (uint16_t)((size_t)alert_length < sizeof(msg) ?
                                               (size_t)alert_length : sizeof(msg) - 1U),
                                    HAL_MAX_DELAY);
              }

              if (alert_tier > previous_alert_tier && alert_tier > 0U) {
                  (void)HAL_TIM_Base_Stop_IT(&htim21);
                  buzzer_beep_tick_count = 0;
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, BUZZER_TONE_DUTY);
                  __HAL_TIM_SET_COUNTER(&htim21, 0);
                  if (HAL_TIM_Base_Start_IT(&htim21) != HAL_OK) {
                      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
                      snprintf(msg, sizeof(msg), "Buzzer alert timer start failed\r\n");
                      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
                  }
              }
              previous_alert_tier = alert_tier;
          }
      }

     /* 3) Refresh the OLED from the latest sensor readings. */
     if (oled_ok && (now - t_oled) >= 1000) {
         t_oled = now;
         char oled_text[48];
         char temperature[12];
         char humidity[12];
         char light[12];
         if (sensors.dht_valid) {
             snprintf(temperature, sizeof(temperature), "T:%4.1fC", sensors.dht_temp);
             snprintf(humidity, sizeof(humidity), "H:%4.1f%%", sensors.dht_hum);
         } else {
             strcpy(temperature, "T:--.-C");
             strcpy(humidity, "H:--.-%");
         }
         if (light_valid) {
             snprintf(light, sizeof(light), "L:%u%%", (unsigned)light_percent);
         } else {
             strcpy(light, "L:ERR");
         }
         if (previous_alert_tier > 0U) {
             snprintf(oled_text, sizeof(oled_text), "%s",
                      AlertDisplayText(previous_alert_tier));
         } else {
             snprintf(oled_text, sizeof(oled_text), "%s\n%s\n%s\n%s",
                      temperature, humidity, light,
                      light_valid ? LightCategory(light_percent) : "NO LIGHT");
         }
         OLED_Clear();
         OLED_WriteCenteredString(oled_text);
     }

     /* 4) ThingSpeak accepts a free-channel update every 15 seconds. */
     if (wifi_ok && (now - t_publish) >= THINGSPEAK_UPDATE_INTERVAL_MS) {
         uint8_t light_status = !light_valid ? 5U :
             (light_percent <= 10U ? 0U :
              light_percent <= 30U ? 1U :
              light_percent <= 60U ? 2U :
              light_percent <= 85U ? 3U : 4U);
         uint8_t published;

         published = ThingSpeak_Publish(THINGSPEAK_WRITE_API_KEY,
                                         sensors.dht_valid, sensors.dht_temp,
                                         sensors.dht_valid, sensors.dht_hum,
                                         light_valid, light_percent, light_adc,
                                         light_status, previous_alert_tier);
         t_publish = HAL_GetTick();
         if (published) {
             snprintf(msg, sizeof(msg), "ThingSpeak update accepted (%s)\r\n",
                      ThingSpeak_GetLastResult());
         } else {
             snprintf(msg, sizeof(msg), "ThingSpeak update failed: %s\r\n",
                      ThingSpeak_GetLastResult());
         }
         HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
     }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_8;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM21) {
    buzzer_beep_tick_count++;
    if (buzzer_beep_tick_count >= BUZZER_BEEP_INTERVALS) {
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
      buzzer_beep_tick_count = 0;
      (void)HAL_TIM_Base_Stop_IT(&htim21);
    } else if ((buzzer_beep_tick_count & 1U) != 0U) {
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
    } else {
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, BUZZER_TONE_DUTY);
    }
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
