/*
 * Implement AES algorithm in Intel AES-NI instructions.
 *
 * The white paper of AES-NI instructions can be downloaded from:
 *   http://softwarecommunity.intel.com/isn/downloads/intelavx/AES-Instructions-Set_WP.pdf
 *
 * Copyright (C) 2008, Intel Corp.
 *    Author: Huang Ying <ying.huang@intel.com>
 *            Vinodh Gopal <vinodh.gopal@intel.com>
 *            Kahraman Akdemir
 *
 * Ported x86_64 version to x86:
 *    Author: Mathias Krause <minipli@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/linkage.h>
#include <asm/inst.h>
.text

#define STATE1	%xmm0
#define STATE2	%xmm4
#define STATE3	%xmm5
#define STATE4	%xmm6
#define STATE	STATE1
#define IN1	%xmm1
#define IN2	%xmm7
#define IN3	%xmm8
#define IN4	%xmm9
#define IN	IN1
#define KEY	%xmm2
#define IV	%xmm3

#define BSWAP_MASK %xmm10
#define CTR	%xmm11
#define INC	%xmm12

#ifdef __x86_64__
#define AREG	%rax
#define KEYP	%rdi
#define OUTP	%rsi
#define UKEYP	OUTP
#define INP	%rdx
#define LEN	%rcx
#define IVP	%r8
#define KLEN	%r9d
#define T1	%r10
#define TKEYP	T1
#define T2	%r11
#define TCTR_LOW T2
#else
#define AREG	%eax
#define KEYP	%edi
#define OUTP	AREG
#define UKEYP	OUTP
#define INP	%edx
#define LEN	%esi
#define IVP	%ebp
#define KLEN	%ebx
#define T1	%ecx
#define TKEYP	T1
#endif

_key_expansion_128:
_key_expansion_256a:
	pshufd $0b11111111, %xmm1, %xmm1
	shufps $0b00010000, %xmm0, %xmm4
	pxor %xmm4, %xmm0
	shufps $0b10001100, %xmm0, %xmm4
	pxor %xmm4, %xmm0
	pxor %xmm1, %xmm0
	movaps %xmm0, (TKEYP)
	add $0x10, TKEYP
	ret

.align 4
_key_expansion_192a:
	pshufd $0b01010101, %xmm1, %xmm1
	shufps $0b00010000, %xmm0, %xmm4
	pxor %xmm4, %xmm0
	shufps $0b10001100, %xmm0, %xmm4
	pxor %xmm4, %xmm0
	pxor %xmm1, %xmm0

	movaps %xmm2, %xmm5
	movaps %xmm2, %xmm6
	pslldq $4, %xmm5
	pshufd $0b11111111, %xmm0, %xmm3
	pxor %xmm3, %xmm2
	pxor %xmm5, %xmm2

	movaps %xmm0, %xmm1
	shufps $0b01000100, %xmm0, %xmm6
	movaps %xmm6, (TKEYP)
	shufps $0b01001110, %xmm2, %xmm1
	movaps %xmm1, 0x10(TKEYP)
	add $0x20, TKEYP
	ret

.align 4
_key_expansion_192b:
	pshufd $0b01010101, %xmm1, %xmm1
	shufps $0b00010000, %xmm0, %xmm4
	pxor %xmm4, %xmm0
	shufps $0b10001100, %xmm0, %xmm4
	pxor %xmm4, %xmm0
	pxor %xmm1, %xmm0

	movaps %xmm2, %xmm5
	pslldq $4, %xmm5
	pshufd $0b11111111, %xmm0, %xmm3
	pxor %xmm3, %xmm2
	pxor %xmm5, %xmm2

	movaps %xmm0, (TKEYP)
	add $0x10, TKEYP
	ret

.align 4
_key_expansion_256b:
	pshufd $0b10101010, %xmm1, %xmm1
	shufps $0b00010000, %xmm2, %xmm4
	pxor %xmm4, %xmm2
	shufps $0b10001100, %xmm2, %xmm4
	pxor %xmm4, %xmm2
	pxor %xmm1, %xmm2
	movaps %xmm2, (TKEYP)
	add $0x10, TKEYP
	ret

/*
 * int aesni_set_key(struct crypto_aes_ctx *ctx, const u8 *in_key,
 *                   unsigned int key_len)
 */
ENTRY(aesni_set_key)
#ifndef __x86_64__
	pushl KEYP
	movl 8(%esp), KEYP		# ctx
	movl 12(%esp), UKEYP		# in_key
	movl 16(%esp), %edx		# key_len
#endif
	movups (UKEYP), %xmm0		# user key (first 16 bytes)
	movaps %xmm0, (KEYP)
	lea 0x10(KEYP), TKEYP		# key addr
	movl %edx, 480(KEYP)
	pxor %xmm4, %xmm4		# xmm4 is assumed 0 in _key_expansion_x
	cmp $24, %dl
	jb .Lenc_key128
	je .Lenc_key192
	movups 0x10(UKEYP), %xmm2	# other user key
	movaps %xmm2, (TKEYP)
	add $0x10, TKEYP
	AESKEYGENASSIST 0x1 %xmm2 %xmm1		# round 1
	call _key_expansion_256a
	AESKEYGENASSIST 0x1 %xmm0 %xmm1
	call _key_expansion_256b
	AESKEYGENASSIST 0x2 %xmm2 %xmm1		# round 2
	call _key_expansion_256a
	AESKEYGENASSIST 0x2 %xmm0 %xmm1
	call _key_expansion_256b
	AESKEYGENASSIST 0x4 %xmm2 %xmm1		# round 3
	call _key_expansion_256a
	AESKEYGENASSIST 0x4 %xmm0 %xmm1
	call _key_expansion_256b
	AESKEYGENASSIST 0x8 %xmm2 %xmm1		# round 4
	call _key_expansion_256a
	AESKEYGENASSIST 0x8 %xmm0 %xmm1
	call _key_expansion_256b
	AESKEYGENASSIST 0x10 %xmm2 %xmm1	# round 5
	call _key_expansion_256a
	AESKEYGENASSIST 0x10 %xmm0 %xmm1
	call _key_expansion_256b
	AESKEYGENASSIST 0x20 %xmm2 %xmm1	# round 6
	call _key_expansion_256a
	AESKEYGENASSIST 0x20 %xmm0 %xmm1
	call _key_expansion_256b
	AESKEYGENASSIST 0x40 %xmm2 %xmm1	# round 7
	call _key_expansion_256a
	jmp .Ldec_key
.Lenc_key192:
	movq 0x10(UKEYP), %xmm2		# other user key
	AESKEYGENASSIST 0x1 %xmm2 %xmm1		# round 1
	call _key_expansion_192a
	AESKEYGENASSIST 0x2 %xmm2 %xmm1		# round 2
	call _key_expansion_192b
	AESKEYGENASSIST 0x4 %xmm2 %xmm1		# round 3
	call _key_expansion_192a
	AESKEYGENASSIST 0x8 %xmm2 %xmm1		# round 4
	call _key_expansion_192b
	AESKEYGENASSIST 0x10 %xmm2 %xmm1	# round 5
	call _key_expansion_192a
	AESKEYGENASSIST 0x20 %xmm2 %xmm1	# round 6
	call _key_expansion_192b
	AESKEYGENASSIST 0x40 %xmm2 %xmm1	# round 7
	call _key_expansion_192a
	AESKEYGENASSIST 0x80 %xmm2 %xmm1	# round 8
	call _key_expansion_192b
	jmp .Ldec_key
.Lenc_key128:
	AESKEYGENASSIST 0x1 %xmm0 %xmm1		# round 1
	call _key_expansion_128
	AESKEYGENASSIST 0x2 %xmm0 %xmm1		# round 2
	call _key_expansion_128
	AESKEYGENASSIST 0x4 %xmm0 %xmm1		# round 3
	call _key_expansion_128
	AESKEYGENASSIST 0x8 %xmm0 %xmm1		# round 4
	call _key_expansion_128
	AESKEYGENASSIST 0x10 %xmm0 %xmm1	# round 5
	call _key_expansion_128
	AESKEYGENASSIST 0x20 %xmm0 %xmm1	# round 6
	call _key_expansion_128
	AESKEYGENASSIST 0x40 %xmm0 %xmm1	# round 7
	call _key_expansion_128
	AESKEYGENASSIST 0x80 %xmm0 %xmm1	# round 8
	call _key_expansion_128
	AESKEYGENASSIST 0x1b %xmm0 %xmm1	# round 9
	call _key_expansion_128
	AESKEYGENASSIST 0x36 %xmm0 %xmm1	# round 10
	call _key_expansion_128
.Ldec_key:
	sub $0x10, TKEYP
	movaps (KEYP), %xmm0
	movaps (TKEYP), %xmm1
	movaps %xmm0, 240(TKEYP)
	movaps %xmm1, 240(KEYP)
	add $0x10, KEYP
	lea 240-16(TKEYP), UKEYP
.align 4
.Ldec_key_loop:
	movaps (KEYP), %xmm0
	AESIMC %xmm0 %xmm1
	movaps %xmm1, (UKEYP)
	add $0x10, KEYP
	sub $0x10, UKEYP
	cmp TKEYP, KEYP
	jb .Ldec_key_loop
	xor AREG, AREG
#ifndef __x86_64__
	popl KEYP
#endif
	ret

/*
 * void aesni_enc(struct crypto_aes_ctx *ctx, u8 *dst, const u8 *src)
 */
ENTRY(aesni_enc)
#ifndef __x86_64__
	pushl KEYP
	pushl KLEN
	movl 12(%esp), KEYP
	movl 16(%esp), OUTP
	movl 20(%esp), INP
#endif
	movl 480(KEYP), KLEN		# key length
	movups (INP), STATE		# input
	call _aesni_enc1
	movups STATE, (OUTP)		# output
#ifndef __x86_64__
	popl KLEN
	popl KEYP
#endif
	ret

/*
 * _aesni_enc1:		internal ABI
 * input:
 *	KEYP:		key struct pointer
 *	KLEN:		round count
 *	STATE:		initial state (input)
 * output:
 *	STATE:		finial state (output)
 * changed:
 *	KEY
 *	TKEYP (T1)
 */
.align 4
_aesni_enc1:
	movaps (KEYP), KEY		# key
	mov KEYP, TKEYP
	pxor KEY, STATE		# round 0
	add $0x30, TKEYP
	cmp $24, KLEN
	jb .Lenc128
	lea 0x20(TKEYP), TKEYP
	je .Lenc192
	add $0x20, TKEYP
	movaps -0x60(TKEYP), KEY
	AESENC KEY STATE
	movaps -0x50(TKEYP), KEY
	AESENC KEY STATE
.align 4
.Lenc192:
	movaps -0x40(TKEYP), KEY
	AESENC KEY STATE
	movaps -0x30(TKEYP), KEY
	AESENC KEY STATE
.align 4
.Lenc128:
	movaps -0x20(TKEYP), KEY
	AESENC KEY STATE
	movaps -0x10(TKEYP), KEY
	AESENC KEY STATE
	movaps (TKEYP), KEY
	AESENC KEY STATE
	movaps 0x10(TKEYP), KEY
	AESENC KEY STATE
	movaps 0x20(TKEYP), KEY
	AESENC KEY STATE
	movaps 0x30(TKEYP), KEY
	AESENC KEY STATE
	movaps 0x40(TKEYP), KEY
	AESENC KEY STATE
	movaps 0x50(TKEYP), KEY
	AESENC KEY STATE
	movaps 0x60(TKEYP), KEY
	AESENC KEY STATE
	movaps 0x70(TKEYP), KEY
	AESENCLAST KEY STATE
	ret

/*
 * _aesni_enc4:	internal ABI
 * input:
 *	KEYP:		key struct pointer
 *	KLEN:		round count
 *	STATE1:		initial state (input)
 *	STATE2
 *	STATE3
 *	STATE4
 * output:
 *	STATE1:		finial state (output)
 *	STATE2
 *	STATE3
 *	STATE4
 * changed:
 *	KEY
 *	TKEYP (T1)
 */
.align 4
_aesni_enc4:
	movaps (KEYP), KEY		# key
	mov KEYP, TKEYP
	pxor KEY, STATE1		# round 0
	pxor KEY, STATE2
	pxor KEY, STATE3
	pxor KEY, STATE4
	add $0x30, TKEYP
	cmp $24, KLEN
	jb .L4enc128
	lea 0x20(TKEYP), TKEYP
	je .L4enc192
	add $0x20, TKEYP
	movaps -0x60(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps -0x50(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
#.align 4
.L4enc192:
	movaps -0x40(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps -0x30(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
#.align 4
.L4enc128:
	movaps -0x20(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps -0x10(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps (TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x10(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x20(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x30(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x40(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x50(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x60(TKEYP), KEY
	AESENC KEY STATE1
	AESENC KEY STATE2
	AESENC KEY STATE3
	AESENC KEY STATE4
	movaps 0x70(TKEYP), KEY
	AESENCLAST KEY STATE1		# last round
	AESENCLAST KEY STATE2
	AESENCLAST KEY STATE3
	AESENCLAST KEY STATE4
	ret

/*
 * void aesni_dec (struct crypto_aes_ctx *ctx, u8 *dst, const u8 *src)
 */
ENTRY(aesni_dec)
#ifndef __x86_64__
	pushl KEYP
	pushl KLEN
	movl 12(%esp), KEYP
	movl 16(%esp), OUTP
	movl 20(%esp), INP
#endif
	mov 480(KEYP), KLEN		# key length
	add $240, KEYP
	movups (INP), STATE		# input
	call _aesni_dec1
	movups STATE, (OUTP)		#output
#ifndef __x86_64__
	popl KLEN
	popl KEYP
#endif
	ret

/*
 * _aesni_dec1:		internal ABI
 * input:
 *	KEYP:		key struct pointer
 *	KLEN:		key length
 *	STATE:		initial state (input)
 * output:
 *	STATE:		finial state (output)
 * changed:
 *	KEY
 *	TKEYP (T1)
 */
.align 4
_aesni_dec1:
	movaps (KEYP), KEY		# key
	mov KEYP, TKEYP
	pxor KEY, STATE		# round 0
	add $0x30, TKEYP
	cmp $24, KLEN
	jb .Ldec128
	lea 0x20(TKEYP), TKEYP
	je .Ldec192
	add $0x20, TKEYP
	movaps -0x60(TKEYP), KEY
	AESDEC KEY STATE
	movaps -0x50(TKEYP), KEY
	AESDEC KEY STATE
.align 4
.Ldec192:
	movaps -0x40(TKEYP), KEY
	AESDEC KEY STATE
	movaps -0x30(TKEYP), KEY
	AESDEC KEY STATE
.align 4
.Ldec128:
	movaps -0x20(TKEYP), KEY
	AESDEC KEY STATE
	movaps -0x10(TKEYP), KEY
	AESDEC KEY STATE
	movaps (TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x10(TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x20(TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x30(TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x40(TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x50(TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x60(TKEYP), KEY
	AESDEC KEY STATE
	movaps 0x70(TKEYP), KEY
	AESDECLAST KEY STATE
	ret

/*
 * _aesni_dec4:	internal ABI
 * input:
 *	KEYP:		key struct pointer
 *	KLEN:		key length
 *	STATE1:		initial state (input)
 *	STATE2
 *	STATE3
 *	STATE4
 * output:
 *	STATE1:		finial state (output)
 *	STATE2
 *	STATE3
 *	STATE4
 * changed:
 *	KEY
 *	TKEYP (T1)
 */
.align 4
_aesni_dec4:
	movaps (KEYP), KEY		# key
	mov KEYP, TKEYP
	pxor KEY, STATE1		# round 0
	pxor KEY, STATE2
	pxor KEY, STATE3
	pxor KEY, STATE4
	add $0x30, TKEYP
	cmp $24, KLEN
	jb .L4dec128
	lea 0x20(TKEYP), TKEYP
	je .L4dec192
	add $0x20, TKEYP
	movaps -0x60(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps -0x50(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
.align 4
.L4dec192:
	movaps -0x40(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps -0x30(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
.align 4
.L4dec128:
	movaps -0x20(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps -0x10(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps (TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x10(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x20(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x30(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x40(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x50(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x60(TKEYP), KEY
	AESDEC KEY STATE1
	AESDEC KEY STATE2
	AESDEC KEY STATE3
	AESDEC KEY STATE4
	movaps 0x70(TKEYP), KEY
	AESDECLAST KEY STATE1		# last round
	AESDECLAST KEY STATE2
	AESDECLAST KEY STATE3
	AESDECLAST KEY STATE4
	ret

/*
 * void aesni_ecb_enc(struct crypto_aes_ctx *ctx, const u8 *dst, u8 *src,
 *		      size_t len)
 */
ENTRY(aesni_ecb_enc)
#ifndef __x86_64__
	pushl LEN
	pushl KEYP
	pushl KLEN
	movl 16(%esp), KEYP
	movl 20(%esp), OUTP
	movl 24(%esp), INP
	movl 28(%esp), LEN
#endif
	test LEN, LEN		# check length
	jz .Lecb_enc_ret
	mov 480(KEYP), KLEN
	cmp $16, LEN
	jb .Lecb_enc_ret
	cmp $64, LEN
	jb .Lecb_enc_loop1
.align 4
.Lecb_enc_loop4:
	movups (INP), STATE1
	movups 0x10(INP), STATE2
	movups 0x20(INP), STATE3
	movups 0x30(INP), STATE4
	call _aesni_enc4
	movups STATE1, (OUTP)
	movups STATE2, 0x10(OUTP)
	movups STATE3, 0x20(OUTP)
	movups STATE4, 0x30(OUTP)
	sub $64, LEN
	add $64, INP
	add $64, OUTP
	cmp $64, LEN
	jge .Lecb_enc_loop4
	cmp $16, LEN
	jb .Lecb_enc_ret
.align 4
.Lecb_enc_loop1:
	movups (INP), STATE1
	call _aesni_enc1
	movups STATE1, (OUTP)
	sub $16, LEN
	add $16, INP
	add $16, OUTP
	cmp $16, LEN
	jge .Lecb_enc_loop1
.Lecb_enc_ret:
#ifndef __x86_64__
	popl KLEN
	popl KEYP
	popl LEN
#endif
	ret

/*
 * void aesni_ecb_dec(struct crypto_aes_ctx *ctx, const u8 *dst, u8 *src,
 *		      size_t len);
 */
ENTRY(aesni_ecb_dec)
#ifndef __x86_64__
	pushl LEN
	pushl KEYP
	pushl KLEN
	movl 16(%esp), KEYP
	movl 20(%esp), OUTP
	movl 24(%esp), INP
	movl 28(%esp), LEN
#endif
	test LEN, LEN
	jz .Lecb_dec_ret
	mov 480(KEYP), KLEN
	add $240, KEYP
	cmp $16, LEN
	jb .Lecb_dec_ret
	cmp $64, LEN
	jb .Lecb_dec_loop1
.align 4
.Lecb_dec_loop4:
	movups (INP), STATE1
	movups 0x10(INP), STATE2
	movups 0x20(INP), STATE3
	movups 0x30(INP), STATE4
	call _aesni_dec4
	movups STATE1, (OUTP)
	movups STATE2, 0x10(OUTP)
	movups STATE3, 0x20(OUTP)
	movups STATE4, 0x30(OUTP)
	sub $64, LEN
	add $64, INP
	add $64, OUTP
	cmp $64, LEN
	jge .Lecb_dec_loop4
	cmp $16, LEN
	jb .Lecb_dec_ret
.align 4
.Lecb_dec_loop1:
	movups (INP), STATE1
	call _aesni_dec1
	movups STATE1, (OUTP)
	sub $16, LEN
	add $16, INP
	add $16, OUTP
	cmp $16, LEN
	jge .Lecb_dec_loop1
.Lecb_dec_ret:
#ifndef __x86_64__
	popl KLEN
	popl KEYP
	popl LEN
#endif
	ret

/*
 * void aesni_cbc_enc(struct crypto_aes_ctx *ctx, const u8 *dst, u8 *src,
 *		      size_t len, u8 *iv)
 */
ENTRY(aesni_cbc_enc)
#ifndef __x86_64__
	pushl IVP
	pushl LEN
	pushl KEYP
	pushl KLEN
	movl 20(%esp), KEYP
	movl 24(%esp), OUTP
	movl 28(%esp), INP
	movl 32(%esp), LEN
	movl 36(%esp), IVP
#endif
	cmp $16, LEN
	jb .Lcbc_enc_ret
	mov 480(KEYP), KLEN
	movups (IVP), STATE	# load iv as initial state
.align 4
.Lcbc_enc_loop:
	movups (INP), IN	# load input
	pxor IN, STATE
	call _aesni_enc1
	movups STATE, (OUTP)	# store output
	sub $16, LEN
	add $16, INP
	add $16, OUTP
	cmp $16, LEN
	jge .Lcbc_enc_loop
	movups STATE, (IVP)
.Lcbc_enc_ret:
#ifndef __x86_64__
	popl KLEN
	popl KEYP
	popl LEN
	popl IVP
#endif
	ret

/*
 * void aesni_cbc_dec(struct crypto_aes_ctx *ctx, const u8 *dst, u8 *src,
 *		      size_t len, u8 *iv)
 */
ENTRY(aesni_cbc_dec)
#ifndef __x86_64__
	pushl IVP
	pushl LEN
	pushl KEYP
	pushl KLEN
	movl 20(%esp), KEYP
	movl 24(%esp), OUTP
	movl 28(%esp), INP
	movl 32(%esp), LEN
	movl 36(%esp), IVP
#endif
	cmp $16, LEN
	jb .Lcbc_dec_just_ret
	mov 480(KEYP), KLEN
	add $240, KEYP
	movups (IVP), IV
	cmp $64, LEN
	jb .Lcbc_dec_loop1
.align 4
.Lcbc_dec_loop4:
	movups (INP), IN1
	movaps IN1, STATE1
	movups 0x10(INP), IN2
	movaps IN2, STATE2
#ifdef __x86_64__
	movups 0x20(INP), IN3
	movaps IN3, STATE3
	movups 0x30(INP), IN4
	movaps IN4, STATE4
#else
	movups 0x20(INP), IN1
	movaps IN1, STATE3
	movups 0x30(INP), IN2
	movaps IN2, STATE4
#endif
	call _aesni_dec4
	pxor IV, STATE1
#ifdef __x86_64__
	pxor IN1, STATE2
	pxor IN2, STATE3
	pxor IN3, STATE4
	movaps IN4, IV
#else
	pxor (INP), STATE2
	pxor 0x10(INP), STATE3
	pxor IN1, STATE4
	movaps IN2, IV
#endif
	movups STATE1, (OUTP)
	movups STATE2, 0x10(OUTP)
	movups STATE3, 0x20(OUTP)
	movups STATE4, 0x30(OUTP)
	sub $64, LEN
	add $64, INP
	add $64, OUTP
	cmp $64, LEN
	jge .Lcbc_dec_loop4
	cmp $16, LEN
	jb .Lcbc_dec_ret
.align 4
.Lcbc_dec_loop1:
	movups (INP), IN
	movaps IN, STATE
	call _aesni_dec1
	pxor IV, STATE
	movups STATE, (OUTP)
	movaps IN, IV
	sub $16, LEN
	add $16, INP
	add $16, OUTP
	cmp $16, LEN
	jge .Lcbc_dec_loop1
.Lcbc_dec_ret:
	movups IV, (IVP)
.Lcbc_dec_just_ret:
#ifndef __x86_64__
	popl KLEN
	popl KEYP
	popl LEN
	popl IVP
#endif
	ret

#ifdef __x86_64__
.align 16
.Lbswap_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/*
 * _aesni_inc_init:	internal ABI
 *	setup registers used by _aesni_inc
 * input:
 *	IV
 * output:
 *	CTR:	== IV, in little endian
 *	TCTR_LOW: == lower qword of CTR
 *	INC:	== 1, in little endian
 *	BSWAP_MASK == endian swapping mask
 */
.align 4
_aesni_inc_init:
	movaps .Lbswap_mask, BSWAP_MASK
	movaps IV, CTR
	PSHUFB_XMM BSWAP_MASK CTR
	mov $1, TCTR_LOW
	MOVQ_R64_XMM TCTR_LOW INC
	MOVQ_R64_XMM CTR TCTR_LOW
	ret

/*
 * _aesni_inc:		internal ABI
 *	Increase IV by 1, IV is in big endian
 * input:
 *	IV
 *	CTR:	== IV, in little endian
 *	TCTR_LOW: == lower qword of CTR
 *	INC:	== 1, in little endian
 *	BSWAP_MASK == endian swapping mask
 * output:
 *	IV:	Increase by 1
 * changed:
 *	CTR:	== output IV, in little endian
 *	TCTR_LOW: == lower qword of CTR
 */
.align 4
_aesni_inc:
	paddq INC, CTR
	add $1, TCTR_LOW
	jnc .Linc_low
	pslldq $8, INC
	paddq INC, CTR
	psrldq $8, INC
.Linc_low:
	movaps CTR, IV
	PSHUFB_XMM BSWAP_MASK IV
	ret

/*
 * void aesni_ctr_enc(struct crypto_aes_ctx *ctx, const u8 *dst, u8 *src,
 *		      size_t len, u8 *iv)
 */
ENTRY(aesni_ctr_enc)
	cmp $16, LEN
	jb .Lctr_enc_just_ret
	mov 480(KEYP), KLEN
	movups (IVP), IV
	call _aesni_inc_init
	cmp $64, LEN
	jb .Lctr_enc_loop1
.align 4
.Lctr_enc_loop4:
	movaps IV, STATE1
	call _aesni_inc
	movups (INP), IN1
	movaps IV, STATE2
	call _aesni_inc
	movups 0x10(INP), IN2
	movaps IV, STATE3
	call _aesni_inc
	movups 0x20(INP), IN3
	movaps IV, STATE4
	call _aesni_inc
	movups 0x30(INP), IN4
	call _aesni_enc4
	pxor IN1, STATE1
	movups STATE1, (OUTP)
	pxor IN2, STATE2
	movups STATE2, 0x10(OUTP)
	pxor IN3, STATE3
	movups STATE3, 0x20(OUTP)
	pxor IN4, STATE4
	movups STATE4, 0x30(OUTP)
	sub $64, LEN
	add $64, INP
	add $64, OUTP
	cmp $64, LEN
	jge .Lctr_enc_loop4
	cmp $16, LEN
	jb .Lctr_enc_ret
.align 4
.Lctr_enc_loop1:
	movaps IV, STATE
	call _aesni_inc
	movups (INP), IN
	call _aesni_enc1
	pxor IN, STATE
	movups STATE, (OUTP)
	sub $16, LEN
	add $16, INP
	add $16, OUTP
	cmp $16, LEN
	jge .Lctr_enc_loop1
.Lctr_enc_ret:
	movups IV, (IVP)
.Lctr_enc_just_ret:
	ret
#endif
