#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"
#include "app_list.h"
/// 版本号
#define VERSION 20260430
/// 消息类型
#define Board_to_Android 0x00 // 主板->安卓
#define Android_to_Board 0x01 // 安卓->主板
#define Board_to_Ctrl 0x02    // 主板->控台
#define Ctrl_to_Board 0x03    // 控台->主板

#define ResendTrigger_Time 1000 // 重新发送触发时间ms
#define MesgDeal_Time 250       // 消息处理时间
#define Max_Resend_Times 3      // 最大重新发送次数


/// 接收消息结构体(新球盘)
typedef struct
{
    uint8_t Head;
    uint8_t ResendID;
    uint8_t ID;
    uint8_t Code1;
    uint8_t Code2;
    uint8_t Data1;
    uint8_t Data2;
    uint8_t Data3;
    uint8_t Data4;
    uint8_t ACKbyte;
    uint8_t ExpandCode;
    uint8_t CRC16_H;
    uint8_t CRC16_L;
    uint8_t Tail;
} Mesg_TypeDef;

///
uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode);
uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List);

void Resend_Task(void);
void MesgDeal_Task(void);
///
void CommInit(void);
void CommTask(void);

#endif
