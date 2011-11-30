
#define CTL_DISPLAY 0xae
#define CTL_DISPLAY_ON  (CTL_DISPLAY | (1 << 0))
#define CTL_DISPLAY_OFF (CTL_DISPLAY | (0 << 0))

#define CTL_ADC_SELECT 0xa0
#define CTL_ADC_SEG_NORMAL  (CTL_ADC_SELECT | (0 << 0))
#define CTL_ADC_SEG_REVERSE (CTL_ADC_SELECT |(1 << 0))

#define CTL_SHL_SELECT 0xc0
#define CTL_SHL_SEG_NORMAL  (CTL_SHL_SELECT | (0 << 3))
#define CTL_SHL_SEG_REVERSE (CTL_SHL_SELECT | (1 << 3))

#define CTL_LCD_BIAS 0xa2
#define CTL_LCD_BIAS_0 (CTL_LCD_BIAS | (0 << 0))
#define CTL_LCD_BIAS_1 (CTL_LCD_BIAS | (1 << 0))

#define CTL_POWER 0x28
#define CTL_POWER_VC (1 << 2)
#define CTL_POWER_VR (1 << 1)
#define CTL_POWER_VF (1 << 0)

#define CTL_REGRES 0x20
#define CTL_REGRES_1_90 (CTL_REGRES | 0)
#define CTL_REGRES_2_19 (CTL_REGRES | 1)
#define CTL_REGRES_2_55 (CTL_REGRES | 2)
#define CTL_REGRES_3_02 (CTL_REGRES | 3)
#define CTL_REGRES_3_61 (CTL_REGRES | 4)
#define CTL_REGRES_4_35 (CTL_REGRES | 5)
#define CTL_REGRES_5_29 (CTL_REGRES | 6)
#define CTL_REGRES_6_48 (CTL_REGRES | 7)

#define CTL_SET_REF_V 0x81

#define CTL_SET_INIT_LINE 0x40

#define CTL_SET_PAGE 0xb0

#define CTL_SET_COL_MSB 0x10
#define CTL_SET_COL_LSB 0x00

#define RIRM_LCDCN_DAT GPCDAT

#define RIRM_LCDCN0_PIN  (1 << 0)
#define RIRM_LCDCN1_PIN  (1 << 1)
#define RIRM_LCDCN2_PIN  (1 << 2)
#define RIRM_LCDCN3_PIN  (1 << 3)
#define RIRM_LCDCN4_PIN  (1 << 4)
#define RIRM_LCDCN5_PIN  (1 << 5)

//#define ROBERTS
#ifndef ROBERTS
# define LCD_E     RIRM_LCDCN0_PIN
# define LCD_RW    RIRM_LCDCN1_PIN
# define LCD_RS    RIRM_LCDCN2_PIN
# define LCD_RESET RIRM_LCDCN3_PIN
# define LCD_CS1   RIRM_LCDCN4_PIN
# define LCD_POWER RIRM_LCDCN5_PIN
#else
# define LCD_E     RIRM_LCDCN4_PIN
# define LCD_RW    RIRM_LCDCN3_PIN
# define LCD_RS    RIRM_LCDCN2_PIN
# define LCD_RESET RIRM_LCDCN1_PIN
# define LCD_CS1   RIRM_LCDCN0_PIN
# define LCD_POWER RIRM_LCDCN5_PIN
#endif
