#include "port_device.h"

/* 获取系统时间，单位为毫秒 */
static inline uint32_t Get_SysTime(void)
{
    return HAL_GetTick();
}

/*
 *--------------------- 舵机 ---------------------*
 */

/*
 * @brief 设置舵机角度
 * @param servo 舵机结构体指针
 * @param angle 目标角度
 */
static void Device_Servo_SetAngle(void *servo, uint16_t angle)
{
    servo_t *Servo = (servo_t *)servo;
    Servo->angle = angle;
    Servo_SetAngle(Servo->htim, Servo->channel, Servo->angle);
}

/*
 * @brief 获取舵机角度
 * @param servo 舵机结构体指针
 * @return 舵机角度
 */
static uint16_t Device_Servo_GetAngle(void *servo)
{
    servo_t *Servo = (servo_t *)servo;
    return Servo->angle;
}

/*
 * @brief 增加舵机角度
 * @param servo 舵机结构体指针
 * @param angle 增加的角度
 * @note 该函数会先检查增加后的角度是否超过最大值，避免unsigned类型下溢问题
 */
static void Device_Servo_IncreaseAngle(void *servo, uint16_t angle)
{
    servo_t *Servo = (servo_t *)servo;
    /* 先判断边界，避免超出和unsigned下溢问题 */
    if (Servo->angle + angle < Servo->max_angle)
    {
        Servo->angle += angle;
    }
    else
    {
        Servo->angle = Servo->max_angle;
    }
    /* 传入正确的指针（servo_t *），避免传入 &Servo（servo_t **）的错误 */
    Device_Servo_SetAngle(Servo, Servo->angle);
}

/*
 * @brief 减少舵机角度
 * @param servo 舵机结构体指针
 * @param angle 减少的角度
 * @note 该函数会先检查减少后的角度是否小于最小值，避免unsigned类型下溢问题
 */
static void Device_Servo_DecreaseAngle(void *servo, uint16_t angle)
{
    servo_t *Servo = (servo_t *)servo;
    /* 先判断边界，避免unsigned类型下溢 */
    if (Servo->angle > Servo->min_angle + angle)
    {
        Servo->angle -= angle;
    }
    else
    {
        Servo->angle = Servo->min_angle;
    }
    Device_Servo_SetAngle(Servo, Servo->angle);
}

/*
 * @brief 初始化舵机
 * @param servo 舵机结构体指针
 * @param htim 舵机定时器句柄
 * @param channel 舵机定时器通道
 * @param min_angle 舵机最小角度
 * @param max_angle 舵机最大角度
 * @param default_angle 舵机默认角度
 */
void Device_Servo_Init(servo_t *servo, TIM_HandleTypeDef *htim, uint32_t channel, uint16_t min_angle, uint16_t max_angle, uint16_t default_angle)
{
    servo->htim = htim;
    servo->channel = channel;
    servo->min_angle = min_angle;
    servo->max_angle = max_angle;
    servo->SetAngle = Device_Servo_SetAngle;
    servo->GetAngle = Device_Servo_GetAngle;
    servo->IncreaseAngle = Device_Servo_IncreaseAngle;
    servo->DecreaseAngle = Device_Servo_DecreaseAngle;
    HAL_TIM_PWM_Start(servo->htim, servo->channel);
    servo->SetAngle(servo, default_angle);
}

/*
 *--------------------- 电机 ---------------------*
 */

/*
 * @brief 设置电机速度和方向
 * @param motor 电机结构体指针
 * @param speed 速度值，范围0-100
 * @param direction 方向，0或1
 * @note 该函数会调用底层的 Motor_SetSpeed 函数，并记录运行时间戳
 */
static void Device_Motor_SetSpeed(void *motor, uint16_t speed, uint8_t direction)
{
    motor_t *Motor = (motor_t *)motor;
    Motor->direction = direction;
    Motor_SetSpeed(Motor->htim_p, Motor->channel_p, Motor->htim_n, Motor->channel_n, speed, direction);
    Motor->runtick = Get_SysTime();
}

/*
 * @brief 停止电机
 * @param motor 电机结构体指针
 * @note 该函数会调用底层的 Motor_Stop 函数
 */
static void Device_Motor_Stop(void *motor)
{
    motor_t *Motor = (motor_t *)motor;
    Motor_Stop(Motor->htim_p, Motor->channel_p, Motor->htim_n, Motor->channel_n);
}

/*
 * @brief 断电电机
 * @param motor 电机结构体指针
 * @note 该函数会调用底层的 Motor_LosePower 函数，使电机完全失去动力
 */
static void Device_Motor_LosePower(void *motor)
{
    motor_t *Motor = (motor_t *)motor;
    Motor_LosePower(Motor->htim_p, Motor->channel_p, Motor->htim_n, Motor->channel_n);
}

/*
 * @brief 重置电机运行时间
 * @param motor 电机结构体指针
 * @note 该函数会调用底层的 Motor_ResetRuntime 函数，并记录运行时间戳
 */
static void Device_Motor_ResetRuntime(void *motor)
{
    motor_t *Motor = (motor_t *)motor;
    Motor->runtick = Get_SysTime();
}

/*
 * @brief 获取电机运行时间
 * @param motor 电机结构体指针
 * @return 电机运行时间，单位为毫秒
 */
static uint32_t Device_Motor_GetRuntime(void *motor)
{
    motor_t *Motor = (motor_t *)motor;
    return Get_SysTime() - (Motor->runtick);
}

/*
 * @brief 初始化电机
 * @param motor 电机结构体指针
 * @param htim_p 电机正转定时器句柄
 * @param channel_p 电机正转定时器通道
 * @param htim_n 电机反转定时器句柄
 * @param channel_n 电机反转定时器通道
 */
void Device_Motor_Init(motor_t *motor, TIM_HandleTypeDef *htim_p, uint32_t channel_p, TIM_HandleTypeDef *htim_n, uint32_t channel_n)
{
    motor->htim_p = htim_p;
    motor->channel_p = channel_p;
    motor->htim_n = htim_n;
    motor->channel_n = channel_n;
    motor->state = DEVICE_STATE_IDLE;
    motor->direction = 1;
    motor->speed = 0;
    motor->runtick = 0;
    motor->SetSpeed = Device_Motor_SetSpeed;
    motor->Stop = Device_Motor_Stop;
    motor->LosePower = Device_Motor_LosePower;
    motor->ResetRuntime = Device_Motor_ResetRuntime;
    motor->GetRuntime = Device_Motor_GetRuntime;
    HAL_TIM_PWM_Start(motor->htim_p, motor->channel_p);
    HAL_TIM_PWM_Start(motor->htim_n, motor->channel_n);
    Device_Motor_LosePower(motor);
}

/*
 *--------------------- MOS管开关器件 ---------------------*
 */

/*
 * @brief 打开开关
 * @param Switch 开关结构体指针
 * @note 该函数会调用底层的 Switch_SetState 函数，并记录运行时间戳
 */
static void Device_Switch_On(void *Switch)
{
    switch_t *S = (switch_t *)Switch;
    //Switch_SetState(S->gpio, S->pin, S->level);
    HAL_GPIO_WritePin(S->gpio, S->pin, S->level);
    S->runtick = Get_SysTime();
}

/*
 * @brief 关闭开关
 * @param Switch 开关结构体指针
 * @note 该函数会调用底层的 Switch_SetState 函数
 */
static void Device_Switch_Off(void *Switch)
{
    switch_t *S = (switch_t *)Switch;
    //Switch_SetState(S->gpio, S->pin, !(S->level));
    HAL_GPIO_WritePin(S->gpio, S->pin, !(S->level));
}

/*
 * @brief 重置开关运行时间
 * @param Switch 开关结构体指针
 * @note 该函数会调用底层的 Switch_ResetRuntime 函数，并记录运行时间戳
 */
static void Device_Switch_ResetRuntime(void *Switch)
{
    switch_t *S = (switch_t *)Switch;
    S->runtick = Get_SysTime();
}

/*
 * @brief 获取开关运行时间
 * @param Switch 开关结构体指针
 * @return 开关运行时间，单位为毫秒
 */
static uint32_t Device_Switch_GetRuntime(void *Switch)
{
    switch_t *S = (switch_t *)Switch;
    return Get_SysTime() - (S->runtick);
}

/*
 * @brief 初始化开关
 * @param Switch 开关结构体指针
 * @param gpio 开关连接的GPIO端口
 * @param pin 开关连接的GPIO引脚
 * @param level 开关的有效电平，GPIO_PIN_SET或GPIO_PIN_RESET
 * @note 该函数会调用底层的 Switch_SetState 函数将开关初始化为关闭状态
 */
void Device_Switch_Init(switch_t *Switch, GPIO_TypeDef *gpio, uint16_t pin, GPIO_PinState level)
{
    Switch->gpio = gpio;
    Switch->pin = pin;
    Switch->level = level;
    Switch->state = DEVICE_STATE_IDLE;
    Switch->runtick = 0;
    Switch->on = Device_Switch_On;
    Switch->off = Device_Switch_Off;
    Switch->ResetRuntime = Device_Switch_ResetRuntime;
    Switch->GetRuntime = Device_Switch_GetRuntime;
    Switch_SetState(Switch->gpio, Switch->pin, !(Switch->level));
}