	.file	"test.generated.c"
	.text
	.globl	sum_list                        // -- Begin function sum_list
	.p2align	2
	.type	sum_list,@function
sum_list:                               // @sum_list
	.cfi_startproc
// %bb.0:
	stp	x29, x30, [sp, #-80]!           // 16-byte Folded Spill
	.cfi_def_cfa_offset 80
	str	x25, [sp, #16]                  // 8-byte Folded Spill
	stp	x24, x23, [sp, #32]             // 16-byte Folded Spill
	stp	x22, x21, [sp, #48]             // 16-byte Folded Spill
	stp	x20, x19, [sp, #64]             // 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 80
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -40
	.cfi_offset w24, -48
	.cfi_offset w25, -64
	.cfi_offset w30, -72
	.cfi_offset w29, -80
	.cfi_remember_state
	mov	x23, x0
	mov	w0, #32                         // =0x20
	mov	x21, x2
	mov	x22, x1
	bl	malloc
	cbz	x0, .LBB0_12
// %bb.1:
	mov	w8, #3                          // =0x3
	mov	x19, x0
	dup	v0.2d, x8
	mov	w8, #8                          // =0x8
	str	x8, [x0, #24]
	stur	q0, [x0, #8]
	mov	w0, #24                         // =0x18
	bl	malloc
	str	x0, [x19]
	cbz	x0, .LBB0_13
// %bb.2:
	adrp	x24, clair_rt_list_cleanup_registered
	mov	x20, x0
	stp	x23, x22, [x0]
	ldrb	w8, [x24, :lo12:clair_rt_list_cleanup_registered]
	str	x21, [x0, #16]
	tbnz	w8, #0, .LBB0_5
// %bb.3:
	adrp	x0, clair_rt_cleanup_lists
	add	x0, x0, :lo12:clair_rt_cleanup_lists
	bl	atexit
	cbnz	w0, .LBB0_14
// %bb.4:
	mov	w8, #1                          // =0x1
	strb	w8, [x24, :lo12:clair_rt_list_cleanup_registered]
.LBB0_5:
	adrp	x21, clair_rt_list_count
	adrp	x23, clair_rt_list_capacity
	ldr	x22, [x21, :lo12:clair_rt_list_count]
	ldr	x8, [x23, :lo12:clair_rt_list_capacity]
	cmp	x22, x8
	b.ne	.LBB0_10
// %bb.6:
	lsl	x8, x22, #1
	cmp	x22, #0
	mov	w9, #16                         // =0x10
	csel	x24, x9, x8, eq
	cmp	x24, x22
	b.lo	.LBB0_15
// %bb.7:
	lsr	x8, x24, #61
	cbnz	x8, .LBB0_15
// %bb.8:
	adrp	x25, clair_rt_lists
	lsl	x1, x24, #3
	ldr	x0, [x25, :lo12:clair_rt_lists]
	bl	realloc
	cbz	x0, .LBB0_16
// %bb.9:
	mov	x8, x0
	str	x0, [x25, :lo12:clair_rt_lists]
	str	x24, [x23, :lo12:clair_rt_list_capacity]
	b	.LBB0_11
.LBB0_10:
	adrp	x8, clair_rt_lists
	ldr	x8, [x8, :lo12:clair_rt_lists]
.LBB0_11:
	ldp	x9, x10, [x20]
	add	x12, x22, #1
	ldr	x11, [x20, #16]
	str	x12, [x21, :lo12:clair_rt_list_count]
	add	x9, x10, x9
	add	x0, x9, x11
	str	x19, [x8, x22, lsl #3]
	.cfi_def_cfa wsp, 80
	ldp	x20, x19, [sp, #64]             // 16-byte Folded Reload
	ldr	x25, [sp, #16]                  // 8-byte Folded Reload
	ldp	x22, x21, [sp, #48]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #80             // 16-byte Folded Reload
	.cfi_def_cfa_offset 0
	.cfi_restore w19
	.cfi_restore w20
	.cfi_restore w21
	.cfi_restore w22
	.cfi_restore w23
	.cfi_restore w24
	.cfi_restore w25
	.cfi_restore w30
	.cfi_restore w29
	ret
.LBB0_12:
	.cfi_restore_state
	adrp	x8, :got:stderr
	adrp	x0, .L.str.2
	add	x0, x0, :lo12:.L.str.2
	ldr	x8, [x8, :got_lo12:stderr]
	mov	w1, #55                         // =0x37
	mov	w2, #1                          // =0x1
	ldr	x3, [x8]
	bl	fwrite
	mov	w0, #1                          // =0x1
	bl	exit
.LBB0_13:
	adrp	x0, .L.str.4
	add	x0, x0, :lo12:.L.str.4
	mov	x1, x19
	bl	clair_rt_list_runtime_error
.LBB0_14:
	adrp	x0, .L.str.6
	add	x0, x0, :lo12:.L.str.6
	mov	x1, x19
	bl	clair_rt_list_runtime_error
.LBB0_15:
	adrp	x0, .L.str.7
	add	x0, x0, :lo12:.L.str.7
	mov	x1, x19
	bl	clair_rt_list_runtime_error
.LBB0_16:
	adrp	x0, .L.str.8
	add	x0, x0, :lo12:.L.str.8
	mov	x1, x19
	bl	clair_rt_list_runtime_error
.Lfunc_end0:
	.size	sum_list, .Lfunc_end0-sum_list
	.cfi_endproc
                                        // -- End function
	.globl	main                            // -- Begin function main
	.p2align	2
	.type	main,@function
main:                                   // @main
	.cfi_startproc
// %bb.0:
	stp	x29, x30, [sp, #-32]!           // 16-byte Folded Spill
	.cfi_def_cfa_offset 32
	stp	x20, x19, [sp, #16]             // 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 32
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w30, -24
	.cfi_offset w29, -32
	bl	clair_rt_input_int
	mov	x19, x0
	bl	clair_rt_input_int
	mov	x20, x0
	bl	clair_rt_input_int
	mov	x2, x0
	mov	x0, x19
	mov	x1, x20
	bl	sum_list
	mov	x1, x0
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	bl	printf
	mov	w0, wzr
	.cfi_def_cfa wsp, 32
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	.cfi_def_cfa_offset 0
	.cfi_restore w19
	.cfi_restore w20
	.cfi_restore w30
	.cfi_restore w29
	ret
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc
                                        // -- End function
	.p2align	2                               // -- Begin function clair_rt_input_int
	.type	clair_rt_input_int,@function
clair_rt_input_int:                     // @clair_rt_input_int
	.cfi_startproc
// %bb.0:
	sub	sp, sp, #304
	.cfi_def_cfa_offset 304
	stp	x29, x30, [sp, #272]            // 16-byte Folded Spill
	stp	x28, x19, [sp, #288]            // 16-byte Folded Spill
	add	x29, sp, #272
	.cfi_def_cfa w29, 32
	.cfi_offset w19, -8
	.cfi_offset w28, -16
	.cfi_offset w30, -24
	.cfi_offset w29, -32
	.cfi_remember_state
	adrp	x8, :got:stdin
	add	x0, sp, #16
	mov	w1, #256                        // =0x100
	ldr	x8, [x8, :got_lo12:stdin]
	ldr	x2, [x8]
	bl	fgets
	cbz	x0, .LBB2_9
// %bb.1:
	bl	__errno
	mov	x19, x0
	str	wzr, [x0]
	add	x0, sp, #16
	add	x1, sp, #8
	mov	w2, #10                         // =0xa
	str	xzr, [sp, #8]
	bl	strtoimax
	ldr	x8, [sp, #8]
	mov	x10, #9728                      // =0x2600
	mov	w9, #1                          // =0x1
	movk	x10, #1, lsl #32
.LBB2_2:                                // =>This Inner Loop Header: Depth=1
	ldrb	w11, [x8]
	lsl	x12, x9, x11
	cmp	w11, #32
	and	x12, x12, x10
	ccmp	x12, #0, #4, ls
	b.eq	.LBB2_4
// %bb.3:                               //   in Loop: Header=BB2_2 Depth=1
	add	x8, x8, #1
	str	x8, [sp, #8]
	b	.LBB2_2
.LBB2_4:
	add	x9, sp, #16
	cmp	x8, x9
	b.eq	.LBB2_8
// %bb.5:
	cbnz	w11, .LBB2_8
// %bb.6:
	ldr	w8, [x19]
	cmp	w8, #34
	b.eq	.LBB2_8
// %bb.7:
	.cfi_def_cfa wsp, 304
	ldp	x28, x19, [sp, #288]            // 16-byte Folded Reload
	ldp	x29, x30, [sp, #272]            // 16-byte Folded Reload
	add	sp, sp, #304
	.cfi_def_cfa_offset 0
	.cfi_restore w19
	.cfi_restore w28
	.cfi_restore w30
	.cfi_restore w29
	ret
.LBB2_8:
	.cfi_restore_state
	adrp	x8, :got:stderr
	adrp	x0, .L.str.12
	add	x0, x0, :lo12:.L.str.12
	ldr	x8, [x8, :got_lo12:stderr]
	mov	w1, #58                         // =0x3a
	b	.LBB2_10
.LBB2_9:
	adrp	x8, :got:stderr
	adrp	x0, .L.str.11
	add	x0, x0, :lo12:.L.str.11
	ldr	x8, [x8, :got_lo12:stderr]
	mov	w1, #54                         // =0x36
.LBB2_10:
	ldr	x3, [x8]
	mov	w2, #1                          // =0x1
	bl	fwrite
	mov	w0, #1                          // =0x1
	bl	exit
.Lfunc_end2:
	.size	clair_rt_input_int, .Lfunc_end2-clair_rt_input_int
	.cfi_endproc
                                        // -- End function
	.section	.text.unlikely.,"ax",@progbits
	.p2align	2                               // -- Begin function clair_rt_list_runtime_error
	.type	clair_rt_list_runtime_error,@function
clair_rt_list_runtime_error:            // @clair_rt_list_runtime_error
	.cfi_startproc
// %bb.0:
	stp	x29, x30, [sp, #-32]!           // 16-byte Folded Spill
	.cfi_def_cfa_offset 32
	stp	x20, x19, [sp, #16]             // 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 32
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w30, -24
	.cfi_offset w29, -32
	ldr	x8, [x1]
	mov	x20, x0
	mov	x19, x1
	mov	x0, x8
	bl	free
	mov	x0, x19
	bl	free
	adrp	x8, :got:stderr
	adrp	x1, .L.str.5
	add	x1, x1, :lo12:.L.str.5
	ldr	x8, [x8, :got_lo12:stderr]
	mov	x2, x20
	ldr	x0, [x8]
	bl	fprintf
	mov	w0, #1                          // =0x1
	bl	exit
.Lfunc_end3:
	.size	clair_rt_list_runtime_error, .Lfunc_end3-clair_rt_list_runtime_error
	.cfi_endproc
                                        // -- End function
	.text
	.p2align	2                               // -- Begin function clair_rt_cleanup_lists
	.type	clair_rt_cleanup_lists,@function
clair_rt_cleanup_lists:                 // @clair_rt_cleanup_lists
	.cfi_startproc
// %bb.0:
	stp	x29, x30, [sp, #-64]!           // 16-byte Folded Spill
	.cfi_def_cfa_offset 64
	str	x23, [sp, #16]                  // 8-byte Folded Spill
	stp	x22, x21, [sp, #32]             // 16-byte Folded Spill
	stp	x20, x19, [sp, #48]             // 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 64
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -48
	.cfi_offset w30, -56
	.cfi_offset w29, -64
	adrp	x20, clair_rt_list_count
	adrp	x21, clair_rt_lists
	ldr	x22, [x20, :lo12:clair_rt_list_count]
	ldr	x19, [x21, :lo12:clair_rt_lists]
	cbz	x22, .LBB4_5
// %bb.1:
	mov	x23, x19
	b	.LBB4_3
.LBB4_2:                                //   in Loop: Header=BB4_3 Depth=1
	subs	x22, x22, #1
	add	x23, x23, #8
	b.eq	.LBB4_5
.LBB4_3:                                // =>This Inner Loop Header: Depth=1
	ldr	x8, [x23]
	cbz	x8, .LBB4_2
// %bb.4:                               //   in Loop: Header=BB4_3 Depth=1
	ldr	x0, [x8]
	bl	free
	ldr	x0, [x23]
	bl	free
	b	.LBB4_2
.LBB4_5:
	mov	x0, x19
	bl	free
	adrp	x8, clair_rt_list_capacity
	str	xzr, [x21, :lo12:clair_rt_lists]
	str	xzr, [x20, :lo12:clair_rt_list_count]
	str	xzr, [x8, :lo12:clair_rt_list_capacity]
	.cfi_def_cfa wsp, 64
	ldp	x20, x19, [sp, #48]             // 16-byte Folded Reload
	ldr	x23, [sp, #16]                  // 8-byte Folded Reload
	ldp	x22, x21, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #64             // 16-byte Folded Reload
	.cfi_def_cfa_offset 0
	.cfi_restore w19
	.cfi_restore w20
	.cfi_restore w21
	.cfi_restore w22
	.cfi_restore w23
	.cfi_restore w30
	.cfi_restore w29
	ret
.Lfunc_end4:
	.size	clair_rt_cleanup_lists, .Lfunc_end4-clair_rt_cleanup_lists
	.cfi_endproc
                                        // -- End function
	.type	.L.str,@object                  // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"%ld\n"
	.size	.L.str, 5

	.type	.L.str.2,@object                // @.str.2
.L.str.2:
	.asciz	"Clair runtime error: out of memory while creating list\n"
	.size	.L.str.2, 56

	.type	.L.str.4,@object                // @.str.4
.L.str.4:
	.asciz	"out of memory while creating list"
	.size	.L.str.4, 34

	.type	.L.str.5,@object                // @.str.5
.L.str.5:
	.asciz	"Clair runtime error: %s\n"
	.size	.L.str.5, 25

	.type	clair_rt_list_cleanup_registered,@object // @clair_rt_list_cleanup_registered
	.local	clair_rt_list_cleanup_registered
	.comm	clair_rt_list_cleanup_registered,1,4
	.type	.L.str.6,@object                // @.str.6
.L.str.6:
	.asciz	"cannot register list cleanup"
	.size	.L.str.6, 29

	.type	clair_rt_list_count,@object     // @clair_rt_list_count
	.local	clair_rt_list_count
	.comm	clair_rt_list_count,8,8
	.type	clair_rt_list_capacity,@object  // @clair_rt_list_capacity
	.local	clair_rt_list_capacity
	.comm	clair_rt_list_capacity,8,8
	.type	.L.str.7,@object                // @.str.7
.L.str.7:
	.asciz	"list allocation registry overflow"
	.size	.L.str.7, 34

	.type	clair_rt_lists,@object          // @clair_rt_lists
	.local	clair_rt_lists
	.comm	clair_rt_lists,8,8
	.type	.L.str.8,@object                // @.str.8
.L.str.8:
	.asciz	"out of memory while tracking lists"
	.size	.L.str.8, 35

	.type	.L.str.11,@object               // @.str.11
.L.str.11:
	.asciz	"Clair runtime error: input_int() could not read input\n"
	.size	.L.str.11, 55

	.type	.L.str.12,@object               // @.str.12
.L.str.12:
	.asciz	"Clair runtime error: input_int() expected a valid integer\n"
	.size	.L.str.12, 59

	.ident	"clang version 21.1.8"
	.section	".note.GNU-stack","",@progbits
