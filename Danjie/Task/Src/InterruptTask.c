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
// #define COIN_INPUT_DEBOUNCE_TIME 50U

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

// static uint32_t CoinInputLastTick = 0;
// static uint8_t CoinInputTriggered = 0;

/*
 * TIM7当前配置为1MHz计数：
 * 1个计数约等于1us。
 *
 * 现有投币光眼实测低电平约37.8ms。
 * 原吐珠代码初始有效下限相当于5ms。
 *
 * 暂定统一有效范围5ms～60ms。
 * 该范围需要在钢珠、瓷珠、投币、投珠四个位置实测后确认。
 */
#define OPTICAL_SENSOR_PULSE_MIN_US       5000U
#define OPTICAL_SENSOR_PULSE_MAX_US      60000U
#define OPTICAL_SENSOR_PULSE_MAX_MS         60U
#define OPTICAL_SENSOR_RETRIGGER_TIME_MS    50U

typedef struct
{
    uint16_t StartCount;
    uint32_t StartTick;
    uint32_t LastValidTick;

    uint8_t LowStarted;
    uint8_t HasValidPulse;
} OpticalSensorFilter_t;

static OpticalSensorFilter_t HoolleInputFilter;
static OpticalSensorFilter_t CoinInputFilter;
static OpticalSensorFilter_t HoolleOutput1Filter;
static OpticalSensorFilter_t HoolleOutput2Filter;

static volatile uint16_t HoolleInputPendingCount = 0;
static volatile uint16_t CoinInputPendingCount = 0;

/*
 * 对计数器进行饱和加1，避免长时间未处理导致uint16_t回绕。
 */
static void PendingCount_Increase(volatile uint16_t *Count)
{
    if (*Count < 0xFFFFU)
    {
        (*Count)++;
    }
}

/*
 * 原子读取并清除计数器。
 * 防止主循环读取期间，光眼中断正好增加计数。
 */
static uint16_t PendingCount_Take(volatile uint16_t *Count)
{
    uint16_t Value;
    uint32_t Primask = __get_PRIMASK();

    __disable_irq();

    Value = *Count;
    *Count = 0;

    if (Primask == 0U)
    {
        __enable_irq();
    }

    return Value;
}

/*
 * 统一光眼脉冲检测：
 *
 * 下降沿：
 *   记录低电平开始时间。
 *
 * 上升沿：
 *   计算低电平持续时间；
 *   只有处于有效范围内才返回1。
 *
 * CheckRetrigger：
 *   投币、投珠使用50ms重复触发过滤；
 *   吐珠检测不使用，避免连续珠子间隔较短时漏计。
 */
static uint8_t OpticalSensor_CheckPulse(
    OpticalSensorFilter_t *Filter,
    GPIO_TypeDef *GPIOx,
    uint16_t GPIO_Pin,
    uint8_t CheckRetrigger)
{
    uint16_t CurrentCount;
    uint16_t Delta;
    uint32_t CurrentTick;
    GPIO_PinState PinState;

    CurrentCount = (uint16_t)__HAL_TIM_GetCounter(&htim7);
    CurrentTick = HAL_GetTick();
    PinState = HAL_GPIO_ReadPin(GPIOx, GPIO_Pin);

    /* 光眼被遮挡，信号进入低电平 */
    if (PinState == GPIO_PIN_RESET)
    {
        Filter->StartCount = CurrentCount;
        Filter->StartTick = CurrentTick;
        Filter->LowStarted = 1U;
        return 0U;
    }

    /*
     * 上电时输入可能直接处于高电平。
     * 如果之前没有记录下降沿，不认为是完整脉冲。
     */
    if (Filter->LowStarted == 0U)
    {
        return 0U;
    }

    Filter->LowStarted = 0U;

    /*
     * uint16_t减法自动按65536取模，
     * 可正确处理一次TIM7回绕。
     */
    Delta = (uint16_t)(CurrentCount - Filter->StartCount);

    /*
     * 同时检查HAL毫秒时间，避免低电平超过65.536ms后
     * 因TIM7回绕而被误判为短脉冲。
     */
    if ((CurrentTick - Filter->StartTick) > OPTICAL_SENSOR_PULSE_MAX_MS)
    {
        return 0U;
    }

    if (Delta < OPTICAL_SENSOR_PULSE_MIN_US ||
        Delta > OPTICAL_SENSOR_PULSE_MAX_US)
    {
        return 0U;
    }

    if (CheckRetrigger != 0U)
    {
        if (Filter->HasValidPulse != 0U &&
            (CurrentTick - Filter->LastValidTick) <
                OPTICAL_SENSOR_RETRIGGER_TIME_MS)
        {
            return 0U;
        }

        Filter->LastValidTick = CurrentTick;
        Filter->HasValidPulse = 1U;
    }

    return 1U;
}

/*
 * 玩家投珠光眼。
 * 使用计数器而不是事件位，连续两个有效脉冲不会合并。
 */
static void HoolleInput_IRQ(void)
{
    if (OpticalSensor_CheckPulse(
            &HoolleInputFilter,
            HoolleInput_GPIO_Port,
            HoolleInput_Pin,
            1U) != 0U)
    {
        PendingCount_Increase(&HoolleInputPendingCount);
    }
}

/*
 * 投币光眼。
 * 一个硬币对应一个完整低电平脉冲。
 */
static void CoinInput_IRQ(void)
{
    if (OpticalSensor_CheckPulse(
            &CoinInputFilter,
            CoinInput_GPIO_Port,
            CoinInput_Pin,
            1U) != 0U)
    {
        PendingCount_Increase(&CoinInputPendingCount);
    }
}

/*
 * 1号吐珠电机：钢珠。
 */
static void Hoolle_1_Output_IRQ(void)
{
    if (OpticalSensor_CheckPulse(
            &HoolleOutput1Filter,
            HoolleOutput_1_GPIO_Port,
            HoolleOutput_1_Pin,
            0U) == 0U)
    {
        return;
    }

    /*
     * 只允许正常正转吐珠状态扣减数量。
     * 反转松珠阶段状态为DEVICE_STATE_TIMEOUT，不扣数量。
     */
    if (Motor_Hoolle1.Motor.state != DEVICE_STATE_BUSY ||
        Motor_Hoolle1.Motor.direction != HoolleMotor_Dir ||
        Motor_Hoolle1.Hoolle_num == 0U)
    {
        return;
    }

    /*
     * 只有完整且有效的光眼脉冲才刷新3秒超时。
     * 无效干扰下降沿不再延长电机运行时间。
     */
    Motor_Hoolle1.Motor.ResetRuntime(&Motor_Hoolle1.Motor);

    Motor_Hoolle1.Hoolle_num--;
    Motor_Hoolle1.RetryCount = 0;

    Motor_Hoolle1.RemainingReportValue =
        Motor_Hoolle1.Hoolle_num;
    Motor_Hoolle1.RemainingReportPending = 1U;

    if (Motor_Hoolle1.Hoolle_num == 0U)
    {
        Motor_Hoolle1.Motor.state = DEVICE_STATE_STOP;
    }
}

/*
 * 2号吐珠电机：瓷珠。
 */
static void Hoolle_2_Output_IRQ(void)
{
    if (OpticalSensor_CheckPulse(
            &HoolleOutput2Filter,
            HoolleOutput_2_GPIO_Port,
            HoolleOutput_2_Pin,
            0U) == 0U)
    {
        return;
    }

    if (Motor_Hoolle2.Motor.state != DEVICE_STATE_BUSY ||
        Motor_Hoolle2.Motor.direction != HoolleMotor_Dir ||
        Motor_Hoolle2.Hoolle_num == 0U)
    {
        return;
    }

    Motor_Hoolle2.Motor.ResetRuntime(&Motor_Hoolle2.Motor);

    Motor_Hoolle2.Hoolle_num--;
    Motor_Hoolle2.RetryCount = 0;

    Motor_Hoolle2.RemainingReportValue =
        Motor_Hoolle2.Hoolle_num;
    Motor_Hoolle2.RemainingReportPending = 1U;

    if (Motor_Hoolle2.Hoolle_num == 0U)
    {
        Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
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

uint16_t HoolleInput_TakePendingCount(void)
{
    return PendingCount_Take(&HoolleInputPendingCount);
}

uint16_t CoinInput_TakePendingCount(void)
{
    return PendingCount_Take(&CoinInputPendingCount);
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
