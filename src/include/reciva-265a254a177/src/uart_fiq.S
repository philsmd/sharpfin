@ FIQ handling code for Reciva soft-uart transmitter

        @ r8 = character currently being transmitted
        @ r9 = bit position in byte
        @ r10 = number of bits
        @ r11-r13: temporary

        @ using timer 2

#include <linux/version.h>
#include <asm/arch/map.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define S3C2410_VA_IRQ  S3C24XX_VA_IRQ
#define S3C2410_VA_GPIO S3C24XX_VA_GPIO
#endif

#define TIMER2_BIT              (1 << 12)
#define IRQ_BASE                S3C2410_VA_IRQ
#define SRCPND                  0x0
#define INTMOD                  0x4

#define GPIO_BASE               S3C2410_VA_GPIO
#define GPBDAT                  0x14

	.globl	uart_fiq_start
uart_fiq_start:
        mov     r0, r0                  @ XXX voodoo workaround for mystery memory stomper
        mov     r11, #(GPIO_BASE & 0xff000000)
        orr     r11, r11, #(GPIO_BASE & 0x00ff0000)
        ldr     r12, [r11, #GPBDAT]        @ read GPBDAT

        @ work out correct level for output pin
        mov     r13, #1
        tst     r8, r13, lsl r9
        orrne   r12, r12, #1 << 9
        biceq   r12, r12, #1 << 9

        str     r12, [r11, #GPBDAT]        @ set level of output pin

        add     r9, r9, #1                 @ increment  bit position
        teq     r9, r10                    @ have we transmitted correct number of bits ?

        @ clear interrupt
        mov     r11, #IRQ_BASE
        movne   r13, #TIMER2_BIT
        strne   r13, [r11, #SRCPND]

	subnes	pc, lr, #4

        @ we've transmitted all bits - set back to irq mode
        ldr     r13, [r11, #INTMOD]
        bic     r13, r13, #TIMER2_BIT
        str     r13, [r11, #INTMOD]        

	subs	pc, lr, #4

	.globl	uart_fiq_end
uart_fiq_end:
