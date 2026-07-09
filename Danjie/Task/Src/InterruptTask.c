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
 * TIM7每个计数为0.1ms。
 * 20个计数对应2ms。
 */
#define HOOLLE_LOW_MIN_COUNT 20U
volatile uint16_t HoolleInputPendingCount = 0U;
/*
 * 进珠光眼初始滤波参数。
 *
 * LOW_MIN：
 *   低电平不足2ms视为毛刺。
 *
 * LOW_MAX：
 *   遮挡超过1000ms视为卡珠或异常，不计数。
 *
 * HIGH_MIN：
 *   前一次上升沿后，高电平至少稳定5ms，
 *   才允许识别下一次下降沿。
 *
 * 这些值尚未通过示波器实测，属于初始测试值。
 */
#define HOOLLE_INPUT_LOW_MIN_MS   1U
#define HOOLLE_INPUT_LOW_MAX_MS   1000U
#define HOOLLE_INPUT_HIGH_MIN_MS  3U

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

#define HOOLLE_INPUT_FILTER_MS  3U

// volatile uint16_t HoolleInputPendingCount = 0U;

static uint8_t HoolleInputRawState = 1U;
static uint8_t HoolleInputStableState = 1U;
static uint8_t HoolleInputSameCount = 0U;

void HoolleInput_FilterInit(void)
{
    uint8_t CurrentState;

    CurrentState =
        HAL_GPIO_ReadPin(
            HoolleInput_GPIO_Port,
            HoolleInput_Pin) == GPIO_PIN_SET
            ? 1U
            : 0U;

    HoolleInputRawState = CurrentState;
    HoolleInputStableState = CurrentState;
    HoolleInputSameCount = 0U;
}

void HoolleInput_Scan1ms(void)
{
    uint8_t CurrentState;

    CurrentState =
        HAL_GPIO_ReadPin(
            HoolleInput_GPIO_Port,
            HoolleInput_Pin) == GPIO_PIN_SET
            ? 1U
            : 0U;

    /*
     * 检查当前采样值是否连续保持不变。
     */
    if (CurrentState == HoolleInputRawState)
    {
        if (HoolleInputSameCount < HOOLLE_INPUT_FILTER_MS)
        {
            HoolleInputSameCount++;
        }
    }
    else
    {
        HoolleInputRawState = CurrentState;
        HoolleInputSameCount = 1U;
    }

    /*
     * 电平连续稳定达到设定时间后，
     * 才更新稳定状态。
     */
    if (HoolleInputSameCount >= HOOLLE_INPUT_FILTER_MS &&
        HoolleInputStableState != HoolleInputRawState)
    {
        HoolleInputStableState = HoolleInputRawState;

        /*
         * 光眼低电平表示珠子进入。
         * 只有稳定状态从高变低时才计一颗。
         *
         * 稳定状态必须重新恢复为高电平后，
         * 才能识别下一颗，因此同一颗抖动不会连续计数。
         */
        if (HoolleInputStableState == 0U)
        {
            if (HoolleInputPendingCount < 0xFFFFU)
            {
                HoolleInputPendingCount++;
            }
        }
    }
}

static void HoolleInput_IRQ(void)
{
    // /*
    //  * WaitingRise：
    //  * 0：等待珠子进入，即等待下降沿。
    //  * 1：已经检测到下降沿，等待对应上升沿。
    //  */
    // static uint8_t WaitingRise = 0U;

    // /*
    //  * HasRise：
    //  * 是否已经记录过上升沿。
    //  * 第一次上电时没有上升沿记录，不检查高电平间隔。
    //  */
    // static uint8_t HasRise = 0U;

    // /*
    //  * LowStartTick：
    //  * 当前低电平开始时间。
    //  *
    //  * LastRiseTick：
    //  * 最近一次上升沿时间，用于检查高电平稳定时间。
    //  */
    // static uint32_t LowStartTick = 0U;
    // static uint32_t LastRiseTick = 0U;

    // uint32_t CurrentTick;
    // uint32_t LowTime;
    // uint32_t HighTime;
    // GPIO_PinState PinState;

    // CurrentTick = HAL_GetTick();

    // PinState = HAL_GPIO_ReadPin(
    //     HoolleInput_GPIO_Port,
    //     HoolleInput_Pin);

    // /*
    //  * 当前是低电平：
    //  * 说明发生下降沿，珠子开始遮挡光眼。
    //  */
    // if (PinState == GPIO_PIN_RESET)
    // {
    //     /*
    //      * 已经记录过下降沿，正在等待上升沿。
    //      * 不重复记录同一次遮挡。
    //      */
    //     if (WaitingRise != 0U)
    //     {
    //         return;
    //     }

    //     /*
    //      * 前一次上升沿后，高电平时间必须足够长。
    //      * 高电平太短说明很可能是同一颗珠抖动。
    //      */
    //     if (HasRise != 0U)
    //     {
    //         HighTime = CurrentTick - LastRiseTick;

    //         if (HighTime < HOOLLE_INPUT_HIGH_MIN_MS)
    //         {
    //             return;
    //         }
    //     }

    //     LowStartTick = CurrentTick;
    //     WaitingRise = 1U;
    //     return;
    // }

    // /*
    //  * 当前是高电平：
    //  * 说明发生上升沿，珠子离开光眼。
    //  *
    //  * 无论这次脉冲是否有效，都记录上升沿时间，
    //  * 防止毛刺结束后立即再次被当成新珠子。
    //  */
    // LastRiseTick = CurrentTick;
    // HasRise = 1U;

    // /*
    //  * 没有对应的下降沿，忽略这个上升沿。
    //  */
    // if (WaitingRise == 0U)
    // {
    //     return;
    // }

    // LowTime = CurrentTick - LowStartTick;
    // WaitingRise = 0U;

    // /*
    //  * 低电平时间太短：电气毛刺、反光或边缘抖动。
    //  *
    //  * 低电平时间太长：卡珠、持续遮挡或传感器异常。
    //  */
    // if (LowTime < HOOLLE_INPUT_LOW_MIN_MS ||
    //     LowTime > HOOLLE_INPUT_LOW_MAX_MS)
    // {
    //     return;
    // }

    // /*
    //  * 只有完成一次有效的：
    //  * 下降沿 → 有效低电平 → 上升沿
    //  * 才确认一颗珠。
    //  */
    // if (HoolleInputPendingCount < 0xFFFFU)
    // {
    //     HoolleInputPendingCount++;
    // }
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
    /*
     * 最后一颗珠子下降沿到来时已经提前停止电机。
     * 等上升沿确认脉宽有效后，再正式扣除数量。
     */
    static uint8_t LastBallStopped = 0U;

    uint32_t LowCount;

    if (HAL_GPIO_ReadPin(
            HoolleOutput_1_GPIO_Port,
            HoolleOutput_1_Pin) == GPIO_PIN_RESET)
    {
        /*
         * 下降沿：钢珠开始遮挡光眼。
         */
        __HAL_TIM_SetCounter(&htim7, 0);
        Motor_Hoolle1.Motor.ResetRuntime(
            &Motor_Hoolle1.Motor);

        /*
         * 当前只剩最后一颗，并且电机正在正常吐珠：
         * 在最后一颗进入光眼时立即停止电机。
         *
         * 此处只停止硬件输出，逻辑状态暂时保持BUSY，
         * 避免CtrlTask提前把Hoolle_num清零。
         */
        if (Motor_Hoolle1.Hoolle_num == 1U &&
            Motor_Hoolle1.Motor.state == DEVICE_STATE_BUSY)
        {
            Motor_Hoolle1.Motor.Stop(
                &Motor_Hoolle1.Motor);

            LastBallStopped = 1U;
        }
        else
        {
            LastBallStopped = 0U;
        }

        return;
    }

    /*
     * 上升沿：钢珠离开光眼。
     */
    LowCount = __HAL_TIM_GetCounter(&htim7);

    if (LowCount > HOOLLE_LOW_MIN_COUNT)
    {
        /*
         * 低电平超过20ms，确认是一颗有效钢珠。
         */
        EventGroupSetBits(
            &Mesg_event,
            MesgEvent_RemainingHoolle);

        if (Motor_Hoolle1.Hoolle_num > 0U)
        {
            Motor_Hoolle1.Hoolle_num--;
            Motor_Hoolle1.RetryCount = 0;

            /*
             * 最后一颗已经在下降沿停止过硬件输出。
             * 此处设置STOP，让CtrlTask完成IDLE、
             * Hoolle_num和ClearMode的状态收尾。
             */
            if (Motor_Hoolle1.Hoolle_num == 0U &&
                Motor_Hoolle1.Motor.state != DEVICE_STATE_IDLE)
            {
                Motor_Hoolle1.Motor.state =
                    DEVICE_STATE_STOP;
            }
        }

        LastBallStopped = 0U;
        return;
    }

    /*
     * 低电平不足20ms，视为毛刺。
     *
     * 如果刚才因为“最后一颗下降沿”提前停止过电机，
     * 则重新进入START状态，让CtrlTask恢复吐珠。
     */
    if (LastBallStopped != 0U &&
        Motor_Hoolle1.Hoolle_num == 1U &&
        Motor_Hoolle1.Motor.state == DEVICE_STATE_BUSY)
    {
        Motor_Hoolle1.Motor.state =
            DEVICE_STATE_START;
    }

    LastBallStopped = 0U;
}

static void Hoolle_2_Output_IRQ(void)
{
    /*
     * 最后一颗珠子下降沿到来时已经提前停止电机。
     */
    static uint8_t LastBallStopped = 0U;

    uint32_t LowCount;

    if (HAL_GPIO_ReadPin(
            HoolleOutput_2_GPIO_Port,
            HoolleOutput_2_Pin) == GPIO_PIN_RESET)
    {
        /*
         * 下降沿：钢珠开始遮挡光眼。
         */
        __HAL_TIM_SetCounter(&htim7, 0);
        Motor_Hoolle2.Motor.ResetRuntime(
            &Motor_Hoolle2.Motor);

        /*
         * 只剩最后一颗时，在下降沿立即停止电机。
         */
        if (Motor_Hoolle2.Hoolle_num == 1U &&
            Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY)
        {
            Motor_Hoolle2.Motor.Stop(
                &Motor_Hoolle2.Motor);

            LastBallStopped = 1U;
        }
        else
        {
            LastBallStopped = 0U;
        }

        return;
    }

    /*
     * 上升沿：检查低电平持续时间。
     */
    LowCount = __HAL_TIM_GetCounter(&htim7);

    if (LowCount > 1)
    {
        if (Motor_Hoolle2.Hoolle_num > 0U)
        {
            Motor_Hoolle2.Hoolle_num--;
            Motor_Hoolle2.RetryCount = 0;

            if (Motor_Hoolle2.Hoolle_num == 0U &&
                Motor_Hoolle2.Motor.state != DEVICE_STATE_IDLE)
            {
                Motor_Hoolle2.Motor.state =
                    DEVICE_STATE_STOP;
            }
        }

        LastBallStopped = 0U;
        return;
    }

    /*
     * 低电平不足20ms，恢复被提前停止的电机。
     */
    if (LastBallStopped != 0U &&
        Motor_Hoolle2.Hoolle_num == 1U &&
        Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY)
    {
        Motor_Hoolle2.Motor.state =
            DEVICE_STATE_START;
    }

    LastBallStopped = 0U;
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
