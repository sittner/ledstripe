#include <xmc_gpio.h>
#include <string.h>

#include "stripe.h"
#include "font.h"

#define BTN1 P15_13
#define BTN2 P15_12

#define LED1 P5_9
#define LED2 P5_8

static const XMC_GPIO_CONFIG_t gpio_input_tristate_config = {
  .mode = XMC_GPIO_MODE_INPUT_TRISTATE,
};

static const XMC_GPIO_CONFIG_t gpio_output_low_config = {
  .mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
  .output_level = XMC_GPIO_OUTPUT_LEVEL_LOW,
  .output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SOFT_EDGE
};

#define LED_COLS     30
#define LED_CHANNELS 3

static const char msg[]   = "   \003lichen Gl\232ckwunsch liebe Annette. Wir w\232nschen Dir viele weitere erf\232llte und gl\232ckliche Jahre und eine Hammerfete!      ";
static const char color[] = "   rcccccc ggggggggggg yyyyy wwwwwwww mmm cccccccc www yyyyy bbbbbbb mmmmmmmm www mmmmmmmmmm ccccc rrr bbbb wwwwwwwwwwm      ";

typedef struct {
  const char *msg_buf;
  const char *msg_pos;
  const char *color_buf;
  const char *color_pos;
  int shift;
} MSG_POS_T;

static void msg_pos_init(MSG_POS_T *pos, const char *msg_buf, const char *color_buf)
{
  pos->msg_buf = msg_buf;
  pos->msg_pos = msg_buf;
  pos->color_buf = color_buf;
  pos->color_pos = color_buf;
  pos->shift = 0;
}

typedef struct {
  char name;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} NAMED_RGB_COLOR_T;

static const NAMED_RGB_COLOR_T color_map[] = {
  { 'r', 0xff, 0x00, 0x00 },
  { 'g', 0x00, 0xff, 0x00 },
  { 'b', 0x00, 0x00, 0xff },
  { 'c', 0x00, 0xff, 0xff },
  { 'y', 0xff, 0xff, 0x00 },
  { 'm', 0xff, 0x00, 0xff },
  { 'w', 0xff, 0xff, 0xff },
  { 0 }
};

static const NAMED_RGB_COLOR_T *lookup_color(char name)
{
  const NAMED_RGB_COLOR_T *p;

  for (p = color_map; p->name != 0; p++) {
    if (p->name == name) {
      return p;
    }
  }

  return NULL;
}

static void msg_pos_inc(MSG_POS_T *pos)
{
  (pos->shift)++;
  if (pos->shift < FONT_WIDTH) {
    return;
  }

  pos->shift = 0;

  (pos->msg_pos)++;
  if (*(pos->msg_pos) == 0) {
    pos->msg_pos = pos->msg_buf;
  }

  (pos->color_pos)++;
  if (*(pos->color_pos) == 0) {
    pos->color_pos = pos->color_buf;
  }
}

static MSG_POS_T msg_pos;

static uint8_t led_buf[FONT_HEIGHT][LED_COLS][LED_CHANNELS];

void SysTick_Handler(void)
{
  int x, y;
  MSG_POS_T mp;
  uint16_t m;
  const NAMED_RGB_COLOR_T *c;

  memcpy(&mp, &msg_pos, sizeof(mp));
  for (x = 0; x < LED_COLS; x++) {
    m = font[(uint8_t) *(mp.msg_pos)][mp.shift];
    c = lookup_color(*(mp.color_pos));
    msg_pos_inc(&mp);

    for (y = 0; y < FONT_HEIGHT; y++, m >>= 1) {
      if (c != NULL && (m & 1) != 0) {
        led_buf[y][x][0] = c->g;
        led_buf[y][x][1] = c->r;
        led_buf[y][x][2] = c->b;
      } else {
        led_buf[y][x][0] = 0;
        led_buf[y][x][1] = 0;
        led_buf[y][x][2] = 0;
      }
    }
  }

  stripe_send(led_buf, sizeof(led_buf));
  msg_pos_inc(&msg_pos);
}

int main(void)
{
  XMC_GPIO_Init(BTN1, &gpio_input_tristate_config);
  XMC_GPIO_Init(BTN2, &gpio_input_tristate_config);
  XMC_GPIO_Init(LED1, &gpio_output_low_config);
  XMC_GPIO_Init(LED2, &gpio_output_low_config);
  stripe_init();

  msg_pos_init(&msg_pos, msg, color);
  memset(led_buf, 0, sizeof(led_buf));

  // system timer configuration for 10ms cycle time
  SysTick_Config(SystemCoreClock / 30);

  while(1);

  return 0;
}

