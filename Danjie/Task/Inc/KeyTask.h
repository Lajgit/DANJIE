#ifndef __KEYTASK_H__
#define __KEYTASK_H__

#define KEY_DEBOUNCE_TIME 15
#define KEY_LONG_PRESS_TIME 800
#define KEY_LONG_TRIGGER_FREQUENCY 10


void KeyAll_Init(void);
void Key_Task(void);

#endif
