#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"
#include "app_list.h"
/// 版本号
#define VERSION 20260508
/// 消息类型
#define Board_to_Android 0x00 // 主板->安卓
#define Android_to_Board 0x01 // 安卓->主板
#define Board_to_Ctrl 0x02    // 主板->控制器
#define Ctrl_to_Board 0x03    // 控制器->主板

#define ResendTrigger_Time 1000 // 重新发送触发时间ms
#define MesgDeal_Time 250       // 消息处理时间
#define Max_Resend_Times 3      // 最大重新发送次数

/// 球盘发送给安卓的消息功能码
#define t_VersionRequest 0x00      // 版本请求应答
#define t_HoolleInput 0x01         // 投入弹珠
#define t_CoinInput 0x02           // 投入硬币
#define t_Button 0x03              // 拍拍按键
#define t_SettingButton 0x04       // 设置按键
#define t_RemainingHoolle 0x05     // 剩余珠子数
#define t_WinOrLoss 0x06           // 游戏结果
#define t_HoolleOutputTimeOut 0x07 // 珠子输出超时
#define t_CardOutputTimeOut 0x08   // 卡片输出超时
#define t_NFCEnterSetting 0x09     // NFC进入后台
#define t_UnlockCardStatus 0x0A    // 解锁卡片状态
#define t_BackStageCardStatus 0x0B // 后台卡片状态
#define t_CardID 0x0C              // 绑定卡片ID
#define t_AlreadyUnlock 0x0D       // 已开锁
#define t_LightEye 0x0E            // 光眼
#define t_Encoder 0x0F             // 编码器
#define t_ChannelRequest 0x10      // 击中通道位置反馈
#define t_ClearRemainMesg 0x11     // 清除剩余珠子消息
#define t_IntoHigherStage 0x12     // 进入高级后台

/// 球盘接收到安卓的消息功能码
#define r_GetVersion 0x00             // 获取版本信息
#define r_HoolleOutput 0x01           // 珠子输出
#define r_CardOutput 0x02             // 卡片输出
#define r_ValveTrigger 0x03           // 触发电磁阀
#define r_BoardLightness 0x04         // 球盘亮度
#define r_LightBoardLightness 0x05    // 灯板亮度
#define r_LightBeltLightness 0x06     // 灯带亮度
#define r_SceneChange 0x07            // 场景编号
#define r_WinChannel 0x08             // 中奖通道
#define r_LittleGameResult 0x09       // 小游戏输赢结果
#define r_ButtonLight 0x0A            // 按键灯
#define r_OutputAllHoolle 0x0B        // 清珠
#define r_OutputRemainingItem 0x0C    // 吐出剩余物品
#define r_ResumeDefultSetting 0x0D    // 恢复默认设置
#define r_SaveSetting 0x0E            // 保存设置
#define r_HoleValveTrigger 0x0F       // 洞内电磁阀触发
#define r_Unlock 0x10                 // 已开锁
#define r_ResumeBoundCard 0x11        // 重新绑卡
#define r_WirelessMasterSetting 0x12  // 无线通信主从设置
#define r_WirelessChannelSetting 0x13 // 无线通信信道设置
#define r_ServoControl 0x14           // 舵机控制
#define r_LightControl 0x15           // 灯控制
#define r_DigitalTubeData 0x16        // 数字数据
#define r_CtrlLightness 0x18          // 控台亮度
#define r_ServoReset 0x20              // 舵机归零
#define r_StopAllDevice 0xFF          // 停止所有输出
#define r_SystemReset 0xF0            // 系统复位

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

//
void Resend_Task(void);
void MesgDeal_Task(void);

///
void CommInit(void);
void CommTask(void);

#endif
