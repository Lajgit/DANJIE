#include "port_slider.h"
#include "main.h"

void Slider_Init(Slider_t *Slider, Slider_Init_t init)
{
    Slider->ops.init = init.init;
    Slider->ops.read = init.read;
    Slider->ops.init();

    Slider->table = init.table;
    Slider->table_size = init.table_size;
    Slider->block.Curr_Pos = Slider->table[0].pos;
    Slider->block.Pre_Pos = Slider->block.Curr_Pos;
    // 若使用DMA传输则不需要主动读取接口
    if(Slider->ops.read != NULL)    
        Slider->block.Curr_Value = Slider->ops.read(Slider);
    Slider->block.Pre_Value = Slider->block.Curr_Value;
}