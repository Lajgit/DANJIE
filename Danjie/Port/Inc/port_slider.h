#ifndef __PORT_SLIDER_H__
#define __PORT_SLIDER_H__

#include "stdint.h"

#define tempbuffer_size 10

/// @brief  坐标表
typedef struct
{
    uint16_t value; // 坐标对应值
    uint16_t pos;   // 坐标编号
} PositionTable;

/// @brief  滑块
typedef struct
{
    PositionTable *table;
    uint16_t table_size;
    uint16_t temp[tempbuffer_size];
    struct
    {
        uint16_t Pre_Value;
        uint16_t Curr_Value;
        uint16_t Pre_Pos;
        uint16_t Curr_Pos;
    } block;
    struct
    {
        void (*init)(void);
        uint16_t (*read)(void *self);
    } ops;
} Slider_t;

typedef struct
{
    PositionTable *table;
    uint16_t table_size;
    void (*init)(void);
    uint16_t (*read)(void *self);
} Slider_Init_t;

void Slider_Init(Slider_t *Slider, Slider_Init_t init);

#endif
