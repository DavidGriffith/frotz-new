typedef unsigned short zword;
void os_init_sound(void);                     /* startup system*/
void os_beep(int);                            /* enqueue a beep sample*/
void os_prepare_sample(int);                  /* put a sample into memory*/
void os_start_sample(int, int, int, zword);   /* queue up a sample*/
void os_stop_sample(int);                     /* terminate sample*/
void os_finish_with_sample(int);              /* remove from memory*/
