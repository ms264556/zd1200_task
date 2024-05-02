/*
 * Copyright 2008-2010 Freescale Semiconductor, Inc.
 * Kumar Gala <kumar.gala@freescale.com>
 * This file is taken from u-boot
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation;
 */
#include <linux/init.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/cputable.h>
#include <asm/thread_info.h>
#include <asm/ppc_asm.h>
#include <asm/asm-offsets.h>
#include <asm/cache.h>

/* To boot secondary cpus, we need a place for them to start up.
 * Normally, they start at 0xfffffffc, but that's usually the
 * firmware, and we don't want to have to run the firmware again.
 * Instead, the primary cpu will set the BPTR to point here to
 * this page.  We then set up the core, and head to
 * start_secondary.  Note that this means that the code below
 * must never exceed 1023 instructions (the branch at the end
 * would then be the 1024th).
 */
	.globl	__secondary_start_page
	.align	12
__secondary_start_page:
	lis	r3, 0x8000		/* enable machine check */
#ifndef CONFIG_PPC_E500MC
	ori	r3,r3,0x4000		/* enable Timebase */
#endif
#ifdef CONFIG_PHYS_64BIT
	/* for 36-bit addressing */
	ori	r3,r3,0x0080		/* enable MAS7 updates */
#endif
	mtspr	SPRN_HID0,r3

#ifndef CONFIG_PPC_E500MC
	li	r3,0x3000		/* Addr streaming & broadcast */
	mtspr	SPRN_HID1,r3
#endif

	/* Enable branch prediction */
	li	r3,0x201
	mtspr	SPRN_BUCSR,r3

	/* Ensure TB is 0 */
	li	r3,0
	mttbl	r3
	mttbu	r3

	mfspr	r0,SPRN_L1CSR1
	ori	r0,r0,0x0003		/* Enable/invalidate the I-Cache */
	mtspr	SPRN_L1CSR1,r0
	isync


	mfspr	r0,SPRN_L1CSR0
	ori	r0,r0,0x0003		/* Enable/invalidate the D-Cache */
	msync
	isync
	mtspr	SPRN_L1CSR0,r0
	isync

#define toreset(x) (x - __secondary_start_page + 0xfffff000)

	/* get our PIR to figure out our table entry */
	lis	r3,toreset(__spin_table)@h
	ori	r3,r3,toreset(__spin_table)@l

	/* r10 has the base address for the entry */
	mfspr	r0,SPRN_PIR
#ifdef CONFIG_PPC_E500MC
	rlwinm	r4,r0,27,27,31
#else
	mr	r4,r0
#endif
	slwi	r8,r4,5
	add	r10,r3,r8

#define EPAPR_MAGIC		(0x45504150)
#define ENTRY_ADDR_UPPER	0
#define ENTRY_ADDR_LOWER	4
#define ENTRY_R3_UPPER		8
#define ENTRY_R3_LOWER		12
#define ENTRY_RESV		16
#define ENTRY_PIR		20
#define ENTRY_R6_UPPER		24
#define ENTRY_R6_LOWER		28
#define ENTRY_SIZE		32

	/* setup the entry */
	li	r3,0
	li	r8,1
	stw	r0,ENTRY_PIR(r10)
	stw	r3,ENTRY_ADDR_UPPER(r10)
	stw	r8,ENTRY_ADDR_LOWER(r10)
	stw	r3,ENTRY_R3_UPPER(r10)
	stw	r4,ENTRY_R3_LOWER(r10)
	stw	r3,ENTRY_R6_UPPER(r10)
	stw	r3,ENTRY_R6_LOWER(r10)

	/* setup mapping for AS = 1, and jump there */
	lis	r11,(MAS0_TLBSEL(1)|MAS0_ESEL(1))@h
	mtspr	SPRN_MAS0,r11
	lis	r11,(MAS1_VALID|MAS1_IPROT)@h
	ori	r11,r11,(MAS1_TS|MAS1_TSIZE(BOOK3E_PAGESZ_4K))@l
	mtspr	SPRN_MAS1,r11
	lis	r11,(0xfffff000|MAS2_I|MAS2_G)@h
	ori	r11,r11,(0xfffff000|MAS2_I|MAS2_G)@l
	mtspr	SPRN_MAS2,r11
	lis	r11,(0xfffff000|MAS3_SX|MAS3_SW|MAS3_SR)@h
	ori	r11,r11,(0xfffff000|MAS3_SX|MAS3_SW|MAS3_SR)@l
	mtspr	SPRN_MAS3,r11
	tlbwe

	bl	1f
1:	mflr	r11
	addi	r11,r11,28
	mfmsr	r13
	ori	r12,r13,MSR_IS|MSR_DS@l

	mtspr	SPRN_SRR0,r11
	mtspr	SPRN_SRR1,r12
	rfi

	/* spin waiting for addr */
2:
	lwz	r4,ENTRY_ADDR_LOWER(r10)
	andi.	r11,r4,1
	bne	2b
	isync

	/* get the upper bits of the addr */
	lwz	r11,ENTRY_ADDR_UPPER(r10)

	/* setup branch addr */
	mtspr	SPRN_SRR0,r4

	/* mark the entry as released */
	li	r8,3
	stw	r8,ENTRY_ADDR_LOWER(r10)

	/* mask by ~64M to setup our tlb we will jump to */
	rlwinm	r12,r4,0,0,5

	/* setup r3, r4, r5, r6, r7, r8, r9 */
	lwz	r3,ENTRY_R3_LOWER(r10)
	li	r4,0
	li	r5,0
	lwz	r6,ENTRY_R6_LOWER(r10)
	lis	r7,(64*1024*1024)@h
	li	r8,0
	li	r9,0

	/* load up the pir */
	lwz	r0,ENTRY_PIR(r10)
	mtspr	SPRN_PIR,r0
	mfspr	r0,SPRN_PIR
	stw	r0,ENTRY_PIR(r10)

	mtspr	SPRN_IVPR,r12
/*
 * Coming here, we know the cpu has one TLB mapping in TLB1[0]
 * which maps 0xfffff000-0xffffffff one-to-one.  We set up a
 * second mapping that maps addr 1:1 for 64M, and then we jump to
 * addr
 */
	lis	r10,(MAS0_TLBSEL(1)|MAS0_ESEL(0))@h
	mtspr	SPRN_MAS0,r10
	lis	r10,(MAS1_VALID|MAS1_IPROT)@h
	ori	r10,r10,(MAS1_TSIZE(BOOK3E_PAGESZ_64M))@l
	mtspr	SPRN_MAS1,r10
	/* WIMGE = 0b00000 for now */
	mtspr	SPRN_MAS2,r12
	ori	r12,r12,(MAS3_SX|MAS3_SW|MAS3_SR)
	mtspr	SPRN_MAS3,r12
#ifdef CONFIG_PHYS_64BIT
	mtspr	SPRN_MAS7,r11
#endif
	tlbwe

/* Now we have another mapping for this page, so we jump to that
 * mapping
 */
	mtspr	SPRN_SRR1,r13
	rfi

	.align L1_CACHE_SHIFT
	.globl __spin_table
/*__spin_table_loc:
	.space 544 - (__spin_table_loc - __secondary_start_page)
*/
__spin_table:
	.space NR_CPUS*ENTRY_SIZE

	/* Fill in the empty space.  The actual reset vector is
	 * the last word of the page */
__secondary_start_code_end:
	.space 4092 - (__secondary_start_code_end - __secondary_start_page)

__secondary_reset_vector:
	b	__secondary_start_page