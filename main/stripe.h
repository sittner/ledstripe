#ifndef _STRIPE_H_
#define _STRIPE_H_

#include <stdint.h>

#include "font.h"

#define LED_COLS 30
#define LED_CHANNELS 3
#define STRIPE_GPIO_NUM 47

void stripe_init(void);
void stripe_send(const uint8_t led_buf[FONT_HEIGHT][LED_COLS][LED_CHANNELS]);

#endif
