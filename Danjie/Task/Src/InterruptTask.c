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
/*
 * 实测钢珠光眼遮挡低电平约170～248ms。
 * 低于50ms视为毛刺或抖动。
 * 暂不设置最大值，避免因钢珠速度变化造成漏计。
 */
#define HOOLLE_OUTPUT_MIN_LOW_TIME 50U

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
    static uint32_t LowStartTick = 0U;
    static uint8_t LowStarted = 0U;

    uint32_t CurrentTick;
    uint32_t LowTime;

    /*
     * 空闲为高电平，钢珠遮挡为低电平。
     *
     * 下降沿进入这里时，GPIO已经是低电平：
     * 记录钢珠开始遮挡的时间。
     */
    if (HAL_GPIO_ReadPin(
            HoolleOutput_1_GPIO_Port,
            HoolleOutput_1_Pin) == GPIO_PIN_RESET)
    {
        /*
         * 只记录正常正转过程中的下降沿。
         * 反转或停止期间产生的光眼变化不参与计数。
         */
        if (Motor_Hoolle1.Motor.state == DEVICE_STATE_BUSY &&
            Motor_Hoolle1.Hoolle_num > 0U)
        {
            LowStartTick = HAL_GetTick();
            LowStarted = 1U;
        }
        else
        {
            LowStarted = 0U;
        }

        return;
    }

    /*
     * 当前为高电平，表示钢珠离开光眼。
     * 必须先检测到有效下降沿，才能组成完整遮挡脉冲。
     */
    if (LowStarted == 0U)
    {
        return;
    }

    CurrentTick = HAL_GetTick();
    LowTime = CurrentTick - LowStartTick;
    LowStarted = 0U;

    /*
     * 上升沿到来时仍必须处于正常正转状态。
     * 防止下降沿发生在正转、上升沿发生在反转时误计。
     */
    if (Motor_Hoolle1.Motor.state != DEVICE_STATE_BUSY ||
        Motor_Hoolle1.Hoolle_num == 0U)
    {
        return;
    }

    /*
     * 实测正常低电平约170～248ms。
     * 小于50ms的脉冲按毛刺或抖动处理。
     */
    if (LowTime < HOOLLE_OUTPUT_MIN_LOW_TIME)
    {
        return;
    }

    /*
     * 只有完整有效的钢珠脉冲才刷新吐珠超时时间。
     * 无效下降沿不再延长3秒超时。
     */
    Motor_Hoolle1.Motor.ResetRuntime(
        &Motor_Hoolle1.Motor);

    Motor_Hoolle1.Hoolle_num--;
    Motor_Hoolle1.RetryCount = 0;

    EventGroupSetBits(
        &Mesg_event,
        MesgEvent_RemainingHoolle);

    if (Motor_Hoolle1.Hoolle_num == 0U)
    {
        Motor_Hoolle1.Motor.state =
            DEVICE_STATE_STOP;
    }
}

static void Hoolle_2_Output_IRQ(void)
{
    static uint32_t LowStartTick = 0U;
    static uint8_t LowStarted = 0U;

    uint32_t CurrentTick;
    uint32_t LowTime;

    /*
     * 空闲高电平，珠子遮挡低电平。
     */
    if (HAL_GPIO_ReadPin(
            HoolleOutput_2_GPIO_Port,
            HoolleOutput_2_Pin) == GPIO_PIN_RESET)
    {
        if (Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY &&
            Motor_Hoolle2.Hoolle_num > 0U)
        {
            LowStartTick = HAL_GetTick();
            LowStarted = 1U;
        }
        else
        {
            LowStarted = 0U;
        }

        return;
    }

    if (LowStarted == 0U)
    {
        return;
    }

    CurrentTick = HAL_GetTick();
    LowTime = CurrentTick - LowStartTick;
    LowStarted = 0U;

    if (Motor_Hoolle2.Motor.state != DEVICE_STATE_BUSY ||
        Motor_Hoolle2.Hoolle_num == 0U)
    {
        return;
    }

    if (LowTime < HOOLLE_OUTPUT_MIN_LOW_TIME)
    {
        return;
    }

    Motor_Hoolle2.Motor.ResetRuntime(
        &Motor_Hoolle2.Motor);

    Motor_Hoolle2.Hoolle_num--;
    Motor_Hoolle2.RetryCount = 0;

    /*
     * 保持原工程行为，不恢复瓷珠剩余数量上报。
     */
    // EventGroupSetBits(
    //     &Mesg_event,
    //     MesgEvent_RemainingHoolle);

    if (Motor_Hoolle2.Hoolle_num == 0U)
    {
        Motor_Hoolle2.Motor.state =
            DEVICE_STATE_STOP;
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
