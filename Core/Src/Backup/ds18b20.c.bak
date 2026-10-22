#include "ds18b20.h"

uint8_t DS18B20_Start(void)
{
    uint8_t presence = OW_Reset();
    if (!presence) return 0;         /* no sensor detected on the line */

    OW_WriteByte(DS18B20_CMD_SKIP_ROM);
    OW_WriteByte(DS18B20_CMD_CONVERT_T);
    return 1;
}

float DS18B20_ReadTemp(void)
{
    uint8_t tempLSB, tempMSB;
    int16_t raw;

    OW_Reset();
    OW_WriteByte(DS18B20_CMD_SKIP_ROM);
    OW_WriteByte(DS18B20_CMD_READ_SCRATCH);

    tempLSB = OW_ReadByte();
    tempMSB = OW_ReadByte();
    /* remaining 6 scratchpad bytes (TH, TL, config, reserved x2, CRC) are
     * not read here; add OW_ReadByte() calls + CRC check for extra robustness */

    raw = (int16_t)((tempMSB << 8) | tempLSB);
    return raw * 0.0625f;   /* 12-bit default resolution: 0.0625 C per LSB */
}
