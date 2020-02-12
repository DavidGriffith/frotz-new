#ifndef X_SETUP_H
#define X_SETUP_H

typedef struct x_setup_struct {
	int background_color;
	int foreground_color;
	int random_seed;
	int tandy_bit;
} x_setup_t;

extern x_setup_t x_setup;

#endif
