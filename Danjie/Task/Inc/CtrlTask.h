#ifndef __CTRLTASK_H__
#define __CTRLTASK_H__

#include "port_device.h"

#define HoolleMotorTimeout_time 3000 // 吐珠电机超时时间
#define CardMotorTimeout_time 3000   // 卡片机超时时间
#define HoolleMotorReverse_Time 300  // 吐珠电机反转时间
#define HoolleMotorRetry_Times 3     // 吐珠电机重试次数
#define ValveTimeout_time 800        // 电磁阀超时时间
#define LockValveTimeout_time 2000   // 电子锁超时时间
#define Valve_TriggerCount 1

#define HoolleMotor_Speed 85 // 吐珠电机速度100%
#define HoolleMotor_Dir 0     // 吐珠电机方向

typedef struct
{
    motor_t Motor;
    volatile uint16_t Hoolle_num;
    volatile uint8_t RetryCount;
    volatile uint8_t ClearMode;//增加一个清珠标志
} Motor_Hoolle;

typedef struct
{
    switch_t Switch;
    volatile uint16_t Card_num;
    volatile uint8_t RetryCount;
} Motor_Card;

typedef struct
{
    switch_t Switch;
    volatile uint8_t TriggerCount;
} Switch_Valve;

void Device_Init(void);
void CtrlTask(void);
void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num);
void Card_Output(Motor_Card *Switch, uint16_t num);
void Valve_Start(Switch_Valve *Valve, uint8_t TriggerCount);
void Device_Stop(void);

#endif
