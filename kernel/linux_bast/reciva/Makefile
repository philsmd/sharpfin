CROSS_COMPILE = /usr/local/arm/3.3.4/bin/arm-linux-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld

# These modules export symbols
export-objs := reciva_keypad_stratas.o

CFLAGS = -D__KERNEL__ -DMODULE -I../include -O2 -Werror -Wunused -DEXPORT_SYMTAB -Wall -Wstrict-prototypes

LCD_DRIVERS = reciva_lcd_mg1203d.o reciva_lcd_ateam.o reciva_lcd_tm13264cbcg.o reciva_lcd_tm13264cbcg_new.o reciva_lcd_tianma.o

MODULES = reciva_lcd_ateam_4bit.o                                     \
          reciva_lcd_ateam_8bit.o                                     \
          reciva_quad.o                                               \
          reciva_quad_piher_ci11.o                                    \
          reciva_quad_piher_20pulse.o                                 \
          reciva_quad_pins_standard.o                                 \
          reciva_quad_pins_reversed.o                                 \
          reciva_powerbutton_windermere.o                             \
          reciva_keypad_config973.o                                   \
          reciva_keypad_config974.o                                   \
          reciva_keypad_config975.o                                   \
          reciva_keypad_config983.o                                   \
          reciva_keypad_config985.o                                   \
          reciva_keypad_config986.o                                   \
          reciva_keypad_config988.o                                   \
          reciva_keypad_config989.o                                   \
          reciva_keypad_config990.o                                   \
          reciva_keypad_config991.o                                   \
          reciva_keypad_config992.o                                   \
          reciva_keypad_config993.o                                   \
          reciva_keypad_config997.o                                   \
          reciva_keypad_config1006.o                                  \
          reciva_keypad_config1007.o                                  \
          reciva_keypad_config1009.o                                  \
          reciva_keypad_config1012.o                                  \
          reciva_keypad_generic.o                                     \
          reciva_keypad_windermere.o                                  \
          reciva_keypad_vpro500.o                                     \
          reciva_keypad_appa_issueb.o                                 \
          reciva_keypad_allegro.o                                     \
          reciva_keypad_alto.o                                        \
          reciva_keypad_potas.o                                       \
          reciva_keypad_itech_cube.o                                  \
          reciva_gpio.o                                               \
          reciva_util.o                                               \
          reciva_status_pins.o                                        \
          $(LCD_DRIVERS)                                              \
          reciva_leds_potas.o                                         \
          reciva_backlight_dummy.o                                    \
          reciva_backlight_potas.o                                    \
          reciva_backlight_appb.o                                     \
          reciva_audio_mute.o                                         \
          reciva_font_chinese_imt.o                                   \
          reciva_font_chinese_dummy.o																  \
          reciva_slave_mode_controller.o                              \
          reciva_slave_mode_driver_i2c.o                              \
          reciva_slave_mode_driver_serial.o                           \
          reciva_slave_mode_status_master.o                           \
          reciva_slave_mode_status_slave.o                            \
          reciva_si4700.o                                             \
          reciva_uart.o                                               \
          reciva_i2c_slave.o                                          \
          reciva_i2c_master.o                                         \
          reciva_misc_control.o                                       \
          reciva_usb.o                                                \
          reciva_dac_monitor.o                                        \
					reciva_dac_ak4387.o																					\

all: $(MODULES)

DEPS=$(patsubst %.o, %.d, $(MODULES))

reciva_quad_noasm.o: reciva_quad.c
	$(CC) -c $(CFLAGS) -o $@ $^

#%.d: %.c Makefile
#	set -e; echo -n `dirname $<`/ > $@.$$$$ ; \
#	$(CC) -MM $(CPPFLAGS) $< >> $@.$$$$; \
#	sed 's,\($*\)\.o[ :]*,\1.o \1.i $@ : ,g' < $@.$$$$ > $@; \
#	rm -f $@.$$$$

#$(patsubst(reciva_lcd_%.o: reciva_lcd_%_nogen.o lcd_generic.o
#	$(LD) -r -o $@ $^
#	arm-linux-objcopy '--strip-symbol=$$a' $@ # for debug


reciva_lcd_mg1203d.o: reciva_lcd_mg1203d_nogen.o lcd_generic.o
	$(LD) -r -o $@ $^
	arm-linux-objcopy '--strip-symbol=$$a' $@ # for debug

reciva_lcd_ateam.o: reciva_lcd_ateam_nogen.o lcd_generic.o
	$(LD) -r -o $@ $^
	arm-linux-objcopy '--strip-symbol=$$a' $@ # for debug

reciva_lcd_tm13264cbcg.o: reciva_lcd_tm13264cbcg_nogen.o lcd_generic.o
	$(LD) -r -o $@ $^
	arm-linux-objcopy '--strip-symbol=$$a' $@ # for debug

reciva_lcd_tm13264cbcg_new.o: reciva_lcd_tm13264cbcg_new_nogen.o lcd_generic.o
	$(LD) -r -o $@ $^
	arm-linux-objcopy '--strip-symbol=$$a' $@ # for debug

reciva_lcd_tianma.o: reciva_lcd_tianma_nogen.o lcd_generic.o
	$(LD) -r -o $@ $^
	arm-linux-objcopy '--strip-symbol=$$a' $@ # for debug

reciva_lcd_%_nogen.o: reciva_lcd_%.c
	$(CC) -c $(CFLAGS) -o $@ $^
	
reciva_quad.o: reciva_quad_noasm.o reciva_fiq.o
	$(LD) -r -o $@ $^

reciva_font_chinese_imt.o: reciva_font_chinese.c
	$(CC) -c $(CFLAGS) -o $@ $^
	
reciva_font_chinese_dummy.o: reciva_font_chinese.c
	$(CC) -c $(CFLAGS) -DRECIVA_DUMMY_FONT -o $@ $^

clean:
	rm -f *.o *.d *.d.*

.PHONY: clean

-include $(DEPS)



