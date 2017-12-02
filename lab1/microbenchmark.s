	.file	1 "microbenchmark.c"

 # GNU C 2.7.2.3 [AL 1.1, MM 40, tma 0.1] SimpleScalar running sstrix compiled by GNU C

 # Cc1 defaults:
 # -mgas -mgpOPT

 # Cc1 arguments (-G value = 8, Cpu = default, ISA = 1):
 # -quiet -dumpbase -O0 -o

gcc2_compiled.:
__gnu_compiled_c:
	.text
	.align	2
	.globl	main

	.text

	.loc	1 5
	.ent	main
main:
	.frame	$fp,32,$31		# vars= 8, regs= 2/0, args= 16, extra= 0
	.mask	0xc0000000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,32
	sw	$31,28($sp)
	sw	$fp,24($sp)
	move	$fp,$sp
	jal	__main
	sw	$0,16($fp)
$L2:
	lw	$2,16($fp)
	li	$3,0x000f423f		# 999999
	slt	$2,$3,$2
	beq	$2,$0,$L5
	j	$L3
$L5:
 #APP
	addi $1, $0, 20
	addi $2, $0, 30
	
 #NO_APP
$L4:
	lw	$3,16($fp)
	addu	$2,$3,1
	move	$3,$2
	sw	$3,16($fp)
	j	$L2
$L3:
$L1:
	move	$sp,$fp			# sp not trusted here
	lw	$31,28($sp)
	lw	$fp,24($sp)
	addu	$sp,$sp,32
	j	$31
	.end	main
