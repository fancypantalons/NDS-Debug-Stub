#ifndef _DEBUG_STUB_H_
#define _DEBUG_STUB_H_ 1
/*
 * $Id: debug_stub.h,v 1.2 2006/10/10 12:10:47 ben Exp $
 */
/** \file
 * \brief The DS Debugger stub.
 */

/** \brief The funcion interface for a communication mechanism to the host PC.
 */
struct comms_fn_iface_debug {
  /** The initialisation function */
  int (*init_fn)( void *data);

  /** Read a byte from the comms mechanism. Returns 1 ofn successful read, 0 otherwise. */
  int (*readByte_fn)( uint8_t *byte_addr);

  /** Write a byte to the comms mechanism */
  void (*writeByte_fn)( uint8_t byte);

  /** Write data to the comms mechanism */
  void (*writeData_fn)( uint8_t *buffer, uint32_t count);

  /** Poll the comms mechanism (NULL if not needed) */
  void (*poll_fn)( void);

  /** Return the bit mask of the DS Interrupts (REG_IE) needed for comms */
  uint32_t (*get_IRQs)( void);
};

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Calling this function will cause a jump to the debugger.
 */
void
debugHalt( void);


/** \brief Initialises the debugger stub and the supplied comms interface.
 */
int
init_debug( struct comms_fn_iface_debug *comms_if, void *comms_data);


#ifdef __cplusplus
};
#endif

#endif /* End of _DEBUG_STUB_H_ */
