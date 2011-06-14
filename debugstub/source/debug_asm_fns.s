/*
 * $Id: debug_asm_fns.asm,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 *	
 */

	.text

	.arm

	.global debugHalt

/* Cause a prefetch exception */
debugHalt:
	bkpt
	bx	lr
