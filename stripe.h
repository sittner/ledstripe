#ifndef _STRIPE_H_
#define _STRIPE_H_

#include <stdint.h>

void stripe_init(void);
void stripe_send(const void *src, const uint32_t length);

#endif

