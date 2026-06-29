#include "MainTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "FlashTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "LightTask.h"
#include "MesgTask.h"
#include "port_event.h"
#include "DigitalTubeTask.h"
#include "iwdg.h"

#define SYSLIGHT_BLINK_TIME 500

Scene_t Scene = IdleScene;
Event_Handle_t Event;

extern Tx_HandleTypeDef Tx3;
void System_Reset(void)
{
    __disable_irq();
    HAL_NVIC_SystemReset();
}

static void SystemLight_Task(void)
{
    static uint32_t time = 0;
    if (HAL_GetTick() - time > SYSLIGHT_BLINK_TIME)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        time = HAL_GetTick();
    }
}

void Main_Init(void)
{
    FlashTask_Init();
    CommInit();
    Device_Init();
    KeyAll_Init();
    LightTask_Init();
    Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, 0x04, Setting.Ctrl_Lightness, 0x00); // 控台亮度
    Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, 0x03, 0x00, 0x00);                   // 控台灯效
    // DigitalTubeTask_Init();
}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    FlashTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    LightTask();
    HAL_IWDG_Refresh(&hiwdg);
    CtrlTask();
    HAL_IWDG_Refresh(&hiwdg);
    Mesg_Task();
    HAL_IWDG_Refresh(&hiwdg);
    // DigitalTube_Task();
    SystemLight_Task();
}