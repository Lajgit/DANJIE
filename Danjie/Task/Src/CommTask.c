#include "CommTask.h"
#include "MesgTask.h"
#include "CtrlTask.h"
#include "KeyTask.h"
#include "MainTask.h"
#include "LightTask.h"
#include "FlashTask.h"
#include "app_crc.h"
#include "app_list.h"
#include "port_digitaltube.h"
#include "string.h"
#include "usart.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg);

ListHandle_t ResendList, DealList;
static ListNode_t ResendList_buffer[100];
static ListNode_t DealList_buffer[100];

static Mesg_TypeDef MesgTable[256];
static uint8_t rx1_buffer[512];
static uint8_t rx3_buffer[512];

static Mesg_TypeDef Receive1_mesg;
static Mesg_TypeDef Receive3_mesg;

Tx_HandleTypeDef Tx1;
Rx_HandleTypeDef Rx1;
Tx_HandleTypeDef Tx3;
Rx_HandleTypeDef Rx3;

extern Event_Handle_t Event;
extern Event_Handle_t Mesg_event;
extern Motor_Card Card;
extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern servo_t Servo1, Servo2, Servo3;
extern Switch_Valve Lock_Valve;
extern ListHandle_t ResendList, DealList;
extern Scene_t Scene;
extern Light_t Light1;
extern Light_t Light2;
extern BreathLight_t *BreathList[];
extern Setting_TypeDef Setting;
extern Light_Handle_t *Light_BLUE[8];
extern Light_Handle_t *Light_YELLOW[8];

/// 串口1消息验证
static bool USART1_ReceiveMesg_Verify(void *self, void *mesg)
{
    Rx_HandleTypeDef *rx = (Rx_HandleTypeDef *)self;
    Mesg_TypeDef *Rx_mesg = (Mesg_TypeDef *)mesg;
    uint16_t crc16, mesg_crc16;
    crc16 = CRC16_calculate(rx->Queue.Buf, 11);
    mesg_crc16 = Rx_mesg->CRC16_H << 8 | Rx_mesg->CRC16_L;
    if (crc16 == mesg_crc16)
        return true;
    return false;
}
/// 串口1消息处理
static void USART1_Deal(void *Rx_mesg)
{
    uint32_t data;
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;
    if (mesg->Code1 == Android_to_Board)
    {
        USART_RequestMesg(&Tx1, mesg);
        if (List_IsExistID(&DealList, mesg->ID) == false)
        {
            switch (mesg->Code2)
            {
                /// 版本请求
            case r_GetVersion:
                EventGroupSetBits(&Mesg_event, MesgEvent_VersionRequest);
                break;
                /// 出珠
            case r_HoolleOutput:
                data = ((mesg->Data3 << 8) | mesg->Data4);
                if (mesg->ExpandCode == 0x00)
                    Hoolle_Output(&Motor_Hoolle2, data); // 瓷珠
                else
                    Hoolle_Output(&Motor_Hoolle1, data); // 钢珠
                break;
                /// 出卡
            case r_CardOutput:
                data = ((mesg->Data3 << 8) | mesg->Data4);
                Card_Output(&Card, data);
                break;
                /// 球盘亮度
            case r_BoardLightness:
                Setting.Board_Lightness = mesg->Data4;
                Light1.Init = true;
                Light2.Init = true;
                SemaphoreGive(Light1.Semaphore);
                SemaphoreGive(Light2.Semaphore);
                break;
                /// 灯带亮度
            case r_LightBeltLightness:
                Setting.LightBelt_Lightness = mesg->Data4;
                BreathLight_RefreshState(BreathList, 4);
                break;
                /// 场景切换
            case r_SceneChange:
                Scene = mesg->Data4;
                Light1.Init = true;
                Light2.Init = true;
                SemaphoreGive(Light1.Semaphore);
                SemaphoreGive(Light2.Semaphore);
                EventGroupSetBits(&Event, Event_SceneChange);
                break;
                /// 清珠
            case r_OutputAllHoolle:
                if (mesg->ExpandCode == 0x00)
                {
                    Motor_Hoolle2.ClearMode = 1;
                    Hoolle_Output(&Motor_Hoolle2, 0xFFFF - Motor_Hoolle2.Hoolle_num);
                }
                else
                {
                    Motor_Hoolle1.ClearMode = 1;
                    Hoolle_Output(&Motor_Hoolle1, 0xFFFF - Motor_Hoolle1.Hoolle_num);
                }
                break;
                /// 吐出剩余
            case r_OutputRemainingItem:
                Hoolle_Output(&Motor_Hoolle2, 0);
                Card_Output(&Card, 0);
                break;
                /// 恢复默认设置
            case r_ResumeDefultSetting:
                ResumeSetting();
                break;
                /// 保存设置
            case r_SaveSetting:
                EventGroupSetBits(&Mesg_event, Event_FlashData);
                break;
                /// 开锁
            case r_Unlock:
                Lock_Valve.Switch.state = DEVICE_STATE_START;
                EventGroupSetBits(&Mesg_event, MesgEvent_Unlock);
                break;
                /// 舵机控制
            case r_ServoControl:
                if (mesg->ExpandCode == 0x00)
                {
                    // 关门
                    Servo2.SetAngle(&Servo2, 5);
                    Servo3.SetAngle(&Servo3, 180);
                    RGB_SetMoreColor(&Light1, 19, 20, NONE, 0, 0);
                    RGB_SetMoreColor(&Light1, 36, 37, NONE, 0, 0);
                    EventGroupClearBits(&Mesg_event, Event_DoorOpen);
                    Light1.Init = true;
                }
                if (mesg->ExpandCode == 0x01)
                {
                    // 开门
                    Servo2.SetAngle(&Servo2, 80);
                    Servo3.SetAngle(&Servo3, 80);
                    EventGroupSetBits(&Mesg_event, Event_DoorOpen);
                    Light1.Init = true;
                }
                break;
                /// 灯光控制
            case r_LightControl:
                if (mesg->Data3 != 0x00)
                {
                    Light_BLUE[mesg->Data3 - 1]->state = mesg->ExpandCode;
                }
                if (mesg->Data4 != 0x00)
                {
                    Light_YELLOW[mesg->Data4 - 1]->state = mesg->ExpandCode;
                }
                break;
                /// 控台亮度
            case r_CtrlLightness:
                Setting.Ctrl_Lightness = mesg->Data4;
                Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, 0x04, Setting.Ctrl_Lightness, 0x00);
                break;
            /// 舵机归零
            case r_ServoReset:
                Servo1.SetAngle(&Servo1, 90);
                break;
                /// 停止所有设备
            case r_StopAllDevice:
                Motor_Hoolle1.Motor.state = DEVICE_STATE_STOP;
                Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
                Card.Switch.state = DEVICE_STATE_STOP;
                break;
                /// 数码管显示
            case r_DigitalTubeData:
            {
                uint32_t data = mesg->Data1 << 24 | mesg->Data2 << 16 | mesg->Data3 << 8 | mesg->Data4;
                // DigitalTube.Set_Num(&DigitalTube, 0, data, 4);
                // DigitalTube.Refresh(&DigitalTube);
                Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, 0x01, data, 0x00); // 发送给控台板
                break;
            }
            }
            /// 将该消息包加入已处理列表，防止短时间内重复处理同样ID的消息包
            List_AddNode(&DealList, mesg->ID, HAL_GetTick());
        }
    }
    /// 收到的是应答
    else if (mesg->Code1 == Board_to_Android)
    {
        /// 重发列表中去除该消息包
        List_DeleteNode(&ResendList, mesg->ID);
    }
}

/// @brief  串口3处理
/// @param Rx_mesg
static void USART3_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;
    if (mesg->Code1 == Ctrl_to_Board)
    {
        switch (mesg->Code2)
        {
        /// 按键
        case 0x01:
            Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Button, mesg->Data4, mesg->ExpandCode);
            break;
        /// 键盘
        case 0x02:
            Comm_SendMesg_FillData(&Tx1, Board_to_Android, 0x15, mesg->Data4, mesg->ExpandCode);
            break;
        /// 编码器
        case 0x03:
            if (mesg->ExpandCode == 0x00)
            {
                Servo1.DecreaseAngle(&Servo1, 1);
                Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Encoder, 0x00, 0x01);
            }
            else if (mesg->ExpandCode == 0x01)
            {
                Servo1.IncreaseAngle(&Servo1, 1);
                Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Encoder, 0x00, 0x00);
                
            }
            else if (mesg->ExpandCode == 0x02)
                Servo1.SetAngle(&Servo1, 90);
            break;
        }
    }
}
///====================================================================================

/// 发送消息，无重传
static uint8_t USART_SendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    static uint8_t ID = 0;
    uint8_t data[14];
    uint16_t crc;

    ID++;                  // 每次发送新消息都会自增
    mesg->ResendID = 0;    // 重发次数清零
    mesg->ID = ID;         // 赋予新ID号
    MesgTable[ID] = *mesg; // 保存消息包
    memcpy(data, mesg, 14);
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    // Tx->Transimit(Tx, data, 14);
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return ID;
}
/// 填入参数发送消息，无重传
uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode)
{
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.ResendID = 0;
    mesg.ID = 0;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)(data);
    mesg.ACKbyte = 0x00;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    return USART_SendMesg(Tx, &mesg);
}

/// 填充数据发送消息并加入重发列表
uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List)
{
    uint8_t ID;
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.ResendID = 0;
    mesg.ID = 0;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)(data);
    mesg.ACKbyte = 0x01;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    ID = USART_SendMesg(Tx, &mesg);
    List_AddNode(List, ID, HAL_GetTick());
    return ID;
}

/// 发送重发消息
static uint8_t USART_ReSendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;
    mesg->ResendID++;
    if (mesg->ResendID > Max_Resend_Times)
        return 1;
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    data[2] = mesg->ID;
    data[3] = mesg->Code1;
    data[4] = mesg->Code2;
    data[5] = mesg->Data1;
    data[6] = mesg->Data2;
    data[7] = mesg->Data3;
    data[8] = mesg->Data4;
    data[9] = mesg->ACKbyte;
    data[10] = mesg->ExpandCode;
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    data[13] = Mesg_Tail;
    // Tx->Transimit(Tx, data, 14);
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return 0;
}
/// 发送应答消息
static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    data[2] = mesg->ID;
    data[3] = mesg->Code1;
    data[4] = mesg->Code2;
    data[5] = mesg->Data1;
    data[6] = mesg->Data2;
    data[7] = mesg->Data3;
    data[8] = mesg->Data4;
    data[9] = mesg->ACKbyte;
    data[10] = mesg->ExpandCode;
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    data[13] = Mesg_Tail;
    // Tx->Transimit(Tx, data, 14);
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
}

/*
 * ----------检测重发消息----------
 */
void Resend_Task(void)
{
    ListNode_t *Current = ResendList.Head;
    uint32_t CurrentTime = HAL_GetTick();
    for (uint8_t i = 0; i < ResendList.NodeCount; i++)
    {
        // 超时时间内未收到应答，立即重发
        if (CurrentTime - Current->Value > ResendTrigger_Time)
        {
            USART_ReSendMesg(&Tx1, &(MesgTable[Current->ID]));
            Current->Value = CurrentTime;
            // 如果重发次数达到最大次数，则从重发列表中删除
            if (MesgTable[Current->ID].ResendID >= Max_Resend_Times)
                List_DeleteNode(&ResendList, Current->ID);
        }
        Current = Current->Next;
    }
}

/*
 * ----------清除已执行消息任务----------
 */
void MesgDeal_Task(void)
{
    ListNode_t *Current = DealList.Head;
    uint32_t CurrentTime = HAL_GetTick();
    for (uint8_t i = 0; i < DealList.NodeCount; i++)
    {
        // 达到超时时间则从列表中删除，表示可接收同样ID的新消息
        if (CurrentTime - Current->Value > MesgDeal_Time)
            List_DeleteNode(&DealList, Current->ID);
        Current = Current->Next;
    }
}

/*
 * ----------通信初始化----------
 */
void CommInit(void)
{
    List_Create(&ResendList, ResendList_buffer, 100);
    List_Create(&DealList, DealList_buffer, 100);

    /// 串口1接收
    Rx_InitTypeDef Rxinit;
    Rxinit.huart = &huart1;
    Rxinit.RingBuf = rx1_buffer;
    Rxinit.RingBuf_Size = sizeof(rx1_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART1_ReceiveMesg_Verify;
    Rxinit.Deal = USART1_Deal;
    Communicate_Rx_Init(&Rx1, Rxinit);

    /// 串口3接收
    Rxinit.huart = &huart3;
    Rxinit.RingBuf = rx3_buffer;
    Rxinit.RingBuf_Size = sizeof(rx3_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART1_ReceiveMesg_Verify;
    Rxinit.Deal = USART3_Deal;
    Communicate_Rx_Init(&Rx3, Rxinit);

    /// 串口1发送
    Tx_InitTypeDef Tx_init;
    Tx_init.huart = &huart1;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx1, Tx_init);
    /// 串口3发送
    Tx_init.huart = &huart3;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx3, Tx_init);
}

void CommTask(void)
{
    Rx1.Receive(&Rx1, &Receive1_mesg, 14);
    Rx3.Receive(&Rx3, &Receive3_mesg, 14);
}
