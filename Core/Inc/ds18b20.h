#ifndef DS18B20_H
#define DS18B20_H

#include "onewire.h"

#define DS18B20_CMD_SKIP_ROM     0xCC
#define DS18B20_CMD_CONVERT_T    0x44
#define DS18B20_CMD_READ_SCRATCH 0xBE

uint8_t DS18B20_Start(void);          /* sends the "start conversion" command, returns presence flag */
float   DS18B20_ReadTemp(void);       /* reads scratchpad, returns temperature in Celsius */

#endif /* DS18B20_H */
