;@----------------------------------------------------------------------------
	#include "mmc3.i"
;@----------------------------------------------------------------------------
	.global mapper88init
	.global mapper95init
	.global mapper154init
	.global mapper206init

	.struct mmc3Extra
mapper95RegA10:	.skip 6
mapper95ChrMode:	.byte 0

;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ Tengen's "MIMIC-1" and Namcot's "108", "109", "118", and "119".
;@ See also mapper 76
mapper206init:
;@----------------------------------------------------------------------------
	.word mappingW, rom_W, rom_W, rom_W
	b mmc3Init
;@----------------------------------------------------------------------------
;@ Namcot 108, 118
;@ Dragon Spirit - Aratanaru Densetsu
;@ Namcot Mahjong 3 - Mahjong Tengoku
;@ Quinty (J)
mapper88init:
;@----------------------------------------------------------------------------
	.word mapping88W,rom_W,rom_W,rom_W
	b mmc3Init
;@----------------------------------------------------------------------------
;@ Namcot 108
;@ Dragon Buster (J)
mapper95init:
;@----------------------------------------------------------------------------
	.word mapping95W,rom_W,rom_W,rom_W
	stmfd sp!,{lr}
	mov r0,#0
	strb_ r0,reg0
	bl mmc3Init

	ldr r0,=void
	str_ r0,scanlineHook
	mov r0,#0
	strb_ r0,irq_enable

	mov r0,#0
	adrl_ r1,mapper95RegA10
	mov r2,#7
mapper95ClearA10:
	strb r0,[r1],#1
	subs r2,r2,#1
	bne mapper95ClearA10

	bl mapper95UpdateMirror
	ldmfd sp!,{pc}
;@----------------------------------------------------------------------------
;@ Namcot 108
;@ Devil Man (J)
mapper154init:
;@----------------------------------------------------------------------------
	.word mapping88W,write1,write1,write1
	b mmc3Init

;@----------------------------------------------------------------------------
mappingW:		;@ 8000-9FFF
;@----------------------------------------------------------------------------
	tst addy,#1
	andne r0,r0,#0x3F
	andeq r0,r0,#0x07
	b mmc3MappingW
;@----------------------------------------------------------------------------
mapping95W:		;@ 8000-9FFF
;@----------------------------------------------------------------------------
	tst addy,#1
	bne mapping95DataW
	mov r1,r0,lsr#7
	and r1,r1,#1
	strb_ r1,mapper95ChrMode
	and r0,r0,#0x07
	stmfd sp!,{r3-r11,lr}
	bl mmc3Mapping0W
	bl mapper95UpdateMirror
	ldmfd sp!,{r3-r11,pc}
mapping95DataW:
	stmfd sp!,{r3-r11,lr}
	ldrb_ r1,reg0
	and r1,r1,#0x07
	cmp r1,#6
	bcs mapping95Prg
	mov r2,r0,lsr#5
	and r2,r2,#1
	adrl_ r3,mapper95RegA10
	strb r2,[r3,r1]
	and r0,r0,#0x1F
	bl mmc3Mapping1W
	bl mapper95UpdateMirror
	ldmfd sp!,{r3-r11,pc}
mapping95Prg:
	and r0,r0,#0x3F
	ldmfd sp!,{r3-r11,lr}
	b mmc3Mapping1W
;@----------------------------------------------------------------------------
mapping88W:		;@ 8000-9FFF
;@----------------------------------------------------------------------------
	tst addy,#1
	andeq r0,r0,#0x07
	beq mmc3Mapping0W

	ldrb_ r1,reg0
	and r0,r0,#0x3F
	cmp r1,#2
	orrcs r0,r0,#0x40
	b mmc3Mapping1W
;@----------------------------------------------------------------------------
write1:		;@ $8000-$FFFF
;@----------------------------------------------------------------------------
	tst r0,#0x40
	b mirror1_

;@----------------------------------------------------------------------------
mapper95UpdateMirror:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0-r12,lr}
	ldrb_ r2,mapper95ChrMode
	tst r2,#1
	adrl_ r3,mapper95RegA10
	ldreqb r0,[r3]
	ldrneb r0,[r3,#2]
	ldreqb r1,[r3,#1]
	ldrneb r1,[r3,#4]
	and r0,r0,#1
	and r1,r1,#1
	orr r2,r1,r0,lsl#1
	cmp r2,#0
	beq mapper95MirrorA
	cmp r2,#1
	beq mapper95MirrorVertical
	cmp r2,#2
	beq mapper95MirrorHorizontal
	mov r0,#1
	bl mirror1_
	b mapper95MirrorDone
mapper95MirrorA:
	mov r0,#0
	bl mirror1_
	b mapper95MirrorDone
mapper95MirrorVertical:
	mov r0,#0
	bl mirror2V_
	b mapper95MirrorDone
mapper95MirrorHorizontal:
	mov r0,#0
	bl mirror2H_
mapper95MirrorDone:
	ldmfd sp!,{r0-r12,pc}
;@----------------------------------------------------------------------------
