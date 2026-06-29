#include "DigitalTubeTask.h"

#define DigitalTube_Refresh_Time 25

static uint8_t DigitalBuffer[4] = {0x82};
DigitalTube_t DigitalTube;
SoftwareSPI_HandleTypeDef tube_spi;

void SoftSPI_Init(void)
{
    SoftwareSPI_InitTypeDef Init;

    Init.SDA_Port = Tube_SDA_GPIO_Port;
    Init.SDA_Pin = Tube_SDA_Pin;
    Init.CLK_Port = Tube_CLK_GPIO_Port;
    Init.CLK_Pin = Tube_CLK_Pin;
    Init.CS_Port = NULL;
    Init.CS_Pin = NULL;
    Init.CLK_CPOL = 0;
    Init.CS_Level = GPIO_PIN_SET;
    Init.DelayTick = 36;
    SoftwareSPI_Init(&tube_spi, Init);
}

void DigitalTubeTask_Init(void)
{
    SoftSPI_Init();
    DigitalTube_Init_t Init;

    Init.hspi = &tube_spi;
    Init.Buffer = DigitalBuffer;
    Init.bit_num = sizeof(DigitalBuffer);
    Init.LE_GPIO = NULL;
    Init.LE_Pin = NULL;
    Init.CODE_CA = DIGITAL3BIT_CODE_CA;
    DigitalTube_Init(&DigitalTube, Init);
    DigitalTube.Set_Num(&DigitalTube, 0, 0, 4);
    DigitalTube.Refresh(&DigitalTube);
}

void DigitalTube_Task(void)
{
    static uint32_t time = 0;
    if (HAL_GetTick() - time > DigitalTube_Refresh_Time)
    {
        DigitalTube.Refresh(&DigitalTube);
        time = HAL_GetTick();
    }
}