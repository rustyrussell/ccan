/*
  Test if the CPUID instruction is available.
  returns 1 if so, 0 otherwise.  */

/* Only compile this file if we're on a x86 machine.  */
#if defined(__i386__) || defined(__i386) || defined(__x86_64) \
	|| defined(_M_AMD64) || defined(__M_X64)

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
#endif

