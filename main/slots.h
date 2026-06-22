#ifndef _SLOTS_H_
#define _SLOTS_H_

#include <stdbool.h>

#define SLOTS_COUNT 8
#define SLOT_MAX_LEN 256

typedef struct {
    char text[SLOT_MAX_LEN];
    char colors[SLOT_MAX_LEN];
} slot_t;

void slots_init(void);
void slots_save(int index, const char *text, const char *colors);
void slots_set_active(int index);
int slots_get_active(void);
const slot_t *slots_get(int index);
const slot_t *slots_get_all(void);

/* Thread-safe copy of the currently active slot into dst. */
void slots_copy_active(slot_t *dst);

/* Returns true (once) if any slot was modified since the last call. */
bool slots_check_changed(void);

#endif
