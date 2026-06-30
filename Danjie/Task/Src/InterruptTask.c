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
#define HOOLLE_OUTPUT_MIN_LOW_TIME 20U

/*
 * 这是两个“计数下降沿”之间的最小间隔，
 * 不是光眼低电平脉宽下限。
 *
 * 用于过滤同一颗珠子边沿抖动产生的重复下降沿。
 */
#define HOOLLE_OUTPUT_MIN_INTERVAL_MS 100U

//双边沿状态机
#define HOOLLE_LOW_MIN_MS   20U
#define HOOLLE_LOW_MAX_MS   500U
#define HOOLLE_HIGH_MIN_MS  40U

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

// static void Hoolle_1_Output_IRQ(void)
// {
//     /*
//      * Armed = 1：
//      *   当前允许识别下一次下降沿。
//      *
//      * Armed = 0：
//      *   当前这颗珠子已经计数，必须等光眼恢复高电平后
//      *   才能识别下一颗。
//      */
//     static uint8_t Armed = 1U;
//     static uint8_t HasCounted = 0U;
//     static uint32_t LastCountTick = 0U;

//     uint32_t CurrentTick;
//     GPIO_PinState PinState;

//     PinState = HAL_GPIO_ReadPin(
//         HoolleOutput_1_GPIO_Port,
//         HoolleOutput_1_Pin);

//     /*
//      * 空闲为高电平。
//      * 上升沿表示当前珠子已经离开光眼，
//      * 重新允许识别下一颗。
//      */
//     if (PinState == GPIO_PIN_SET)
//     {
//         Armed = 1U;
//         return;
//     }

//     /*
//      * 当前为低电平，表示出现下降沿，珠子开始遮挡。
//      *
//      * 如果本次低电平已经计过数，不重复计数。
//      */
//     if (Armed == 0U)
//     {
//         return;
//     }

//     /*
//      * 立即锁定。
//      * 必须等后续上升沿恢复高电平后才能再次计数。
//      */
//     Armed = 0U;

//     /*
//      * 只允许正常正转吐珠阶段计数。
//      *
//      * DEVICE_STATE_TIMEOUT为反转阶段，
//      * 反转过程中经过光眼不能扣数量。
//      */
//     if (Motor_Hoolle1.Motor.state != DEVICE_STATE_BUSY ||
//         Motor_Hoolle1.Hoolle_num == 0U)
//     {
//         return;
//     }

//     CurrentTick = HAL_GetTick();

//     /*
//      * 防止同一颗珠子的高低抖动形成第二次下降沿。
//      *
//      * 注意：
//      * 这里判断的是两次有效计数的间隔，
//      * 与低电平持续37ms、250ms无关。
//      */
//     if (HasCounted != 0U &&
//         (CurrentTick - LastCountTick) <
//             HOOLLE_OUTPUT_MIN_INTERVAL_MS)
//     {
//         return;
//     }

//     HasCounted = 1U;
//     LastCountTick = CurrentTick;

//     /*
//      * 确认有珠子进入光眼后，刷新3秒超时时间。
//      */
//     Motor_Hoolle1.Motor.ResetRuntime(
//         &Motor_Hoolle1.Motor);

//     Motor_Hoolle1.Hoolle_num--;
//     Motor_Hoolle1.RetryCount = 0;

//     EventGroupSetBits(
//         &Mesg_event,
//         MesgEvent_RemainingHoolle);

//     /*
//      * 第N颗珠子刚进入光眼时立即发出停止请求，
//      * 不再等待它完全离开光眼。
//      */
//     if (Motor_Hoolle1.Hoolle_num == 0U)
//     {
//         Motor_Hoolle1.Motor.state =
//             DEVICE_STATE_STOP;
//     }
// }

static void Hoolle_1_Output_IRQ(void)
{
    /*
     * WaitingRise = 0：
     *   等待下一次有效下降沿。
     *
     * WaitingRise = 1：
     *   已检测到下降沿，正在等待对应上升沿。
     */
    static uint8_t WaitingRise = 0U;

    /*
     * 上一次有效珠子的上升沿时间。
     * 用于判断两颗珠子之间的高电平是否足够稳定。
     */
    static uint8_t HasValidRise = 0U;
    static uint32_t LowStartTick = 0U;
    static uint32_t LastValidRiseTick = 0U;

    uint32_t CurrentTick;
    uint32_t LowTime;
    uint32_t HighTime;
    GPIO_PinState PinState;

    CurrentTick = HAL_GetTick();

    PinState = HAL_GPIO_ReadPin(
        HoolleOutput_1_GPIO_Port,
        HoolleOutput_1_Pin);

    /*
     * 非正常正转状态下不计数。
     *
     * 同时清除未完成的下降沿状态，防止：
     * 正转时下降 → 反转时上升
     * 被误认为有效钢珠。
     */
    if (Motor_Hoolle1.Motor.state != DEVICE_STATE_BUSY ||
        Motor_Hoolle1.Hoolle_num == 0U)
    {
        WaitingRise = 0U;
        return;
    }

    /*
     * 空闲高电平，钢珠遮挡低电平。
     * 当前为低电平，说明发生下降沿。
     */
    if (PinState == GPIO_PIN_RESET)
    {
        /*
         * 已经记录过下降沿，仍在等待上升沿。
         * 不重复覆盖开始时间。
         */
        if (WaitingRise != 0U)
        {
            return;
        }

        /*
         * 如果前面已经识别过一颗钢珠，
         * 检查本次下降沿距离上一次有效上升沿的高电平时间。
         */
        if (HasValidRise != 0U)
        {
            HighTime = CurrentTick - LastValidRiseTick;

            if (HighTime < HOOLLE_HIGH_MIN_MS)
            {
                /*
                 * 高电平时间太短，视为同一颗珠子造成的抖动，
                 * 不开始新的低脉冲。
                 */
                return;
            }
        }

        LowStartTick = CurrentTick;
        WaitingRise = 1U;
        return;
    }

    /*
     * 当前为高电平，表示发生上升沿。
     * 如果之前没有记录下降沿，这个上升沿无效。
     */
    if (WaitingRise == 0U)
    {
        return;
    }

    LowTime = CurrentTick - LowStartTick;
    WaitingRise = 0U;

    /*
     * 只有完整低电平时间处于有效范围，才认定为一颗钢珠。
     */
    if (LowTime < HOOLLE_LOW_MIN_MS ||
        LowTime > HOOLLE_LOW_MAX_MS)
    {
        return;
    }

    /*
     * 保存有效上升沿。
     * 下一次下降沿需要与它间隔至少HOOLLE_HIGH_MIN_MS。
     */
    LastValidRiseTick = CurrentTick;
    HasValidRise = 1U;

    /*
     * 只有完整有效的钢珠脉冲才刷新3秒吐珠超时。
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
    static uint8_t Armed = 1U;
    static uint8_t HasCounted = 0U;
    static uint32_t LastCountTick = 0U;

    uint32_t CurrentTick;
    GPIO_PinState PinState;

    PinState = HAL_GPIO_ReadPin(
        HoolleOutput_2_GPIO_Port,
        HoolleOutput_2_Pin);

    /*
     * 光眼恢复高电平，允许下一颗珠子计数。
     */
    if (PinState == GPIO_PIN_SET)
    {
        Armed = 1U;
        return;
    }

    /*
     * 当前低电平已经处理过。
     */
    if (Armed == 0U)
    {
        return;
    }

    Armed = 0U;

    /*
     * 只在正常正转状态计数。
     */
    if (Motor_Hoolle2.Motor.state != DEVICE_STATE_BUSY ||
        Motor_Hoolle2.Hoolle_num == 0U)
    {
        return;
    }

    CurrentTick = HAL_GetTick();

    if (HasCounted != 0U &&
        (CurrentTick - LastCountTick) <
            HOOLLE_OUTPUT_MIN_INTERVAL_MS)
    {
        return;
    }

    HasCounted = 1U;
    LastCountTick = CurrentTick;

    Motor_Hoolle2.Motor.ResetRuntime(
        &Motor_Hoolle2.Motor);

    Motor_Hoolle2.Hoolle_num--;
    Motor_Hoolle2.RetryCount = 0;

    /*
     * 保持原工程行为，暂时不恢复瓷珠剩余数量事件。
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
