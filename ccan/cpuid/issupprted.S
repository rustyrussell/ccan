/*
  Test if the CPUID instruction is available.
  returns 1 if so, 0 otherwise.  */

.section .text
.global cpuid_is_supported
.type cpuid_is_supported, @function
cpuid_is_supported:
	nop

	pushfl
	popl %eax
	movl %eax, %ecx
	xorl $0x200000, %eax
	pushl %eax
	popfl

	pushfl
	popl %eax
	xorl %ecx, %eax
	shrl $21, %eax
	andl $1, %eax
	pushl %ecx
	popfl

	ret

	.size cpuid_is_supported, .-cpuid_is_supported

