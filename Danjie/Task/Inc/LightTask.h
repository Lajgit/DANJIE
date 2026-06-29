#ifndef __LIGHTTASK_H__
#define __LIGHTTASK_H__

#include "port_lighteffect.h"
#include "port_light.h"

#define Light1_RGBbufSize 66
#define Light2_RGBbufSize 59
#define Light1_CRRbufSize ((Light1_RGBbufSize + 7) * 24)
#define Light2_CRRbufSize ((Light2_RGBbufSize + 7) * 24)


#define LightPause_Time 500
#define VictoryTime 6000

typedef enum
{
    LIGHT_STATE_OFF = 0,
    LIGHT_STATE_ON,
    LIGHT_STATE_FLOW,
}Lightstate_t;

void LightTask_Init(void);
void LightTask(void);


#endif
