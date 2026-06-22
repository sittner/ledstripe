#ifndef _FONT_H_
#define _FONT_H_

#include <stdint.h>

#define FONT_CHARS 256
#define FONT_HEIGHT 16
#define FONT_WIDTH 12

extern const uint16_t font[FONT_CHARS][FONT_WIDTH];

#endif
