#include "InterruptTask.h"
#include "CtrlTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "CommTask.h"
#include "port_event.h"
#include "tim.h"
#include "stdio.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55
#define COIN_INPUT_DEBOUNCE_TIME 50U

extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;
extern Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
extern Motor_Card Card;
extern Switch_Valve Lock_Valve, Valve;
extern uint8_t LightBoard_Lightness;
extern uint8_t LightBelt_Lightness;
extern uint8_t sm16306s_data[2];
extern Rx_HandleTypeDef Rx1;
extern Rx_HandleTypeDef Rx3;

static uint32_t CoinInputLastTick = 0;
static uint8_t CoinInputTriggered = 0;

static void HoolleInput_IRQ(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_HoolleInput);
}

static void CoinInput_IRQ(void)
{
    uint32_t CurrentTick = HAL_GetTick();

    // 投币器有效低电平脉冲约37.8ms，50ms内的重复上升沿视为抖动
    if (CoinInputTriggered == 0 || CurrentTick - CoinInputLastTick >= COIN_INPUT_DEBOUNCE_TIME)
    {
        CoinInputLastTick = CurrentTick;
        CoinInputTriggered = 1;
        EventGroupSetBits(&Mesg_event, MesgEvent_CoinInput);
    }
}

static void Hoolle_1_Output_IRQ(void)
{
    if (HAL_GPIO_ReadPin(HoolleOutput_1_GPIO_Port, HoolleOutput_1_Pin) == GPIO_PIN_RESET)
    {
        /* 脉冲开始：重置计时器并重置运行时 */
        __HAL_TIM_SetCounter(&htim7, 0);
        Motor_Hoolle1.Motor.ResetRuntime(&Motor_Hoolle1.Motor);
        return;
    }
    else
    {
        if (__HAL_TIM_GetCounter(&htim7) > 100)
        {
            /* 有效脉冲：更新剩余数量并通知任务 */
            EventGroupSetBits(&Mesg_event, MesgEvent_RemainingHoolle);
            if (Motor_Hoolle1.Hoolle_num > 0)
            {
                Motor_Hoolle1.Hoolle_num--;
                Motor_Hoolle1.RetryCount = 0;
                if (Motor_Hoolle1.Hoolle_num == 0 && Motor_Hoolle1.Motor.state != DEVICE_STATE_IDLE)
                {
                    Motor_Hoolle1.Motor.state = DEVICE_STATE_STOP;
                }
            }
        }
    }
}

static void Hoolle_2_Output_IRQ(void)
{
    if (HAL_GPIO_ReadPin(HoolleOutput_2_GPIO_Port, HoolleOutput_2_Pin) == GPIO_PIN_RESET)
    {
        /* 脉冲开始：重置计时器并重置运行时 */
        __HAL_TIM_SetCounter(&htim7, 0);
        Motor_Hoolle2.Motor.ResetRuntime(&Motor_Hoolle2.Motor);
        return;
    }
    else
    {
        if (__HAL_TIM_GetCounter(&htim7) > 100)
        {
            if (Motor_Hoolle2.Hoolle_num > 0)
            {
                Motor_Hoolle2.Hoolle_num--;
                Motor_Hoolle2.RetryCount = 0;
                if (Motor_Hoolle2.Hoolle_num == 0 && Motor_Hoolle2.Motor.state != DEVICE_STATE_IDLE)
                {
                    Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
                }
            }
        }
    }
}

static void CardOutput_IRQ(void)
{
    Card.Switch.ResetRuntime(&Card.Switch);
    if (Card.Card_num > 0)
    {
        Card.Card_num--;
        EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputOnce); // 吐卡一次
        if (Card.Card_num <= 0 && Card.Switch.state != DEVICE_STATE_IDLE)
        {
            Card.Switch.state = DEVICE_STATE_STOP;
            EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputFinish); // 吐卡完成
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
    case HoolleInput_Pin:
        HoolleInput_IRQ();
        break;
    case CoinInput_Pin:
        CoinInput_IRQ();
        break;
    case HoolleOutput_1_Pin:
        Hoolle_1_Output_IRQ();
        break;
    case HoolleOutput_2_Pin:
        Hoolle_2_Output_IRQ();
        break;
    case CardFeedback_Pin:
        CardOutput_IRQ();
        break;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == Rx1.Handle.huart)
    {
        Rx1.Handle.RingBuf.f_WriteByte(&Rx1.Handle.RingBuf, Rx1.Handle.temp_data);
        HAL_UART_Receive_IT(huart, &Rx1.Handle.temp_data, 1);
    }
    if (huart == Rx3.Handle.huart)
    {
        Rx3.Handle.RingBuf.f_WriteByte(&Rx3.Handle.RingBuf, Rx3.Handle.temp_data);
        HAL_UART_Receive_IT(huart, &Rx3.Handle.temp_data, 1);
    }
}
