#ifndef _DEBUG_UTILITIES_H_
#define _DEBUG_UTILITIES_H_ 1
/*
 * $Id: debug_utilities.h,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */
/** \file
 * \brief Useful functions for the debug stub.
 */

/** Flush the instruction cache (nicked from libnds) */
void
IC_InvalidateAll_debug( void);

/** Flush the data cache (nicked from libnds) */
void
DC_FlushAll_debug( void);

/** Enable the interrupts in the CPSR */
void
enable_IRQs_debug( void);

/** Disable the interrupts in the CPSR */
void
disable_IRQs_debug( void);

/** Return the SPSR register value */
uint32_t
getSPSR_debug( void);

/** Return the CPSR register value */
uint32_t
getCPSR_debug( void);

/** Set the banked R13 and R14 register values */
void
setBankedR13R14( uint32_t r13, uint32_t r14, uint32_t banked_mode);

#endif /* End of _DEBUG_UTILITIES_H_ */
