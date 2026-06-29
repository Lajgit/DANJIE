#include "app_device.h"

/*
 *--------------------- 舵机 ---------------------*
 */

/*
 * @brief 设置舵机角度
 * @param htim 定时器句柄
 * @param channel 定时器通道
 * @param angle 目标角度，范围为0-180
 */
void Servo_SetAngle(TIM_HandleTypeDef *htim, uint32_t channel, uint16_t angle)
{
    uint16_t crr = (htim->Init.Period + 1) / 20 * (angle + 45) / 90;
    __HAL_TIM_SetCompare(htim, channel, crr);
}

/*
 *--------------------- 电机 ---------------------*
 */

/*
 * @brief 设置电机速度和方向
 * @param htim_p 电机正转定时器句柄
 * @param channel_p 电机正转定时器通道
 * @param htim_n 电机反转定时器句柄
 * @param channel_n 电机反转定时器通道
 * @param speed 速度值，范围0-100
 * @param dir 方向，0或1
 */

void Motor_SetSpeed(TIM_HandleTypeDef *htim_p, uint32_t channel_p, TIM_HandleTypeDef *htim_n, uint32_t channel_n, uint16_t speed, uint8_t dir)
{
    if (dir == 0)
    {
        uint16_t compare = htim_p->Init.Period * speed / 100;
        __HAL_TIM_SetCompare(htim_p, channel_p, compare);
        __HAL_TIM_SetCompare(htim_n, channel_n, 0);
    }
    else
    {
        uint16_t compare = htim_p->Init.Period * speed / 100;
        __HAL_TIM_SetCompare(htim_p, channel_p, 0);
        __HAL_TIM_SetCompare(htim_n, channel_n, compare);
    }
}

/*
 * @brief 停止电机
 * @param htim_p 电机正转定时器句柄
 * @param channel_p 电机正转定时器通道
 * @param htim_n 电机反转定时器句柄
 * @param channel_n 电机反转定时器通道
 */
void Motor_Stop(TIM_HandleTypeDef *htim_p, uint32_t channel_p, TIM_HandleTypeDef *htim_n, uint32_t channel_n)
{
    __HAL_TIM_SetCompare(htim_p, channel_p, htim_p->Init.Period);
    __HAL_TIM_SetCompare(htim_n, channel_n, htim_n->Init.Period);
}

/*
 * @brief 电机失能
 * @param htim_p 电机正转定时器句柄
 * @param channel_p 电机正转定时器通道
 * @param htim_n 电机反转定时器句柄
 * @param channel_n 电机反转定时器通道
 */
void Motor_LosePower(TIM_HandleTypeDef *htim_p, uint32_t channel_p, TIM_HandleTypeDef *htim_n, uint32_t channel_n)
{
    __HAL_TIM_SetCompare(htim_p, channel_p, 0);
    __HAL_TIM_SetCompare(htim_n, channel_n, 0);
}

/*
 *--------------------- MOS管开关器件 ---------------------*
 */

/*
 * @brief 设置开关状态
 * @param GPIOx GPIO端口
 * @param GPIO_Pin GPIO引脚
 * @param state 状态
 */
void Switch_SetState(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState state)
{
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, state);
}