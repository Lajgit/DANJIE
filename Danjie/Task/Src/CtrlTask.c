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

#define DoorServoTimeout_time 200

Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
Motor_Card Card;
servo_t Servo1, Servo2, Servo3;
Switch_Valve Lock_Valve;

static uint32_t DoorServoRuntick = 0;
static uint8_t DoorServoRunning = 0;

extern Tx_HandleTypeDef Tx1;
extern Scene_t Scene;
extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;
extern Light_t Light;
static inline uint32_t Get_SysTime(void)
{
    return HAL_GetTick();
}

static void DoorServo_SetAngle(void *servo, uint16_t angle)
{
    servo_t *Servo = (servo_t *)servo;

    Servo->angle = angle;
    Servo_SetAngle(Servo->htim, Servo->channel, Servo->angle);

    HAL_TIM_PWM_Start(Servo2.htim, Servo2.channel);
    HAL_TIM_PWM_Start(Servo3.htim, Servo3.channel);

    DoorServoRuntick = Get_SysTime();
    DoorServoRunning = 1;
}

static void Ctrl_DoorServo(void)
{
    if (DoorServoRunning != 0 && Get_SysTime() - DoorServoRuntick >= DoorServoTimeout_time)
    {
        HAL_TIM_PWM_Stop(Servo2.htim, Servo2.channel);
        HAL_TIM_PWM_Stop(Servo3.htim, Servo3.channel);
        DoorServoRunning = 0;
    }
}

static void Ctrl_HoolleMotor(Motor_Hoolle *Motor, uint16_t speed, uint8_t dir, uint32_t timeout, uint32_t reverse_time, uint8_t retry_times, void (*Timeout_callbcak)(void))
{
    // 开机吐珠电机
    if (Motor->Motor.state == DEVICE_STATE_START)
    {
        Motor->Motor.SetSpeed(&Motor->Motor, speed, dir);
        Motor->Motor.state = DEVICE_STATE_BUSY;
        // Motor->RetryCount = 0;
    }
    // 停止吐珠电机
    if (Motor->Motor.state == DEVICE_STATE_STOP)
    {
        Motor->Motor.Stop(&Motor->Motor);
        Motor->Motor.state = DEVICE_STATE_IDLE;
        Motor->Hoolle_num = 0;
        Motor->ClearMode = 0;
    }
    // 吐珠电机超时
    if (Motor->Motor.state == DEVICE_STATE_TIMEOUT)
    {
        // 反转时间到
        if (Motor->Motor.GetRuntime(&Motor->Motor) > HoolleMotorReverse_Time)
        {
            // 翻转次数不够，重新吐出
            if (Motor->RetryCount < retry_times)
            {
                Motor->Motor.state = DEVICE_STATE_START;
                Motor->RetryCount++;
            }
            else
            {
                Motor->Motor.state = DEVICE_STATE_IDLE;
                Motor->Motor.Stop(&Motor->Motor);

                 /* 清珠空仓结束，清除0xFFFF剩余计数 */
                if (Motor->ClearMode != 0)
                {
                    Motor->Hoolle_num = 0;
                    Motor->ClearMode = 0;
                }

                // 超时停转后的反应
                if (Timeout_callbcak != NULL)
                    Timeout_callbcak();
            }
        }
    }
    // 吐珠电机暂停
    if (Motor->Motor.state == DEVICE_STATE_PAUSE)
    {
        Motor->Motor.LosePower(&Motor->Motor);
        Motor->Motor.ResetRuntime(&Motor->Motor);
    }
    // 吐珠电机超时
    if (Motor->Motor.GetRuntime(&Motor->Motor) > timeout && Motor->Motor.state != DEVICE_STATE_IDLE)
    {
        Motor->Motor.state = DEVICE_STATE_TIMEOUT;
        Motor->Motor.LosePower(&Motor->Motor);
        HAL_Delay(1);
        // 反转
        Motor->Motor.SetSpeed(&Motor->Motor, speed, !dir);
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

static void HoolleMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_HoolleOutputTimeout);
}

static void CardMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputTimeout);
}

static void ValveTimeout_callback(void)
{
}

void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num)
{
    Motor->Hoolle_num += num;
    if (Motor->Hoolle_num != 0)
    {
        Motor->Motor.state = DEVICE_STATE_START;
        Motor->Motor.runtick = Get_SysTime();
        Motor->RetryCount = 0;
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
    Servo2.SetAngle = DoorServo_SetAngle;
    Servo3.SetAngle = DoorServo_SetAngle;
    HAL_TIM_Base_Start(&htim7);

    Motor_Hoolle1.Hoolle_num = 0;
    Motor_Hoolle2.Hoolle_num = 0;
    Motor_Hoolle1.RetryCount = 0;
    Motor_Hoolle2.RetryCount = 0;
    Motor_Hoolle1.ClearMode = 0;
    Motor_Hoolle2.ClearMode = 0;
    Card.Card_num = 0;
    Servo2.SetAngle(&Servo2, 5);
    Servo3.SetAngle(&Servo3, 180);
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
    Ctrl_HoolleMotor(&Motor_Hoolle1, HoolleMotor_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times, HoolleMotorTimeout_callback);
    Ctrl_HoolleMotor(&Motor_Hoolle2, HoolleMotor_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times, HoolleMotorTimeout_callback);
    /*==============卡片机控制===============*/
    Ctrl_CardMotor(&Card, CardMotorTimeout_time, CardMotorTimeout_callback);
    /*==============电磁阀控制===============*/
    Ctrl_Valve(&Lock_Valve, ValveTimeout_time, NULL);
    Ctrl_DoorServo();
    //Servo_AutoRun(&Servo1,2);
}
