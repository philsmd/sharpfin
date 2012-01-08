@ FIQ handling code for Reciva quadrature interface

@ r9 = previous sampled state (for debounce)
@ r10 = debounce counter
@ r11 = current sequence position
@ r8, r12, r13 = temporary storage

/****************************************************************************
 **
 ** Rotary Encoder Operation :
 **
 ** A clockwise click produces a 2 step state change in the rotary encoder
 ** outputs from 00 to 11 or 11 to 00 as shown below.
 **                      ______
 **   A ________________|      |________
 **                   ______
 **   B _____________|      |___________
 **
 **
 **   An anticlockwise click produces a 2 step state change in the rotary encoder
 **   outputs from 00 to 11 or 11 to 00 as shown below.
 **                   ______
 **   A _____________|      |___________
 **                      ______
 **   B ________________|      |________
 **  
 **
 ****************************************************************************/

	.globl	quad_fiq_start
quad_fiq_start:
	mov	r8, #0xd8000000
	mov	r13, #1 << 13
	str	r13, [r8]		@ clear bit in SRCPND
	orr	r8, r8, #0x00e00000
	ldr	r13, [r8, #0x54]	@ fetch GPFDAT
	and	r13, r13, #0x3
	teq	r13, r9
	bne	not_same
	cmn	r10, #1
	subeqs	pc, lr, #4		@ nothing to do
	add	r10, r10, #1
	teq	r10, #3
	subnes	pc, lr, #4		@ nothing to do

	ldr	r10, 2f
	adr	r12, sequence_up
	ldrb	r8, [r12, r11]
	teq	r8, r13
	moveq	r8, #1
	streq	r8, [r10]
	adr	r12, sequence_down
	ldrb	r8, [r12, r11]
	teq	r8, r13
	moveq	r8, #2
	streq	r8, [r10]
	mvn	r10, #0
	mov	r11, r13
	subs	pc, lr, #4

not_same:
	mov	r10, #0
	mov	r9, r13
	subs	pc, lr, #4

	.align	5
sequence_up:
	.byte	1, 3, 0, 2

sequence_down:
	.byte	2, 0, 3, 1

2:	.word	reciva_quad_data

	.globl	quad_fiq_end
quad_fiq_end:
