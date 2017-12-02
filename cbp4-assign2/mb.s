	.file	"mb.c"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	$0, -4(%rbp)
	movl	$0, -4(%rbp)
	jmp	.L2
.L5:
	movw	$0, -6(%rbp)
	jmp	.L3
.L4:
	movswl	-6(%rbp), %eax
	cltq
	movzwl	-6(%rbp), %edx
	movw	%dx, -16(%rbp,%rax,2)
	movzwl	-6(%rbp), %eax
	addl	$1, %eax
	movw	%ax, -6(%rbp)
.L3:
	cmpw	$4, -6(%rbp)
	jle	.L4
	addl	$1, -4(%rbp)
.L2:
	cmpl	$999, -4(%rbp)
	jle	.L5
	movl	$0, %eax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.ident	"GCC: (Debian 6.3.0-18) 6.3.0 20170516"
	.section	.note.GNU-stack,"",@progbits
