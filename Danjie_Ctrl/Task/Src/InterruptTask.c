#include "InterruptTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "CommTask.h"
#include "port_event.h"
#include "tim.h"
#include "stdio.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

extern Rx_HandleTypeDef Rx;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == Rx.Handle.huart)
    {
        Rx.Handle.RingBuf.f_WriteByte(&Rx.Handle.RingBuf, Rx.Handle.temp_data);
        HAL_UART_Receive_IT(huart, &Rx.Handle.temp_data, 1);
    }
}