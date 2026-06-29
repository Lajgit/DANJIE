#include "KeyTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "LightTask.h"
#include "port_key.h"
#include "port_event.h"
#include "string.h"

static GPIO_TypeDef *GPIO_Port[] = {Hole_B1_GPIO_Port, Hole_B2_GPIO_Port, Hole_B3_GPIO_Port, Hole_B4_GPIO_Port, Hole_B5_GPIO_Port, Hole_B6_GPIO_Port, Hole_B7_GPIO_Port, Hole_B8_GPIO_Port,
                                    Hole_Y1_GPIO_Port, Hole_Y2_GPIO_Port, Hole_Y3_GPIO_Port, Hole_Y4_GPIO_Port, Hole_Y5_GPIO_Port, Hole_Y6_GPIO_Port, Hole_Y7_GPIO_Port, Hole_Y8_GPIO_Port,
                                    Hole_P1_GPIO_Port, Hole_P2_GPIO_Port, Hole_P3_GPIO_Port};
static uint32_t GPIO_Pin[] = {Hole_B1_Pin, Hole_B2_Pin, Hole_B3_Pin, Hole_B4_Pin, Hole_B5_Pin, Hole_B6_Pin, Hole_B7_Pin, Hole_B8_Pin,
                              Hole_Y1_Pin, Hole_Y2_Pin, Hole_Y3_Pin, Hole_Y4_Pin, Hole_Y5_Pin, Hole_Y6_Pin, Hole_Y7_Pin, Hole_Y8_Pin,
                              Hole_P1_Pin, Hole_P2_Pin, Hole_P3_Pin};

static GPIO_TypeDef *Keyboard_Port[] = {KeyBoard1_GPIO_Port, KeyBoard2_GPIO_Port, KeyBoard3_GPIO_Port};
static uint32_t Keyboard_Pin[] = {KeyBoard1_Pin, KeyBoard2_Pin, KeyBoard3_Pin};

Key_HandleTypeDef key[19];
Key_HandleTypeDef *key_list[19];
Key_HandleTypeDef keyboard[3];
Key_HandleTypeDef *keyboard_list[3];

extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;

extern Tx_HandleTypeDef Tx1;

extern ListHandle_t ResendList, DealList;
extern Light_Handle_t *HoleLightList[];

extern Scene_t Scene;

extern Switch_Valve Lock_Valve;

extern Light_Handle_t *Light_BLUE[8];
extern Light_Handle_t *Light_YELLOW[8];
extern uint8_t LightCache[9];
extern Light_t Light2;
/*
 * ----------按键短按回调----------
 */
static void Key_ShortCallback(uint16_t key_id)
{
    // 蓝洞
    if (key_id <= 7)
    {
        Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_LightEye, key_id + 1, 0x00, &ResendList);
    }
    // 黄洞
    else if (key_id >= 8 && key_id <= 15)
    {
        Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_LightEye, key_id - 7, 0x01, &ResendList);
    }
    // 粉洞
    else if (key_id >= 16 && key_id <= 18)
    {
        Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_LightEye, key_id - 15, 0x02, &ResendList);
    }
}
/*
 * ----------按键长按回调----------
 */

static void Key_LongCallback(uint16_t key_id)
{
}

/*
 * ----------按键松开回调----------
 */

static void Key_ReleaseCallback(uint16_t key_id)
{
}

static void Hole_Init(void)
{
    Key_InitTypeDef init;
    for (uint16_t i = 0; i < 19; i++)
    {
        init.short_callback = Key_ShortCallback;
        init.long_callback = Key_LongCallback;
        init.release_callback = Key_ReleaseCallback;
        init.debounce_time = KEY_DEBOUNCE_TIME;
        init.longpress_time = KEY_LONG_PRESS_TIME;
        init.trigger_frequnecy = KEY_LONG_TRIGGER_FREQUENCY;
        init.trigger_level = GPIO_PIN_SET;

        init.key_id = i;
        init.port = GPIO_Port[i];
        init.pin = GPIO_Pin[i];
        Key_Init(&key[i], init);
        key_list[i] = &key[i];
    }
}

/// 小键盘按键

static void Keyboard_ShortCallback(uint16_t key_id)
{
    Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, key_id + 1, 0x01);
}

static void Keyboard_LongCallback(uint16_t key_id)
{
    Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, key_id + 1, 0x02);
}

static void Keyboard_ReleaseCallback(uint16_t key_id)
{
    Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, key_id + 1, 0x03);
}
void Keyboard_Init(void)
{
    Key_InitTypeDef init;
    for (uint16_t i = 0; i < 3; i++)
    {
        init.short_callback = Keyboard_ShortCallback;
        init.long_callback = Keyboard_LongCallback;
        init.release_callback = Keyboard_ReleaseCallback;
        init.debounce_time = KEY_DEBOUNCE_TIME;
        init.longpress_time = KEY_LONG_PRESS_TIME;
        init.trigger_frequnecy = 1;
        init.trigger_level = GPIO_PIN_RESET;

        init.key_id = i;
        init.port = Keyboard_Port[i];
        init.pin = Keyboard_Pin[i];
        Key_Init(&keyboard[i], init);
        keyboard_list[i] = &keyboard[i];
    }
}

void KeyAll_Init(void)
{
    Hole_Init();
    Keyboard_Init();
}

void Key_Task(void)
{
    Key_Scan(key_list, 19);
    Key_Scan(keyboard_list, 3);
}
