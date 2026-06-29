#include "LightTask.h"
#include "port_light.h"
#include "MainTask.h"
#include "FlashTask.h"
#include "port_event.h"
#include "app_list.h"
#include "tim.h"

RGB_t Light1_RGBbuf[Light1_RGBbufSize];
uint16_t Light1_CRRbuf[Light1_CRRbufSize];
RGB_t Light2_RGBbuf[Light2_RGBbufSize];
uint16_t Light2_CRRbuf[Light2_CRRbufSize];

Semaphore_t Semaphore1;
Semaphore_t Semaphore2;

uint8_t LightBoard_Lightness = 10;
uint8_t LightBelt_Lightness = 5;

extern Scene_t Scene;

Light_t Light1;
Light_t Light2;
Light_Handle_t Light_B1, Light_B2, Light_B3, Light_B4, Light_B5, Light_B6, Light_B7, Light_B8, Light_Y1, Light_Y2, Light_Y3, Light_Y4, Light_Y5, Light_Y6, Light_Y7, Light_Y8, Light_P3, Light_P1_2;
Light_Handle_t *Light_BLUE[8] = {&Light_B1, &Light_B2, &Light_B3, &Light_B4, &Light_B5, &Light_B6, &Light_B7, &Light_B8};
Light_Handle_t *Light_YELLOW[8] = {&Light_Y1, &Light_Y2, &Light_Y3, &Light_Y4, &Light_Y5, &Light_Y6, &Light_Y7, &Light_Y8};

static BreathLight_t J2 = {&htim5, TIM_CHANNEL_1, 999, 0, GPIOA, GPIO_PIN_0, 0, 0, true};
static BreathLight_t J3 = {&htim5, TIM_CHANNEL_2, 999, 0, GPIOB, GPIO_PIN_1, 0, 0, true};
static BreathLight_t J6 = {&htim10, TIM_CHANNEL_1, 999, 0, GPIOB, GPIO_PIN_8, 0, 0, true};
static BreathLight_t J7 = {&htim11, TIM_CHANNEL_1, 999, 0, GPIOB, GPIO_PIN_9, 0, 0, true};

BreathLight_t *BreathList[] = {&J2, &J3, &J6, &J7};
uint8_t LightCache[8] = {0};

extern Event_Handle_t Event;
extern Event_Handle_t Mesg_event;
extern Setting_TypeDef Setting;
void LightTask_Init(void)
{
    RGB_Init(&Light1, &htim3, TIM_CHANNEL_1, Light1_RGBbufSize, Light1_RGBbuf, Light1_CRRbuf, &Semaphore1, RGB);
    RGB_Init(&Light2, &htim3, TIM_CHANNEL_2, Light2_RGBbufSize, Light2_RGBbuf, Light2_CRRbuf, &Semaphore2, RGB);
    BreathLight_Init(&J2, &htim5, TIM_CHANNEL_1, GPIOA, GPIO_PIN_0);
    BreathLight_Init(&J3, &htim5, TIM_CHANNEL_2, GPIOB, GPIO_PIN_1);
    BreathLight_Init(&J6, &htim10, TIM_CHANNEL_1, GPIOB, GPIO_PIN_8);
    BreathLight_Init(&J7, &htim11, TIM_CHANNEL_1, GPIOB, GPIO_PIN_9);
    RegisterLight(ColorLight, &Light1);
    RegisterLight(ColorLight, &Light2);
    RegisterLight(BreathLight, &J2);
    RegisterLight(BreathLight, &J3);
    RegisterLight(BreathLight, &J6);
    RegisterLight(BreathLight, &J7);
    LightDerive_Init(&Light_B1, &Light1, 0, 7, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B2, &Light1, 8, 15, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B3, &Light1, 16, 23, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B4, &Light1, 24, 31, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_P3, &Light1, 32, 35, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B5, &Light1, 36, 43, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B6, &Light1, 44, 49, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B7, &Light1, 50, 55, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B8, &Light1, 56, 61, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_P1_2, &Light1, 62, 65, (uint8_t *)&Setting.Board_Lightness);

    LightDerive_Init(&Light_Y1, &Light2, 0, 7, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y2, &Light2, 8, 15, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y3, &Light2, 16, 21, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y4, &Light2, 22, 27, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y5, &Light2, 28, 35, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y6, &Light2, 36, 42, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y7, &Light2, 43, 50, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y8, &Light2, 51, 58, (uint8_t *)&Setting.Board_Lightness);
    RGB_SetAllColor(&Light1, NONE, 0, 0);
    RGB_SetAllColor(&Light2, NONE, 0, 0);
    RGB_Flush(&Light1);
    RGB_Flush(&Light2);
}

/// 洞口灯光闪烁任务
static void HoleLightBlink_Task(void)
{
    static uint32_t time = 0;
    static uint8_t Lightness = 10;
    if (EventGroupCheckBits(&Mesg_event, Event_DoorOpen) == true)
    {
        LightEffect_Unblock_SetColor(&Light1, 32, 35, PINK, LightBoard_Lightness, 255, 0);
        if (HAL_GetTick() - time > 250)
        {
            RGB_SetMoreColor(&Light1, 62, 65, PINK, Lightness, 255);
            if (Lightness == 10)
                Lightness = 0;
            else
                Lightness = 10;
            RGB_LocalRefresh(&Light1, 62, 65);
            time = HAL_GetTick();
        }
        // LightEffect_Unblock_Blink(&Light1, 62, 65, PINK, LightBoard_Lightness, 255, 250);
    }
    else
    {
        RGB_SetMoreColor(&Light1, 32, 35, NONE, LightBoard_Lightness, 0);
        RGB_SetMoreColor(&Light1, 62, 65, NONE, LightBoard_Lightness, 0);
        RGB_Flush(&Light1);
    }
}

/// 洞口灯光流水任务
static void LightBufferFlush(Light_Handle_t *light, RGB_t color)
{
    if (light->state == LIGHT_STATE_FLOW)
    {
        light->index++;
        if (light->index > light->end)
        {
            light->index = light->start;
            RGB_SetMoreColor(light->light, light->start, light->end, NONE, *(light->Lightness), 255);
        }
        else
            RGB_SetMoreColor(light->light, light->start, light->index, color, *(light->Lightness), 255);
    }
    else if (light->state == LIGHT_STATE_OFF)
        RGB_SetMoreColor(light->light, light->start, light->end, NONE, *(light->Lightness), 0);
    else if (light->state == LIGHT_STATE_ON)
        RGB_SetMoreColor(light->light, light->start, light->end, color, *(light->Lightness), 255);
}

/// 洞口灯光刷新任务
static void LightFlush_Task(void)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        LightBufferFlush(Light_BLUE[i], SKYBLUE);
        LightBufferFlush(Light_YELLOW[i], GREEN);
    }
    RGB_Flush(&Light1);
    RGB_Flush(&Light2);
}

/// 灯光任务
void LightTask(void)
{
    // if (EventGroupCheckBits(&Mesg_event, Event_DoorOpen) == true)
    //     LightEffect_Unblock_Blink(&Light1, 36, 37, PINK, LightBoard_Lightness, 255, 250);
    // else
    //     LightEffect_Unblock_SetColor(&Light1, 36, 37, NONE, LightBoard_Lightness, 0, 0);
    if (Scene == SettingScene)
    {
        LightEffect_Unblock_SetColor(&Light1, 0, Light1_RGBbufSize, WHITE, LightBoard_Lightness, 255, true);
        BreathLight_SetLightKeep(&J2, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J3, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J6, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J7, 0, Setting.LightBelt_Lightness, 255);
        LightEffect_Unblock_SetColor(&Light2, 0, Light2_RGBbufSize, WHITE, LightBoard_Lightness, 255, true);
    }
    if (Scene == IdleScene)
    {
        BreathLight_SetLightKeep(&J2, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J3, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J6, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J7, 0, Setting.LightBelt_Lightness, 255);
        LightEffect_Unblock_Flow(&Light1, 0, Light1_RGBbufSize, NONE, SKYBLUE, LightBoard_Lightness, 255, 50, 1000, 0);
        LightEffect_Unblock_Flow(&Light2, 0, Light2_RGBbufSize, NONE, GREEN, LightBoard_Lightness, 255, 50, 1000, 0);
    }
    if (Scene == PlayingScene)
    {
        static uint32_t time = 0;
        if (EventGroupCheckBits(&Event, Event_SceneChange) == true)
        {
            RGB_SetAllColor(&Light1, NONE, 0, 0);
            RGB_SetAllColor(&Light2, NONE, 0, 0);
            RGB_Flush(&Light1);
            RGB_Flush(&Light2);
            //     // LightResume();
            EventGroupClearBits(&Event, Event_SceneChange);
        }
        if (HAL_GetTick() - time > 100)
        {
            LightFlush_Task();
            time = HAL_GetTick();
        }
        BreathLight_SetLightKeep(&J2, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J3, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J6, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J7, 0, Setting.LightBelt_Lightness, 255);
        // LightEffect_Unblock_SetColor(&Light1, 0, Light1_RGBbufSize, NONE, LightBoard_Lightness, 255, 0);
        // LightEffect_Unblock_SetColor(&Light2, 0, Light2_RGBbufSize, NONE, LightBoard_Lightness, 255, 0);
        // LightEffect_Unblock_Blink(&Light1, 36, 37, PINK, LightBoard_Lightness, 255, 250);
    }
    if (Scene == LittleGame_1)
    {
        if (EventGroupCheckBits(&Event, Event_SceneChange))
        {
            EventGroupClearBits(&Event, Event_SceneChange);
            RGB_CleanAll(&Light1);
            RGB_CleanAll(&Light2);
            RGB_Flush(&Light1);
            RGB_Flush(&Light2);
        }
    }
    if (Scene == LittleGame_2)
    {
        if (EventGroupCheckBits(&Event, Event_SceneChange))
        {
            EventGroupClearBits(&Event, Event_SceneChange);
            RGB_CleanAll(&Light1);
            RGB_CleanAll(&Light2);
            RGB_Flush(&Light1);
            RGB_Flush(&Light2);
        }
    }
    if (Scene != IdleScene)
        HoleLightBlink_Task();
}
