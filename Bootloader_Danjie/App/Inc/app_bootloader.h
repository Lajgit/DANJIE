#ifndef __APP_BOOTLOADER_H__
#define __APP_BOOTLOADER_H__ 

#include "main.h"
#include "app_sd.h"


#define BOOTLOADER_ADDR 0x08000000 //bootloader存放地址
#define APP_ADDR 0x0800C000 //应用程序存放地址

HAL_StatusTypeDef Flash_Program(uint32_t StartAddress, uint8_t *Data, uint32_t Size);
void JumpToApplication(void);
void App_Bootloader(void);
#endif