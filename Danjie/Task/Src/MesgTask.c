#include "MainTask.h"
#include "MesgTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "port_communicate.h"
#include "port_event.h"
#include "InterruptTask.h"

extern Tx_HandleTypeDef Tx1;
extern Rx_HandleTypeDef Rx1;
extern Tx_HandleTypeDef Tx3;
extern Rx_HandleTypeDef Rx3;

extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern Motor_Card Card;
extern Switch_Valve Lock_Valve;
extern ListHandle_t ResendList, DealList;

Event_Handle_t Mesg_event;
// 发送给NFC开锁消息
static uint8_t NFCUnlock_mesg[11] = {0xaa, 0x0f, 0x01, 0x01, 0x01, 0x01, 0x5c, 0x77, 0x08, 0x7f, 0x55};

/*
 * 原子读取每台吐珠电机的上报信息。
 * Value是事件发生时保存的数值，不直接读取当前Hoolle_num。
 */
static uint8_t Hoolle_TakeReport(
    volatile uint8_t *Pending,
    volatile uint16_t *Value,
    uint16_t *ReportValue)
{
    uint8_t HasReport;
    uint32_t Primask = __get_PRIMASK();

    __disable_irq();

    HasReport = *Pending;

    if (HasReport != 0U)
    {
        *ReportValue = *Value;
        *Pending = 0U;
    }

    if (Primask == 0U)
    {
        __enable_irq();
    }

    return HasReport;
}

void Mesg_Task(void)
{
    uint16_t PendingCount;
    uint16_t ReportValue;
    // 按键进入设置
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_ButtonEnterSetting))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, 0x03, 0x01);
        EventGroupClearBits(&Mesg_event, MesgEvent_ButtonEnterSetting);
    }
    // 开锁
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_Unlock) == true)
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_AlreadyUnlock, 0x00, 0x00);
        EventGroupClearBits(&Mesg_event, MesgEvent_Unlock);
    }
    // // 投珠
    // if (EventGroupCheckBits(&Mesg_event, MesgEvent_HoolleInput) == true)
    // {
    //     Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_HoolleInput, 0x00, 0x00, &ResendList);
    //     EventGroupClearBits(&Mesg_event, MesgEvent_HoolleInput);
    // }
    // // 投币
    // if (EventGroupCheckBits(&Mesg_event, MesgEvent_CoinInput) == true)
    // {
    //     Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_CoinInput, 0x00, 0x00, &ResendList);
    //     EventGroupClearBits(&Mesg_event, MesgEvent_CoinInput);
    // }
    // // 吐珠超时
    // if (EventGroupCheckBits(&Mesg_event, MesgEvent_HoolleOutputTimeout))
    // {
    //     Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_HoolleOutputTimeOut, (uint32_t)Motor_Hoolle2.Hoolle_num, 0x00, &ResendList);
    //     EventGroupClearBits(&Mesg_event, MesgEvent_HoolleOutputTimeout);
    // }
    // 吐卡超时
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputTimeout))
    {
        Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_CardOutputTimeOut, (uint32_t)Card.Card_num, 0x00, &ResendList);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputTimeout);
    }
    // // 剩余珠子数
    // if (EventGroupCheckBits(&Mesg_event, MesgEvent_RemainingHoolle) == true)
    // {
    //     Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_RemainingHoolle, (uint32_t)Motor_Hoolle2.Hoolle_num, 0x00);
    //     EventGroupClearBits(&Mesg_event, MesgEvent_RemainingHoolle);
    // }
    // 版本请求
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_VersionRequest) == true)
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_VersionRequest, (uint32_t)VERSION, 0x00);
        EventGroupClearBits(&Mesg_event, MesgEvent_VersionRequest);
    }
    /*
    * 玩家投珠。
    * 每个有效光眼脉冲发送一条消息，不会因事件位合并而少报。
    */
    PendingCount = HoolleInput_TakePendingCount();

    while (PendingCount > 0U)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_HoolleInput,
            0x00,
            0x00,
            &ResendList);

        PendingCount--;
    }

    /*
    * 投币。
    * 一个硬币对应一个有效光眼脉冲。
    */
    PendingCount = CoinInput_TakePendingCount();

    while (PendingCount > 0U)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_CoinInput,
            0x00,
            0x00,
            &ResendList);

        PendingCount--;
    }

    /*
    * 瓷珠吐珠超时。
    * 补充位发送0x00。
    */
    if (Hoolle_TakeReport(
            &Motor_Hoolle2.TimeoutReportPending,
            &Motor_Hoolle2.TimeoutReportValue,
            &ReportValue) != 0U)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_HoolleOutputTimeOut,
            (uint32_t)ReportValue,
            Motor_Hoolle2.ExpandCode,
            &ResendList);
    }

    /*
    * 钢珠吐珠超时。
    * 补充位发送0x01。
    */
    if (Hoolle_TakeReport(
            &Motor_Hoolle1.TimeoutReportPending,
            &Motor_Hoolle1.TimeoutReportValue,
            &ReportValue) != 0U)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_HoolleOutputTimeOut,
            (uint32_t)ReportValue,
            Motor_Hoolle1.ExpandCode,
            &ResendList);
    }

    /*
    * 瓷珠剩余数量。
    */
    if (Hoolle_TakeReport(
            &Motor_Hoolle2.RemainingReportPending,
            &Motor_Hoolle2.RemainingReportValue,
            &ReportValue) != 0U)
    {
        Comm_SendMesg_FillData(
            &Tx1,
            Board_to_Android,
            t_RemainingHoolle,
            (uint32_t)ReportValue,
            Motor_Hoolle2.ExpandCode);
    }

    /*
    * 钢珠剩余数量。
    */
    if (Hoolle_TakeReport(
            &Motor_Hoolle1.RemainingReportPending,
            &Motor_Hoolle1.RemainingReportValue,
            &ReportValue) != 0U)
    {
        Comm_SendMesg_FillData(
            &Tx1,
            Board_to_Android,
            t_RemainingHoolle,
            (uint32_t)ReportValue,
            Motor_Hoolle1.ExpandCode);
    }
    
    Resend_Task();
    MesgDeal_Task();
}