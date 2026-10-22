#include "thingspeak.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESP_RX_SIZE 1024
#define ESP_TEXT_SIZE 768
#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80

static UART_HandleTypeDef *esp_uart;
static uint8_t esp_rx_buffer[ESP_RX_SIZE];
static uint16_t esp_rx_tail;
static char esp_response[ESP_TEXT_SIZE];
static char esp_log_line[ESP_TEXT_SIZE + 96];
static char http_request[320];
static char esp_ip[20] = "0.0.0.0";
static char last_result[48] = "not attempted";

static uint16_t ESP_RxHead(void)
{
    return (uint16_t)((ESP_RX_SIZE - __HAL_DMA_GET_COUNTER(esp_uart->hdmarx)) % ESP_RX_SIZE);
}

static uint8_t ESP_RxGet(uint8_t *value)
{
    if (esp_rx_tail == ESP_RxHead()) {
        return 0;
    }
    *value = esp_rx_buffer[esp_rx_tail];
    esp_rx_tail = (uint16_t)((esp_rx_tail + 1U) % ESP_RX_SIZE);
    return 1;
}

static void ESP_RxFlush(void)
{
    esp_rx_tail = ESP_RxHead();
}

static int ESP_WaitFor(const char *token, uint32_t timeout_ms, uint8_t stop_on_error)
{
    size_t used = 0;
    uint8_t value;
    uint32_t start = HAL_GetTick();

    esp_response[0] = '\0';
    while ((HAL_GetTick() - start) < timeout_ms) {
        while (ESP_RxGet(&value)) {
            if (value == 0) {
                continue;
            }
            if (used >= sizeof(esp_response) - 1U) {
                memmove(esp_response, esp_response + sizeof(esp_response) / 2U,
                        used - sizeof(esp_response) / 2U);
                used -= sizeof(esp_response) / 2U;
            }
            esp_response[used++] = (char)value;
            esp_response[used] = '\0';

            if (strstr(esp_response, token) != NULL) {
                return 1;
            }
            if (stop_on_error &&
                (strstr(esp_response, "ERROR") != NULL ||
                 strstr(esp_response, "FAIL") != NULL ||
                 strstr(esp_response, "busy") != NULL)) {
                return -1;
            }
        }
    }
    return 0;
}

static int ESP_WaitForResponse(const char *token, uint32_t timeout_ms)
{
    size_t used = strlen(esp_response);
    uint8_t value;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        while (ESP_RxGet(&value)) {
            if (value == 0) {
                continue;
            }
            if (used >= sizeof(esp_response) - 1U) {
                memmove(esp_response, esp_response + sizeof(esp_response) / 2U,
                        used - sizeof(esp_response) / 2U);
                used -= sizeof(esp_response) / 2U;
            }
            esp_response[used++] = (char)value;
            esp_response[used] = '\0';
            if (strstr(esp_response, token) != NULL) {
                return 1;
            }
        }
    }
    return 0;
}

static int ESP_SendCommand(const char *command, const char *expected, uint32_t timeout_ms)
{
    ESP_RxFlush();
    if (HAL_UART_Transmit(esp_uart, (uint8_t *)command, (uint16_t)strlen(command), 1000) != HAL_OK ||
        HAL_UART_Transmit(esp_uart, (uint8_t *)"\r\n", 2, 1000) != HAL_OK) {
        snprintf(esp_response, sizeof(esp_response), "UART transmit failed");
        return 0;
    }
    return ESP_WaitFor(expected, timeout_ms, 1) == 1;
}

static void LogAtResult(const char *stage, uint8_t success)
{
    size_t used = (size_t)snprintf(esp_log_line, sizeof(esp_log_line), "[ESP AT] %s (%s): ",
                                   stage, success ? "OK" : "FAILED");
    size_t prefix_length = used;

    for (size_t i = 0; esp_response[i] != '\0' && used < sizeof(esp_log_line) - 4U; i++) {
        unsigned char c = (unsigned char)esp_response[i];
        if (c == '\r' || c == '\n') {
            if (used == 0 || esp_log_line[used - 1U] != ' ') {
                esp_log_line[used++] = ' ';
            }
        } else if (c >= 0x20 && c <= 0x7e) {
            esp_log_line[used++] = (char)c;
        }
    }
    if (used == prefix_length) {
        used += (size_t)snprintf(esp_log_line + used, sizeof(esp_log_line) - used, "%s", "no response");
    }
    esp_log_line[used++] = '\r';
    esp_log_line[used++] = '\n';
    HAL_UART_Transmit(&huart2, (uint8_t *)esp_log_line, (uint16_t)used, HAL_MAX_DELAY);
}

static const char *FindHeaderValue(const char *headers, const char *name)
{
    size_t name_length = strlen(name);

    while (*headers != '\0') {
        const char *line_end = strstr(headers, "\r\n");
        size_t line_length = line_end != NULL ? (size_t)(line_end - headers) : strlen(headers);
        size_t i;

        if (line_length >= name_length) {
            for (i = 0; i < name_length; i++) {
                char a = headers[i];
                char b = name[i];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                if (a != b) break;
            }
            if (i == name_length && headers[name_length] == ':') {
                const char *value = headers + name_length + 1U;
                while (*value == ' ' || *value == '\t') value++;
                return value;
            }
        }

        if (line_end == NULL) break;
        headers = line_end + 2;
    }
    return NULL;
}

static void LogHttpResponse(int status_code, const char *body, size_t body_length,
                            const char *content_length_text)
{
    char line[160];
    size_t used;
    size_t i;
    int written = snprintf(line, sizeof(line),
                           "[ThingSpeak] HTTP status=%d Content-Length=%s body[%u]=\"",
                           status_code, content_length_text != NULL ? content_length_text : "unknown",
                           (unsigned)body_length);

    used = written > 0 ? (size_t)written : 0U;
    if (used >= sizeof(line)) used = sizeof(line) - 1U;
    for (i = 0; i < body_length && i < 64U && used < sizeof(line) - 4U; i++) {
        unsigned char c = (unsigned char)body[i];
        line[used++] = (c >= 0x20U && c <= 0x7eU) ? (char)c : '.';
    }
    if (body_length > 64U && used < sizeof(line) - 5U) {
        line[used++] = '.';
        line[used++] = '.';
        line[used++] = '.';
    }
    line[used++] = '"';
    line[used++] = '\r';
    line[used++] = '\n';
    HAL_UART_Transmit(&huart2, (uint8_t *)line, (uint16_t)used, HAL_MAX_DELAY);
}

static int ESP_SendTcp(const uint8_t *data, uint16_t length)
{
    char command[64];
    int result;

    snprintf(command, sizeof(command), "AT+CIPSEND=%u", (unsigned)length);
    if (!ESP_SendCommand(command, ">", 2000)) {
        return 0;
    }
    if (HAL_UART_Transmit(esp_uart, (uint8_t *)data, length, 5000) != HAL_OK) {
        return 0;
    }
    result = ESP_WaitFor("SEND OK", 8000, 1);
    return result == 1;
}

uint8_t ThingSpeak_InitWiFi(UART_HandleTypeDef *huart, const char *ssid, const char *pass)
{
    char command[96];
    uint8_t alive = 0;

    esp_uart = huart;
    esp_rx_tail = 0;
    if (HAL_UART_Receive_DMA(esp_uart, esp_rx_buffer, ESP_RX_SIZE) != HAL_OK) {
        return 0;
    }
    CLEAR_BIT(esp_uart->Instance->CR3, USART_CR3_EIE);

    ESP_SendCommand("AT+RST", "ready", 5000);
    HAL_Delay(500);
    for (uint8_t attempt = 0; attempt < 5U && !alive; attempt++) {
        alive = (uint8_t)ESP_SendCommand("AT", "OK", 1000);
    }
    if (!alive) {
        return 0;
    }

    ESP_SendCommand("ATE0", "OK", 1000);
    if (!ESP_SendCommand("AT+CWMODE=1", "OK", 2000)) {
        return 0;
    }

    snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"", ssid, pass);
    if (!ESP_SendCommand(command, "OK", 30000)) {
        return 0;
    }
    if (!ESP_SendCommand("AT+CIPMUX=0", "OK", 2000)) {
        return 0;
    }

    if (ESP_SendCommand("AT+CIFSR", "OK", 2000)) {
        char *address = strstr(esp_response, "STAIP,\"");
        if (address != NULL) {
            char *end;
            address += 7;
            end = strchr(address, '"');
            if (end != NULL && (size_t)(end - address) < sizeof(esp_ip)) {
                memcpy(esp_ip, address, (size_t)(end - address));
                esp_ip[end - address] = '\0';
            }
        }
    }
    return 1;
}

const char *ThingSpeak_GetIP(void)
{
    return esp_ip;
}

static uint8_t LightStatusCode(uint8_t light_status)
{
    return light_status <= 5U ? light_status : 5U;
}

uint8_t ThingSpeak_Publish(const char *write_api_key,
                           uint8_t temperature_valid, float temperature,
                           uint8_t humidity_valid, float humidity,
                           uint8_t light_valid, uint8_t light_percent,
                           uint16_t light_adc, uint8_t light_status)
{
    char temperature_value[16];
    char humidity_value[16];
    char light_value[8];
    char adc_value[8];
    char status_value[4];
    char command[64];
    char *body;
    char *http;
    char *header_end;
    char *content_length_header;
    char *content_length_end;
    char *response_end;
    char content_length_text[12];
    long entry_id;
    unsigned long content_length = 0;
    size_t body_length = 0;
    size_t available_body_length;
    int status_code = 0;
    int request_length;

    if (write_api_key == NULL || write_api_key[0] == '\0' ||
        strcmp(write_api_key, "REPLACE_WITH_CHANNEL_WRITE_API_KEY") == 0) {
        snprintf(last_result, sizeof(last_result), "set ThingSpeak write key");
        return 0;
    }

    if (temperature_valid) {
        snprintf(temperature_value, sizeof(temperature_value), "%.1f", (double)temperature);
    } else {
        temperature_value[0] = '\0';
    }
    if (humidity_valid) {
        snprintf(humidity_value, sizeof(humidity_value), "%.1f", (double)humidity);
    } else {
        humidity_value[0] = '\0';
    }
    if (light_valid) {
        snprintf(light_value, sizeof(light_value), "%u", (unsigned)light_percent);
        snprintf(adc_value, sizeof(adc_value), "%u", (unsigned)light_adc);
    } else {
        light_value[0] = '\0';
        adc_value[0] = '\0';
    }
    snprintf(status_value, sizeof(status_value), "%u", (unsigned)LightStatusCode(light_status));

    request_length = snprintf(http_request, sizeof(http_request),
        "GET /update?api_key=%s&field1=%s&field2=%s&field3=%s&field4=%s&field5=%s HTTP/1.1\r\n"
        "Host: " THINGSPEAK_HOST "\r\n"
        "Connection: close\r\n"
        "User-Agent: STM32L073\r\n\r\n",
        write_api_key, temperature_value, humidity_value,
        light_value, adc_value, status_value);
    if (request_length < 0 || (size_t)request_length >= sizeof(http_request)) {
        snprintf(last_result, sizeof(last_result), "HTTP request too large");
        return 0;
    }

    {
        char debug_line[128];
        int debug_length = snprintf(debug_line, sizeof(debug_line),
                                    "[ThingSpeak] Sending fields: f1=%s f2=%s f3=%s f4=%s f5=%s\r\n",
                                    temperature_value, humidity_value, light_value,
                                    adc_value, status_value);
        if (debug_length > 0) {
            HAL_UART_Transmit(&huart2, (uint8_t *)debug_line,
                             (uint16_t)((size_t)debug_length < sizeof(debug_line) ?
                                        (size_t)debug_length : sizeof(debug_line) - 1U),
                             HAL_MAX_DELAY);
        }
    }

    ESP_SendCommand("AT+CIPCLOSE", "OK", 1000);
    snprintf(command, sizeof(command), "AT+CIPSTART=\"TCP\",\"%s\",%u",
             THINGSPEAK_HOST, THINGSPEAK_PORT);
    if (!ESP_SendCommand(command, "OK", 15000)) {
        LogAtResult("ThingSpeak TCP open", 0);
        snprintf(last_result, sizeof(last_result), "TCP open failed");
        return 0;
    }
    LogAtResult("ThingSpeak TCP open", 1);

    if (!ESP_SendTcp((const uint8_t *)http_request, (uint16_t)request_length)) {
        LogAtResult("ThingSpeak HTTP send", 0);
        snprintf(last_result, sizeof(last_result), "HTTP send failed");
        ESP_SendCommand("AT+CIPCLOSE", "OK", 1000);
        return 0;
    }

    if (strstr(esp_response, "CLOSED") == NULL) {
        ESP_WaitForResponse("CLOSED", 8000);
    }

    http = strstr(esp_response, "HTTP/");
    if (http == NULL || sscanf(http, "HTTP/%*s %d", &status_code) != 1) {
        LogAtResult("ThingSpeak HTTP response missing", 0);
        snprintf(last_result, sizeof(last_result), "HTTP status not found");
        ESP_SendCommand("AT+CIPCLOSE", "OK", 1000);
        return 0;
    }

    header_end = strstr(http, "\r\n\r\n");
    if (header_end == NULL) {
        snprintf(last_result, sizeof(last_result), "HTTP headers incomplete");
        return 0;
    }
    body = header_end + 4;
    response_end = strstr(body, "CLOSED");
    available_body_length = response_end != NULL ? (size_t)(response_end - body) : strlen(body);

    content_length_header = (char *)FindHeaderValue(http, "Content-Length");
    if (content_length_header != NULL) {
        content_length = strtoul(content_length_header, &content_length_end, 10);
        if (content_length_end == content_length_header ||
            content_length > (unsigned long)available_body_length) {
            snprintf(last_result, sizeof(last_result), "HTTP body incomplete");
            LogHttpResponse(status_code, body, available_body_length, "incomplete");
            return 0;
        }
        body_length = (size_t)content_length;
        (void)snprintf(content_length_text, sizeof(content_length_text), "%lu", content_length);
    } else {
        char *closed = strstr(body, "CLOSED");
        body_length = closed != NULL ? (size_t)(closed - body) : strlen(body);
        while (body_length > 0U &&
               (body[body_length - 1U] == '\r' || body[body_length - 1U] == '\n')) {
            body_length--;
        }
        content_length_text[0] = '\0';
    }

    LogHttpResponse(status_code, body, body_length,
                    content_length_header != NULL ? content_length_text : NULL);
    if (status_code != 200) {
        LogAtResult("ThingSpeak HTTP response", 0);
        snprintf(last_result, sizeof(last_result), "HTTP status %d", status_code);
        ESP_SendCommand("AT+CIPCLOSE", "OK", 1000);
        return 0;
    }

    entry_id = strtol(body, &content_length_end, 10);
    if (entry_id <= 0) {
        LogAtResult("ThingSpeak update rejected", 0);
        snprintf(last_result, sizeof(last_result), "ThingSpeak returned %.20s",
                 body_length > 0U ? body : "(empty)");
        return 0;
    }

    snprintf(last_result, sizeof(last_result), "entry %ld", entry_id);
    LogAtResult("ThingSpeak update accepted", 1);
    return 1;
}

const char *ThingSpeak_GetLastResult(void)
{
    return last_result;
}
