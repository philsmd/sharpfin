/* Reciva LEDs */

#define RLED_NONE   0x00
#define RLED_MENU   0x01
#define RLED_VOLUME 0x02
extern void reciva_led_set(int bitmask);
extern int reciva_get_leds_supported(void);

