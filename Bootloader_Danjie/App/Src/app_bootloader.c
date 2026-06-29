#include "app_bootloader.h"
#include "app_ws2812.h"
extern FATFS fs;
extern FIL fil;
extern uint8_t r_buffer[1024];
static uint16_t RGB_buffer[Light_CRRbuffer_SIZE];

// Flash编程函数
HAL_StatusTypeDef Flash_Program(uint32_t StartAddress, uint8_t *Data, uint32_t Size)
{
    uint32_t *pData = (uint32_t *)Data;
    uint32_t address = StartAddress;
    uint32_t words = (Size + 3) / 4; // 字节转字(向上取整)

    for (uint32_t i = 0; i < words; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *pData) != HAL_OK)
        {
            return HAL_ERROR;
        }
        address += 4;
        pData++;
    }
    return HAL_OK;
}

// 跳转到应用程序
void JumpToApplication(void)
{
    typedef void (*pFunction)(void);
    pFunction Jump_To_App;

    // 检查应用程序栈指针
    // if (((*(__IO uint32_t *)APP_ADDR) & 0x2FFE0000) == 0x20000000)
    //{

    // 初始化应用程序栈指针
    __set_MSP(*(__IO uint32_t *)APP_ADDR);

    // 获取应用程序入口点
    Jump_To_App = (pFunction)(*(__IO uint32_t *)(APP_ADDR + 4));

    __disable_irq();

    // 设置向量表位置
    SCB->VTOR = APP_ADDR;
// 跳转到应用程序
#if DEBUG_PRINT
    printf("Jumping to app_address\r\n");
#endif
    Jump_To_App();
    //}
}

void App_Bootloader(void)
{
    // 挂载文件系统
    if (mount_disk(&fs, "", 0) != FR_OK)
    {
        // 挂载失败处理
        // Error_Handler();
        f_mount(NULL, "", 0);
        JumpToApplication();
    }
    HAL_StatusTypeDef ProgramStatus;
    FIL firmwareFile;
    UINT bytesRead;
    uint32_t fileSize, sectorError;

    // 打开固件文件
    if (f_open(&firmwareFile, FileName_Bin, FA_READ) == FR_OK)
    {
#if DEBUG_PRINT
        printf("FirmwareFile Open Sucess!\r\n");
#endif
        fileSize = f_size(&firmwareFile);
        uint8_t *buffer = malloc(1024);
        // 配置flash擦除结构体
        FLASH_EraseInitTypeDef erase = {0};
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Sector = FLASH_SECTOR_3; // 从扇区3（0x0800C000）开始擦除
        erase.NbSectors = 6;           // 擦除6个扇区
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        // 解锁Flash
        if (HAL_FLASH_Unlock() == HAL_OK)
        {
            // 擦除应用程序区域
            if (HAL_FLASHEx_Erase(&erase, &sectorError) != HAL_OK)
            {
#if DEBUG_PRINT
                printf("Flash erase error!\r\n");
#endif
                return;
            }
        }
        else
        {
#if DEBUG_PRINT
            printf("flash unlock error");
#endif
            return;
        }

        uint32_t offset = 0;
        while (offset < fileSize)
        {
            // 从SD卡读取数据
            read_file(&firmwareFile, buffer, 1024, &bytesRead);

            // 编程Flash
            ProgramStatus = Flash_Program(APP_ADDR + offset, buffer, bytesRead);
#if DEBUG_PRINT
            if (ProgramStatus != HAL_OK)
            {
                printf("Flash programming error\r\n"); // 编程错误处理
                break;
            }
            else
            {
                printf("Programmed %d bytes\r\n", bytesRead);
            }
#endif

            offset += bytesRead;
        }
        if (offset >= fileSize) // 写入成功
        {
#if DEBUG_PRINT
            printf("Program success!\r\n");
#endif
            Light_SetAllColor(GREEN, RGB_buffer);
            for (uint8_t i = 0; i < 6; i++)
            {
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
                HAL_Delay(100);
            }
        }
        else
        {
#if DEBUG_PRINT
            printf("Program failed!\r\n");
#endif
            Light_SetAllColor(RED, RGB_buffer);
        }

        // 关闭文件
        close_file(&firmwareFile);
        free(buffer);     // 释放空间
        HAL_FLASH_Lock(); // 锁定flash

        // 卸载文件系统
        f_mount(NULL, "", 0);
        HAL_Delay(1000);
        // 跳转到应用程序
        JumpToApplication();
    }
    else
    {
#if DEBUG_PRINT
        printf("FirmwareFile Open Faild!\r\n");
#endif
        HAL_FLASH_Lock();
        f_mount(NULL, "", 0);
        Light_SetAllColor(SKYBLUE, RGB_buffer);
        HAL_Delay(1000);
        JumpToApplication();
    }
}