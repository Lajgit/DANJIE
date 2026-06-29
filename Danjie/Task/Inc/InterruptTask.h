#ifndef __INTERRUPTTASK_H__
#define __INTERRUPTTASK_H__

#include "stdint.h"

/*
 * 获取并清除已经检测到的有效脉冲数量。
 * 返回值表示从上次读取到本次读取期间累计的脉冲数。
 */
uint16_t HoolleInput_TakePendingCount(void);
uint16_t CoinInput_TakePendingCount(void);

#endif