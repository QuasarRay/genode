/*
 * \brief   Startup code for bootloader
 * \author  Alexander Boettcher
 * \date    2026-06-03
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


.section ".text.crt0"

	/* multi-boot 1 header */
	__mbi1_start:

	.long   0x1badb002 /* MBI 1 magic value */
	.long   0x7 /* bit 0: align 4k, bit 1: memory info, bit 2: video info */
	.long -(0x1badb002 + 0x7) /* checksum */
	.long   0 /* unused, since feature  bit 16 is unset */
	.long   0 /* unused, since feature  bit 16 is unset */
	.long   0 /* unused, since feature  bit 16 is unset */
	.long   0 /* unused, since feature  bit 16 is unset */
	.long   0 /* unused, since feature  bit 16 is unset */
	.long   0 /* preferred mode: linear */
	.long   0 /* preferred width  */
	.long   0 /* preferred height */
	.long  32 /* preferred depth  */

	._mbi1_end:

	.align 8

	/* multi-boot 2 header */
	__mbi2_start:

	.long   0xe85250d6 /* MBI2 magic value */
	.long   0x0 /* flags */
	.long  (__mbi2_end - __mbi2_start) /* size */
	.long -(0xe85250d6 + (__mbi2_end - __mbi2_start)) /* checksum */

	/* framebuffer */
	.word   5 /* type  */
	.word   0 /* flags */
	.long  20 /* size  */
	.long   0 /* preferred width  */
	.long   0 /* preferred height */
	.long  32 /* preferred depth  */

	.align  8

	/* align modules to page */
	.word   6 /* type */
	.word   0 /* flags */
	.long   8 /* size */

	.align  8

	/* relocatable tag */
	.word  10          /* type  */
	.word   0          /* flags */
	.long  24          /* size  */
	.long   0x02000000 /* min_addr */
	.long   0xc0000000 /* max_addr  -  3.0 GB */
	.long   0x01000000 /* alignment -   16 MB */
	.long   0          /* preference */

	.align  8

	/* end tag */
	.word   0 /* type */
	.word   0 /* flags */
	.long   8 /* size */

	__mbi2_end:

.code32

	.global _start
	_start:

	/* calculate relocation offset */
	mov   %ebx, %esp     /* use  MBI data as stack, temporarily */
	pop   %esi           /* save MBI data size in register esi */
	call  .get_ip        /* make a call, to get the instruction pointer */
.get_ip:
	pop   %ebp           /* save the instruction pointer in register ebp */
	push  %esi           /* re-store MBI data size */
	mov   %ebp, %ecx     /* save ip in ecx */
	sub   $.get_ip, %ebp /* calculate offset */

	mov $__stack, %edx /* prepare stack */
	add %ebp    , %edx
	mov %edx    , %esp

	push %eax /* save MBI magic on stack */

	/*
	 * Populate page table with 1:1 mapping up to 4G
	 */
	mov $__pagetable, %edx /* prepare page table */
	add %ebp        , %edx

	xor %edi , %edi
	mov $0x87, %eax /* 4 MB page (7), present(1), writeable(2), user(4) */
	mov %edx , %esi
1:
	mov %eax, (%esi)    /* store entry in page table */
	add $4, %esi        /* + 4 byte, next page table entry */
	add $0x400000, %eax /* + 4 MB physical address */
	inc %edi            /* next entry id */
	cmp $1024, %edi     /* 1024 entries on 32bit with 4MB super pages */
	jb  1b              /* done after 1024 entries */

	/*
	 * Set link address as virtual address into page table
	 */

	/* calculate page table entry */
	mov  $LD_TEXT_ADDR, %esi /* link address == virtual address */
	shr            $22, %esi /* right shift 4MB, 10 + 12 bits */
	imul            $4, %esi /* multiply by page table entry size */
	add           %edx, %esi /* add pagetable base */

	mov %ecx       , %edi /* get instruction pointer */
	and $0xffc00000, %edi /* calculate 4M aligned load base */
	add $0x87      , %edi /* add rights, 4 MB page, present, writeable, user */

	mov %edi, (%esi) /* write page table entry */

	/*
	 * Setup paging
	 */

	mov %edx, %cr3   /* load page table pointer */

	/* enable PSE (page size extension for 4MB super page usage) */
	movl $0x10, %ecx
	movl %ecx, %cr4

	/* enable paging, write protection and caching */
	xorl %ecx, %ecx
	btsl   $0, %ecx /* protected mode */
	btsl  $16, %ecx /* write protect */
	btsl  $31, %ecx /* paging */
	movl %ecx, %cr0 /* paging is now active */

	sub %ebp, %esp  /* adjust stack to now paged 1:1 link region */

	/* adjust instruction pointer to now 1:1 paged link region */
	xor %eax, %eax
	call .switch_to_link_addr
.switch_to_link_addr:
	jnz   .switch_to_link_addr_done
	pop   %eax
	sub   %ebp, %eax
	push  %eax
	ret
.switch_to_link_addr_done:

	/*
	 * Call C++ high level code with register regparm 3 layout:
	 *
	 * - eax - MBI magic value
	 * - edx - MBI data pointer
	 * - ecx - relocation offset
	 */
	pop %eax
	mov %ebx, %edx
	mov %ebp, %ecx
	call entry
2:
	ud2 /* undefined opcode - die/reboot on unexpected return */
	jb 2b


.globl switch_to_boot_ip_and_stack
switch_to_boot_ip_and_stack:
	/* eax has to contain relocation offset due to regparm 1 layout */

	add %eax, %esp /* adjust stack */

	/* adjust return address by relocation offset */
	pop  %edx
	add  %eax, %edx
	push %edx
	ret


.bss
	.p2align 12

	.space 4096 * 32
	.globl __stack
	__stack:

	.p2align 12

	__pagetable:
	.space 4096
