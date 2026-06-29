#include "CtrlTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "KeyTask.h"
#include "CommTask.h"
#include "LightTask.h"
#include "tim.h"
#include "port_device.h"
#include "port_event.h"
#include "string.h"

Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
Motor_Card Card;
servo_t Servo1, Servo2, Servo3;
Switch_Valve Lock_Valve;

extern Tx_HandleTypeDef Tx1;
extern Scene_t Scene;
extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;
extern Light_t Light;

static inline uint32_t Get_SysTime(void)
{
    return HAL_GetTick();
}

static void Ctrl_HoolleMotor(
    Motor_Hoolle *Motor,
    uint16_t speed,
    uint8_t dir,
    uint32_t timeout,
    uint32_t reverse_time,
    uint8_t retry_times)
{
    /* 启动吐珠电机正转 */
    if (Motor->Motor.state == DEVICE_STATE_START)
    {
        Motor->Motor.SetSpeed(&Motor->Motor, speed, dir);
        Motor->Motor.state = DEVICE_STATE_BUSY;
    }

    /* 正常停止或停止所有设备 */
    if (Motor->Motor.state == DEVICE_STATE_STOP)
    {
        Motor->Motor.Stop(&Motor->Motor);

        Motor->Motor.state = DEVICE_STATE_IDLE;
        Motor->Hoolle_num = 0;
        Motor->RetryCount = 0;
        Motor->ClearMode = 0;
    }

    /* 当前处于堵转反转阶段 */
    if (Motor->Motor.state == DEVICE_STATE_TIMEOUT)
    {
        if (Motor->Motor.GetRuntime(&Motor->Motor) > reverse_time)
        {
            if (Motor->RetryCount < retry_times)
            {
                Motor->RetryCount++;
                Motor->Motor.state = DEVICE_STATE_START;
            }
            else
            {
                Motor->Motor.Stop(&Motor->Motor);
                Motor->Motor.state = DEVICE_STATE_IDLE;
                Motor->RetryCount = 0;

                /*
                 * 清珠模式下，最终超时表示料仓已经没有珠子。
                 * 必须清除0xFFFF清珠计数。
                 *
                 * 普通吐珠超时不清零，保留未吐完数量上报安卓。
                 */
                if (Motor->ClearMode != 0U)
                {
                    Motor->Hoolle_num = 0;
                    Motor->ClearMode = 0;
                }

                Motor->TimeoutReportValue =
                    Motor->Hoolle_num;
                Motor->TimeoutReportPending = 1U;
            }
        }
    }

    /* 吐珠电机暂停 */
    if (Motor->Motor.state == DEVICE_STATE_PAUSE)
    {
        Motor->Motor.LosePower(&Motor->Motor);
        Motor->Motor.ResetRuntime(&Motor->Motor);
    }

    /*
     * 正常正转超过timeout仍未检测到有效出珠脉冲：
     * 先断电1ms，再反转。
     */
    if (Motor->Motor.GetRuntime(&Motor->Motor) > timeout &&
        Motor->Motor.state != DEVICE_STATE_IDLE)
    {
        Motor->Motor.state = DEVICE_STATE_TIMEOUT;

        Motor->Motor.LosePower(&Motor->Motor);
        HAL_Delay(1);

        Motor->Motor.SetSpeed(
            &Motor->Motor,
            speed,
            !dir);
    }
}

static void Ctrl_CardMotor(Motor_Card *Card, uint32_t timeout, void (*Timeout_callbcak)(void))
{

    /*==============卡片机控制===============*/
    // 开机吐卡
    if (Card->Switch.state == DEVICE_STATE_START)
    {
        Card->Switch.on(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_BUSY;
    }
    // 停止吐卡
    if (Card->Switch.state == DEVICE_STATE_STOP)
    {
        Card->Switch.off(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_IDLE;
        Card->Card_num = 0;
    }
    // 吐卡超时
    if (Card->Switch.state == DEVICE_STATE_TIMEOUT)
    {
        Card->Switch.off(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_IDLE;
        // 吐卡超时反应
        if (Timeout_callbcak != NULL)
            Timeout_callbcak();
    }
    // 吐卡超时判断
    if (Card->Switch.GetRuntime(&Card->Switch) > CardMotorTimeout_time && Card->Switch.state != DEVICE_STATE_IDLE)
    {
        Card->Switch.state = DEVICE_STATE_TIMEOUT;
    }
}

/*==============电磁阀控制===============*/
static void Ctrl_Valve(Switch_Valve *Valve, uint32_t timeout, void (*Timeout_callbcak)(void))
{
    // 电磁阀启动
    if (Valve->Switch.state == DEVICE_STATE_START)
    {
        Valve->Switch.on(&Valve->Switch);
        Valve->Switch.state = DEVICE_STATE_BUSY;
    }
    // 电磁阀停止
    if (Valve->Switch.state == DEVICE_STATE_STOP)
    {
        Valve->Switch.off(&Valve->Switch);
        Valve->Switch.state = DEVICE_STATE_IDLE;
    }
    // 电磁阀超时
    if (Valve->Switch.state == DEVICE_STATE_TIMEOUT)
    {
        Valve->Switch.state = DEVICE_STATE_IDLE;
        Valve->Switch.off(&Valve->Switch);
        if (Timeout_callbcak != NULL)
            Timeout_callbcak();
    }
    // 电磁阀超时判断
    if (Valve->Switch.GetRuntime(&Valve->Switch) > timeout && Valve->Switch.state != DEVICE_STATE_IDLE)
    {
        Valve->Switch.state = DEVICE_STATE_TIMEOUT;
    }
}

// static void HoolleMotorTimeout_callback(void)
// {
//     EventGroupSetBits(&Mesg_event, MesgEvent_HoolleOutputTimeout);
// }

static void CardMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputTimeout);
}

static void ValveTimeout_callback(void)
{
}

void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num)
{
    uint32_t Primask = __get_PRIMASK();

    __disable_irq();

    /*
     * 如果清珠过程中收到新的普通吐珠数量，
     * 先退出清珠模式并清除0xFFFF哨兵值。
     */
    if (num != 0U && Motor->ClearMode != 0U)
    {
        Motor->Hoolle_num = 0;
        Motor->ClearMode = 0;
    }

    Motor->Hoolle_num += num;

    if (Motor->Hoolle_num != 0U)
    {
        Motor->Motor.state = DEVICE_STATE_START;
        Motor->Motor.runtick = Get_SysTime();
        Motor->RetryCount = 0;
    }

    if (Primask == 0U)
    {
        __enable_irq();
    }
}

void Hoolle_Clear(Motor_Hoolle *Motor)
{
    uint32_t Primask = __get_PRIMASK();

    __disable_irq();

    /*
     * 清珠使用独立模式标志。
     * 仍保留现有0xFFFF连续运行方式，
     * 但最终空仓超时后会在状态机中清零。
     */
    Motor->Hoolle_num = 0xFFFFU;
    Motor->ClearMode = 1U;
    Motor->RetryCount = 0;

    Motor->Motor.state = DEVICE_STATE_START;
    Motor->Motor.runtick = Get_SysTime();

    if (Primask == 0U)
    {
        __enable_irq();
    }
}

void Card_Output(Motor_Card *Switch, uint16_t num)
{
    Switch->Card_num += num;
    if (Switch->Card_num != 0)
    {
        Switch->Switch.state = DEVICE_STATE_START;
        Switch->Switch.runtick = Get_SysTime();
    }
}

/// 设备初始化
void Device_Init(void)
{
    Device_Motor_Init(&Motor_Hoolle1.Motor, &htim1, TIM_CHANNEL_1, &htim1, TIM_CHANNEL_2);
    Device_Motor_Init(&Motor_Hoolle2.Motor, &htim1, TIM_CHANNEL_3, &htim1, TIM_CHANNEL_4);
    Device_Switch_Init(&Card.Switch, CardOutput_GPIO_Port, CardOutput_Pin, GPIO_PIN_SET);
    Device_Switch_Init(&Lock_Valve.Switch, GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    Device_Servo_Init(&Servo1, &htim2, TIM_CHANNEL_3, 45, 135, 90);
    Device_Servo_Init(&Servo2, &htim2, TIM_CHANNEL_1, 0, 180, 5);
    Device_Servo_Init(&Servo3, &htim2, TIM_CHANNEL_2, 0, 180, 180);
    HAL_TIM_Base_Start(&htim7);

    // Motor_Hoolle1.Hoolle_num = 0;
    // Motor_Hoolle2.Hoolle_num = 0;
    // Motor_Hoolle1.RetryCount = 0;
    // Motor_Hoolle2.RetryCount = 0;
    Card.Card_num = 0;
    Servo2.SetAngle(&Servo2, 5);
    Servo3.SetAngle(&Servo3, 180);

    /* 1号为钢珠，安卓补充位0x01 */
    Motor_Hoolle1.ExpandCode = HOOLLE_TYPE_STEEL;

    /* 2号为瓷珠，安卓补充位0x00 */
    Motor_Hoolle2.ExpandCode = HOOLLE_TYPE_CERAMIC;

    Motor_Hoolle1.Hoolle_num = 0;
    Motor_Hoolle2.Hoolle_num = 0;

    Motor_Hoolle1.RetryCount = 0;
    Motor_Hoolle2.RetryCount = 0;

    Motor_Hoolle1.ClearMode = 0;
    Motor_Hoolle2.ClearMode = 0;

    Motor_Hoolle1.RemainingReportPending = 0;
    Motor_Hoolle2.RemainingReportPending = 0;

    Motor_Hoolle1.TimeoutReportPending = 0;
    Motor_Hoolle2.TimeoutReportPending = 0;

    Motor_Hoolle1.RemainingReportValue = 0;
    Motor_Hoolle2.RemainingReportValue = 0;

    Motor_Hoolle1.TimeoutReportValue = 0;
    Motor_Hoolle2.TimeoutReportValue = 0;
}


void Servo_AutoRun(servo_t *Servo, uint32_t time)
{
    static uint32_t Time = 0;
    static uint8_t dir = 0;
    static uint32_t time_now;
    time_now = HAL_GetTick();
    if (time_now - Time > time)
    {
        if (dir == 0)
        {
            if (Servo->angle < Servo->max_angle)
            {
                Servo->angle += 2;
                Servo->SetAngle(Servo, Servo->angle);
            }
            else
                dir = 1;
        }
        else
        {
            if (Servo->angle > Servo->min_angle)
            {
                Servo->angle -= 2;
                Servo->SetAngle(Servo, Servo->angle);
            }
            else
                dir = 0;
        }
        Time = time_now;
    }
}
void CtrlTask(void)
{
    /*==============吐珠电机控制===============*/
    Ctrl_HoolleMotor(&Motor_Hoolle1, HoolleMotor_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times);
    Ctrl_HoolleMotor(&Motor_Hoolle2, HoolleMotor_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times);
    /*==============卡片机控制===============*/
    Ctrl_CardMotor(&Card, CardMotorTimeout_time, CardMotorTimeout_callback);
    /*==============电磁阀控制===============*/
    Ctrl_Valve(&Lock_Valve, ValveTimeout_time, NULL);
    //Servo_AutoRun(&Servo1,2);
}