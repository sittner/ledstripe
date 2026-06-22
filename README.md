# ledstripe

ESP-IDF v5.x project for ESP32-S3 driving a WS2812B LED matrix (30x16) with scrolling colored text.

## Hardware

- ESP32-S3 board
- WS2812B LED strip or matrix (default: 30 columns x 16 rows = 480 LEDs)
- Data line from ESP32-S3 GPIO48 (default) to WS2812B DIN
- Shared ground between ESP32-S3 and LED power supply
- External 5V power sized for LED count/current

## Build and flash (ESP-IDF)

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

If your data pin differs from GPIO48, update `STRIPE_GPIO_NUM` in `main/stripe.h`.
