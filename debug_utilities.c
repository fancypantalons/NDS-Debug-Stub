/*
 * $Id: debug_utilities.c,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */
/** \file
 * \brief Useful functions for the debug stub.
 */
#include <stdint.h>

#include "debug_utilities.h"

/** Flush the instruction cache (nicked from libnds) */
void
IC_InvalidateAll_debug( void) {
  asm ( "mov r0, #0 \n\t"
	"mcr	p15, 0, r0, c7, c5, 0  \n\t"
	:
	:
	: "r0");
}

/** Flush the data cache (nicked form libnds) */
void
DC_FlushAll_debug( void) {
  asm volatile ("\tmov r1, #0 \n"
		"outer_loop:\n"
		"\tmov r0, #0\n"
		"inner_loop:\n"
		"\torr	r2, r1, r0			@ generate segment and line address\n"
		"\tmcr	p15, 0, r2, c7, c14, 2		@ clean and flush the line\n"
		"\tadd	r0, r0, #32\n"
		"\tcmp	r0, #0x1000/4\n"
		"\tbne	inner_loop\n"
		"\tadd	r1, r1, #0x40000000\n"
		"\tcmp	r1, #0\n"
		"\tbne	outer_loop\n"
		:
		:
		: "r0","r1"
		);
}


/** Enable the interrupts in the CPSR */
void
enable_IRQs_debug( void) {
  asm( "mrs r0, cpsr;\r\n"
       "bic r0, r0, #0xc0;\r\n"
       "msr cpsr_c, r0;\r\n"
       :
       :
       : "r0"
       );
}

/** Disable the interrupts in the CPSR */
void
disable_IRQs_debug( void) {
  asm( "mrs r0, cpsr;\r\n"
       "orr r0, r0, #0xc0;\r\n"
       "msr cpsr_c, r0;\r\n"
       :
       :
       : "r0"
       );
}


/** Return the SPSR register value */
uint32_t
getSPSR_debug( void) {
  uint32_t spsr_val;

  asm ( "mrs %0, spsr; \r\n"
	: "=r"(spsr_val)
	:
	);

  return spsr_val;
}

/** Return the CPSR register value */
uint32_t
getCPSR_debug( void) {
  uint32_t cpsr_val;

  asm ( "mrs %0, cpsr; \r\n"
	: "=r"(cpsr_val)
	:
	);

  return cpsr_val;
}

/** Set the banked R13 and R14 register values */
void
setBankedR13R14( uint32_t r13, uint32_t r14, uint32_t banked_mode) {
  if ( banked_mode == 0x10) {
    /* use system mode so we can come back */
    banked_mode = 0x1f;
  }

  asm volatile ( "mrs r0, cpsr \n\t"  /* save the current mode */
		 "and r0, r0, #0xff \n\t"
		 "and r1, r0, #0xe0 \n\t"
		 "orr %2, %2, r1 \n\t"  /* ready the banked mode bits */
		 "msr cpsr_c, %2 \n\t"
		 "mov r13, %0 \n\t"
		 "mov r14, %1 \n\t"
		 "msr cpsr_c, r0 \n\t"
		 :
		 : "r"(r13), "r"(r14), "r"(banked_mode)
		 : "r0", "r1"
		 );
}
