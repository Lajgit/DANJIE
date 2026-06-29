#ifndef __MAINTASK_H__
#define __MAINTASK_H__

#include "port_event.h"


#define EncoderCheckTime 100
#define FlashData_SIZE 5

#define Event_SettingButtonPress (1u << 0)
#define Event_EncoderCheck (1u << 1)
#define Event_Encoder_L (1u << 2)
#define Event_Encoder_R (1u << 3)
#define Event_FlashData (1u << 4)
#define Event_ReadFlashData (1u << 5)
#define Event_SceneChange (1u << 6)
#define Event_DoorOpen (1u << 7)
#define Event_SaveSetting (1u << 8)

typedef enum
{
    SettingScene = 0,
    IdleScene = 1,
    PlayingScene = 2,
    LittleGame_1 = 3,
    LittleGame_2 = 4,
} Scene_t;

void System_Reset(void);
void Main_Init(void);
void Main_Task(void);

#endif
