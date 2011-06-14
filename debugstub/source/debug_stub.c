#include <nds.h>

#include <nds/arm9/exceptions.h>

#include <stdint.h>
#include <string.h>

#include "debug_stub.h"
#include "debug_comms.h"

#include "breakpoints.h"
#include "opcode_decode.h"
#include "debug_utilities.h"
#include "logging.h"

/** The maximum number of concurrent breakpoints the stub can maintain.
 * This number includes breakpoints required for stepping code.
 */
#define MAX_BREAKPOINTS 32



/*
 * The UNIX signal numbers that GDB expects
 */
/** Signal number for an illegal instruction */
#define SIGILL 4
/** Signal number for a trap instruction */
#define SIGTRAP 5


#define BUFMAX 2048

static uint8_t remcomInBuffer[BUFMAX];
static uint8_t remcomOutBuffer[BUFMAX];



/** The debug stub descriptor.
 */
struct debug_descr {
  /** The interface to the debugger communications. */
  struct comms_fn_iface_debug *comms_if;

  /** The enabled DS interrupts */
  uint32_t enabled_irqs;

  /** the return address */
  uint32_t ret_addr;

  /** the pool of free breakpoint descriptors */
  struct breakpoint_descr *free_breakpts;

  /** the active breakpoint descriptors */
  struct breakpoint_descr *active_breakpts;

  /** the active stepping breakpoint descriptors */
  struct breakpoint_descr *stepping_breakpts;

  /** the breakpoint descriptors of breakpoint disabled by stepping */
  struct breakpoint_descr *disabled_breakpts;

  /** flag set whilst inside debug stub */
  int in_stub;

  /** the address of the debug stub's irq handler */
  uint32_t debug_irq_handler;

  /** the address of the application's irq handler */
  uint32_t app_irq_handler;

  /** flag indicating if the app IRQ handler has been saved */
  int saved_irq_handler;

  /** The saved value of the master interrupt enable register */
  uint16_t master_irq;
};


/** The pool of breakpoint descriptors */
struct breakpoint_descr breakpoint_pool[MAX_BREAKPOINTS];


/** The instance of the debug stub */
struct debug_descr debug_stub_descr;


/** Call the comms interface transmit function. */
#define getDebugChar( comms_if, byte_addr) (comms_if)->readByte_fn( byte_addr)

/** Call the comms interface receive function. */
#define putDebugChar( comms_if, byte) (comms_if)->writeByte_fn( (byte))

/** Poll the comms interface */
#define poll_comms( comms_if) if ( (comms_if)->poll_fn != NULL) (comms_if)->poll_fn();

/** reads the comms until a byte is available */
#define read_block_comms( comms_if, byte_addr) while ( !getDebugChar( comms_if, byte_addr)) poll_comms( comms_if);


/**
 * Check a memory address is good before reading/writing it.
 * If the memory is outside the good range then ignore write
 * and give zero for read.
 *
 * FIXME: setting up some better exception vectors in the ITCM would
 * be a better solution for data aborts and trying to avoid it.
 */
inline int
memAddrCheck( uint8_t *addr) {
  int ok_flag = 0;

  /* FIXME: what a lovely hardcoded address, works as long as DTCM is above this */
  if ( (uint32_t)addr >= 0x01000000) {
    ok_flag = 1;
  }

  return ok_flag;
}


static const char hexchars[]="0123456789abcdef";

/*
 * Convert ch from a hex digit to an int
 */
static int
hex (unsigned char ch) {
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */

static int
hexToInt( uint8_t **ptr, uint32_t *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr) {
    hexValue = hex(**ptr);
    if (hexValue < 0)
      break;

    *intValue = (*intValue << 4) | hexValue;
    numChars ++;

    (*ptr)++;
  }

  return (numChars);
}


/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 */
static unsigned char *
mem2hex (unsigned char *mem, unsigned char *buf, int count)
{
  unsigned char ch;

  while (count-- > 0) {
    ch = 0;
    if ( memAddrCheck( mem)) {
      ch = *mem++;
    }
    *buf++ = hexchars[ch >> 4];
    *buf++ = hexchars[ch & 0xf];
  }

  *buf = 0;

  return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static unsigned char *
hex2mem (unsigned char *buf, unsigned char *mem, int count) {
  int i;
  unsigned char ch;

  for (i=0; i<count; i++) {
    ch = hex(*buf++) << 4;
    ch |= hex(*buf++);

    if ( memAddrCheck( mem)) {
      *mem++ = ch;
    }
  }

  return mem;
}

/* convert the gdb binary array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */
static unsigned char *
bin2mem( uint8_t *buf, uint8_t *mem, int count) {
  int i;
  uint8_t cur_byte;
  int escaped = 0;

  for ( i = 0; i < count;) {
    cur_byte = *buf++;

    if ( escaped) {
      cur_byte ^= 0x20;
      escaped = 0;
    }
    else if ( cur_byte == 0x7d) {
      escaped = 1;
    }

    if ( !escaped) {
      i += 1;
      if ( memAddrCheck( mem)) {
	*mem++ = cur_byte;
      }
    }
  }

  return mem;
}

/* scan for the sequence $<data>#<checksum>     */
static unsigned char *
getpacket ( struct comms_fn_iface_debug *comms_if) {
  unsigned char *buffer = &remcomInBuffer[0];
  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  uint8_t ch;

  while (1) {
    ch = '\0';
    /* wait around for the start character, ignore all other characters */
    while ( ch != '$') {
      read_block_comms( comms_if, &ch);
    }

  retry:
    checksum = 0;
    xmitcsum = -1;
    count = 0;

    /* now, read until a # or end of buffer is found */
    while (count < BUFMAX - 1) {
      read_block_comms( comms_if, &ch);
      if (ch == '$')
	goto retry;
      if (ch == '#')
	break;
      checksum = checksum + ch;
      buffer[count] = ch;
      count = count + 1;
    }
    buffer[count] = 0;

    if (ch == '#') {
      read_block_comms( comms_if, &ch);
      xmitcsum = hex (ch) << 4;
      read_block_comms( comms_if, &ch);
      xmitcsum += hex (ch);

      if (checksum != xmitcsum) {
	putDebugChar( comms_if, '-');	/* failed checksum */
      }
      else {
	putDebugChar( comms_if, '+');	/* successful transfer */

	/* if a sequence char is present, reply the sequence ID */
	if (buffer[2] == ':') {
	  putDebugChar( comms_if, buffer[0]);
	  putDebugChar( comms_if, buffer[1]);

	  return &buffer[3];
	}

	return &buffer[0];
      }
    }
  }
}


/*
 * send the packet in buffer.
 * Requires room for an additional three trailing bytes and one prefixed byte in the buffer.
 */
static void
putpacket ( struct comms_fn_iface_debug *comms_if, unsigned char *buffer) {
  unsigned char checksum;
  int count;
  unsigned char ch;
  uint8_t read_ch;

  /*  $<packet info>#<checksum>. */

  /* checksum the buffer */
  *(buffer-1) = '$';
  count = 0;
  checksum = 0;
  while ( (ch = buffer[count])) {
    checksum += ch;
    count += 1;
  }
  buffer[count++] = '#';
  buffer[count++] = hexchars[checksum >> 4];
  buffer[count++] = hexchars[checksum & 0xf];

  do {
    comms_if->writeData_fn( buffer-1, count+1);

    read_block_comms( comms_if, &read_ch);
  } while ( read_ch != '+');

#if 0
  do {
    putDebugChar( comms_if, '$');
    checksum = 0;
    count = 0;

    while ( (ch = buffer[count])) {
      putDebugChar( comms_if, ch);
      checksum += ch;
      count += 1;
    }

    putDebugChar( comms_if, '#');
    putDebugChar( comms_if, hexchars[checksum >> 4]);
    putDebugChar( comms_if, hexchars[checksum & 0xf]);

    read_block_comms( comms_if, &read_ch);
  }
  while ( read_ch != '+');
#endif
}


/** Process the GDB messages.
 */
static void
debug_stub( struct debug_descr *debug_descr) {
  int return_now = 0;

  //uint32_t reg_mem_ptr = (uint32_t)reg_mem;
//---------------------------------------------------------------------------------

  uint32_t thumb_state = getSPSR_debug() & 0x20;

  uint8_t *ptr;

  //LOG("CPSR 0x%08x\n", currentMode);

  /*
   * See if something strange has happened
   */
  if ( debug_descr->in_stub) {
    LOG("Exception generated by stub\n");
    while(1);
  }
  debug_descr->in_stub = 1;

  /*
   * Remove all the breakpoints from memory so the
   * code is seen as written.
   */
  removeAll_breakpoint( debug_descr->stepping_breakpts);
  removeAll_breakpoint( debug_descr->active_breakpts);
  //IC_InvalidateAll_debug();
  //DC_FlushAll_debug();

  /*
   * put the disabled breakpoints back on the active list.
   */
  catLists_breakpoint( &debug_descr->active_breakpts,
		       debug_descr->disabled_breakpts);
  debug_descr->disabled_breakpts = NULL;


  /*
   * Remove any stepping breakpoints that match the current address
   */
  {
    struct breakpoint_descr *found =
      removeFromList_breakpoint( &debug_descr->stepping_breakpts, debug_descr->ret_addr);

    if ( found != NULL) {
      addHead_breakpoint( &debug_descr->free_breakpts, found);
    }
  }


  while ( !return_now) {
    int send_reply = 1;
    remcomOutBuffer[1] = 0;

    LOG("Getting debug packet\n");
    ptr = getpacket( debug_descr->comms_if);

    LOG("CMD:");
    LOG( (char *)ptr);
    LOG("\n");

    switch (*ptr++) {
    case '?':
      remcomOutBuffer[1] = 'S';
      remcomOutBuffer[2] = hexchars[0x10 >> 4];
      remcomOutBuffer[3] = hexchars[0x10l & 0xf];
      remcomOutBuffer[4] = 0;
      break;

    case 'g':		/* return the value of the CPU registers */
      {
	int i;
	uint32_t cpsr_val;
	uint32_t pc_value;
	ptr = &remcomOutBuffer[1];

	/* general purpose regs 0 to 15 */
	for ( i = 0; i < 15; i++) {
	  mem2hex( (uint8_t *)&exceptionRegisters[i], ptr, 4);
	  ptr += 8;
	}
	/* set the PC to the return addres value */
	pc_value = debug_descr->ret_addr;
	mem2hex( (uint8_t *)&pc_value, ptr, 4);
	ptr += 8;

	/* floating point registers (8 x 96bit) */
	for ( i = 0; i < 8; i++) {
	  *ptr++ = '0';*ptr++ = '0';*ptr++ = '0';*ptr++ = '0';
	  *ptr++ = '0';*ptr++ = '0';*ptr++ = '0';*ptr++ = '0';
	  *ptr++ = '0';*ptr++ = '0';*ptr++ = '0';*ptr++ = '0';
	  *ptr++ = '0';*ptr++ = '0';*ptr++ = '0';*ptr++ = '0';
	  *ptr++ = '0';*ptr++ = '0';*ptr++ = '0';*ptr++ = '0';
	  *ptr++ = '0';*ptr++ = '0';*ptr++ = '0';*ptr++ = '0';
	}

	/* floating point status register? */
	for ( i = 0; i < 1; i++) {
	  *ptr++ = '0';
	  *ptr++ = '1';
	  *ptr++ = '2';
	  *ptr++ = '3';
	  *ptr++ = '4';
	  *ptr++ = '5';
	  *ptr++ = '6';
	  *ptr++ = '7';
	}

	/* The CPSR */
	cpsr_val = getSPSR_debug();
	mem2hex( (uint8_t *)&cpsr_val, ptr, 4);
	ptr += 8;
	*ptr = 0;
      }
      break;

    case 'G':	   /* set the value of the CPU registers - return OK */
      {
	LOG("G command\n");
	int i;
	uint32_t pc_value;

	/* general purpose regs 0 to 15 */
	for ( i = 0; i < 15; i++) {
	  hex2mem( ptr, (uint8_t *)&exceptionRegisters[i], 4);
	  ptr += 8;
	}
	/* set the PC to the return addres value */
	hex2mem( ptr, (uint8_t *)&pc_value, 4);
	ptr += 8;
	debug_descr->ret_addr = pc_value;

	/* skip the floaing point registers and floating point status register */
	ptr += 8 * (96 / 8 * 2);
	ptr += 8;

	/* the CPSR register is last */
	/* FIXME: do something with the CPSR */
	ptr += 8;

	strcpy( (char *)&remcomOutBuffer[1], "OK");
      }
      break;

      /*
       * The step command.
       * FIXME: always steps from current point.
       */
    case 's': {
      /* the address to insert the step breakpoint */
      int step_thumb_state = thumb_state;
      uint32_t step_addr = debug_descr->ret_addr + (thumb_state ? 2 : 4);
      struct breakpoint_descr *step_bkpt;

      LOG("Stepping\n");

      /* if the next to be executed instruction will cause a branch the step
       * address must be set to the resulting destination address.
       */
      if ( instructionExecuted_opcode( debug_descr->ret_addr, thumb_state, getSPSR_debug())) {
	uint32_t branch_addr;

	/* the register set */
	uint32_t reg_set[17];
	int i;

        LOG("Getting exception regs\n");
	for ( i = 0; i < 15; i++) {
	  reg_set[i] = exceptionRegisters[i];
	}
        LOG("Calculate addy, thumb state is %d\n", thumb_state);
	if ( thumb_state)
	  /* the PC is expected to be current address + 4 */
	  reg_set[15] = debug_descr->ret_addr + 4;
	else
	  /* the PC is expected to be current address + 8 */
	  reg_set[15] = debug_descr->ret_addr + 8;
        LOG("GetSPSR thinger\n");
	reg_set[16] = getSPSR_debug();

        LOG("CauseJump thinger\n");

	if ( causeJump_opcode( debug_descr->ret_addr, &step_thumb_state, reg_set, &branch_addr)) {
	  step_addr = branch_addr;
	  LOG("Branch instruction, dest %08x, thumb %d\n", step_addr, step_thumb_state);
	}
      }
      else {
	LOG("Instruction not executed\n");
      }

      /* see if the step break already exists */
      step_bkpt = removeFromList_breakpoint( &debug_descr->stepping_breakpts,
					     step_addr);
      if ( step_bkpt != NULL) {
	LOG("Step bk at %08x already exists\n", step_addr);
      }
      else {
	step_bkpt = removeHead_breakpoint( &debug_descr->free_breakpts);
      }

      if ( step_bkpt != NULL) {
	struct breakpoint_descr *disable_bkpt;
	initDescr_breakpoint( step_bkpt, step_addr, step_thumb_state);

	addHead_breakpoint( &debug_descr->stepping_breakpts, step_bkpt);

	/* disable any breakpoints at the current return addr */
	disable_bkpt = removeFromList_breakpoint( &debug_descr->active_breakpts,
						  debug_descr->ret_addr);
	if ( disable_bkpt != NULL) {
	  addHead_breakpoint( &debug_descr->disabled_breakpts, disable_bkpt);
	}

	/* continue running the app */
	return_now = 1;
	send_reply = 0;
      }
      else {
	/*
	 * no more breakpoint descriptors left, send a stop packet so the
	 * debugger sits on the current instruction until the user does something.
	 * That is about the best option available.
	 */
	ptr = &remcomOutBuffer[1];
	*ptr++ = 'S';
	*ptr++ = hexchars[SIGTRAP >> 4];
	*ptr++ = hexchars[SIGTRAP & 0xf];
	*ptr = 0;
      }
      break;
    }

      /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
    case 'm': {
      uint32_t addr;
      uint32_t length;
      int error01 = 1;

      if ( hexToInt( &ptr, &addr)) {
	if (*ptr++ == ',') {
	  if ( hexToInt(&ptr, &length)) {
	    //LOG("mem read from %08x (%d)\n", addr, length);
	    if ( !mem2hex( (uint8_t *)addr, &remcomOutBuffer[1], length)) {
	      strcpy ( (char *)&remcomOutBuffer[1], "E03");
	    }
	    error01 = 0;
	  }
	}
      }
      if ( error01)
	strcpy( (char *)&remcomOutBuffer[1],"E01");
      break;
    }

      /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
    case 'M': {
      /* Try to read '%x,%x:'.  */
      uint32_t addr;
      uint32_t length;
      int error01 = 1;
      if ( hexToInt(&ptr, &addr)) {
	if ( *ptr++ == ',') {
	  if ( hexToInt(&ptr, &length)) {
	    if ( *ptr++ == ':') {
	      if ( hex2mem( ptr, (uint8_t *)addr, length)) {
		//LOG("mem write at %08x (%d)\n", addr, length);
		strcpy( (char *)&remcomOutBuffer[1], "OK");
	      }
	      else
		strcpy( (char *)&remcomOutBuffer[1], "E03");
	      error01 = 0;
	    }
	  }
	}
      }

      if ( error01) {
	strcpy( (char *)&remcomOutBuffer[1], "E02");
      }
      break;
    }

      /* binary data to write to memory
       * XAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
#if 1
    case 'X': {
      /* Try to read '%x,%x:'.  */
      uint32_t addr;
      uint32_t length;
      
      int error01 = 1;
      if ( hexToInt(&ptr, &addr)) {
	if ( *ptr++ == ',') {
	  if ( hexToInt(&ptr, &length)) {
	    if ( *ptr++ == ':') {
	      if ( bin2mem( ptr, (uint8_t *)addr, length)) {
		//LOG("mem write at %08x (%d)\n", addr, length);
		strcpy( (char *)&remcomOutBuffer[1], "OK");
	      }
	      else
		strcpy( (char *)&remcomOutBuffer[1], "E03");
	      error01 = 0;
	    }
	  }
	}
      }

      if ( error01) {
	strcpy( (char *)&remcomOutBuffer[1], "E02");
      }
      break;
    }
#endif
    case 'c':    /* cAA..AA    Continue at address AA..AA(optional) */
      /* try to read optional parameter, pc unchanged if no parm */
      /* FIXME: always continuing from where we left off */
      return_now = 1;
      send_reply = 0;
      break;

      /* kill the program */
    case 'k' :		/* do nothing */
      send_reply = 0;
      break;
    }			/* switch */

    /* reply to the request */
    if ( send_reply) {
      //LOG("REPLY: ");
      //LOG( (char *)remcomOutBuffer);
      //LOG("\n");
      putpacket( debug_descr->comms_if, &remcomOutBuffer[1]);
    }
  }

  /*
   * insert the active breakpoints in memory
   */
  LOG( "Inserting active breakpoints\n");
  insertAll_breakpoint( debug_descr->active_breakpts);
  LOG( "Inserting stepping breakpoints\n");
  insertAll_breakpoint( debug_descr->stepping_breakpts);

  LOG( "Flushing caches\n");
  IC_InvalidateAll_debug();
  DC_FlushAll_debug();

  LOG( "Stub complete\n");
  debug_descr->in_stub = 0;
}


/** Trying to get back, setting the values for the registers
 */
static void
jumpBack( uint32_t ret_addr) {
  uint32_t reg_mem[16]; /* space for r0-r14 and the PC */
  int i;

  /* NOTE: this looses stack but as the stack is reset at each exception
   * it does not matter. */

  /* r0-r12 and pc values are copied in the ldm instruction */
  for ( i = 0; i < 13; i++) {
    reg_mem[i] = exceptionRegisters[i];
  }
  reg_mem[i] = ret_addr;
  LOG("ret_addr 0x%08x (%08x,%08x)\n", ret_addr, *(uint32_t *)reg_mem[i], *(uint32_t *)(reg_mem[i] + 4));
  for ( i = 0; i < 16; i += 2) {
    LOG( "r%d %08x, r%d %08x, ", i, exceptionRegisters[i], i+1, exceptionRegisters[i+1]);
  }
  LOG( "\n");

  /* the banked registers, r13 and r14, are placed directly */
  uint32_t broken_mode = getSPSR_debug() & 0x1f;

  setBankedR13R14( exceptionRegisters[13], exceptionRegisters[14],
	      broken_mode);

  /* FIXME: switch processor mode and set the values */
  asm volatile ( "ldmia %0, {r0-r12,pc}^   \n"
		 : 
		 : "r"(reg_mem)
		 : "memory"
		 );
}


/** Ask the comms interface what interrupts it need and
 * enable only those, storing the previous value beforehand.
 * Once setup, the interrupts are enabled in the CPSR register.
 */
static void
enableCommsIRQs( struct debug_descr *debug_descr) {
  uint32_t irq_bits = debug_descr->comms_if->get_IRQs();

  if ( irq_bits == 0) {
    /* none needed so leave interrupts disabled */
    debug_descr->saved_irq_handler = 0;
  }
  else {
    LOG("app irq handler %08x, debug %08x\n", (uint32_t)IRQ_HANDLER,
	debug_descr->debug_irq_handler);
    debug_descr->saved_irq_handler = 1;
    debug_descr->app_irq_handler = (uint32_t)IRQ_HANDLER;
    IRQ_HANDLER = (void (*)(void))debug_descr->debug_irq_handler;
    debug_descr->enabled_irqs = REG_IE;
    debug_descr->master_irq = REG_IME;
    REG_IE = irq_bits;
    REG_IME = 1;
    enable_IRQs_debug();
  }
}


/** Restore the DS enabled interrupts to those set on entry.
 */
static void
restoreIRQstate( struct debug_descr *debug_descr) {
  if ( debug_descr->saved_irq_handler != 0) {
    disable_IRQs_debug();
    REG_IE = debug_descr->enabled_irqs;
    REG_IME = debug_descr->master_irq;
    IRQ_HANDLER = (void (*)(void))debug_descr->app_irq_handler;
  }
}


/** Compute the address of the instruction to jump back
 * to.
 */
static uint32_t
computeReturnAddr( uint32_t *reg_set) {
  uint32_t ret_addr;
  uint32_t currentMode = getCPSR_debug() & 0x1f;
  uint32_t thumb_state = getSPSR_debug() & 0x20;

  /*
   * The processor mode allows the return address to be computed.
   * FIXME: data or prefetch abort need to be determined.
   */
  if ( currentMode == 0x17 ) {
    //LOG ("data/prefetch abort!\n");
    ret_addr = reg_set[PC] - 4;

    if ( (uint32_t)debugHalt == ret_addr) {
      /* jump over the bkpt instruction */
      LOG("Exception caused by debugHalt call\n");
      exceptionRegisters[PC] += 4;
      ret_addr += 4;
    }
  } else {
    //LOG("undefined instruction!\n\n");

    ret_addr = reg_set[PC] - 4;

    if ( thumb_state) {
      /* the undefined instruction is just +2 on */
      ret_addr += 2;
    }
  }

  return ret_addr;
}

static void debugHandler() {
  uint8_t *ptr;
  int i;
  uint32_t spsr_value;

  LOG("Normal handler\n");

  u32 currentMode = getCPSR_debug() & 0x1f;

  exceptionRegisters[15] = *(u32*)0x027FFD98;

  debug_stub_descr.ret_addr = computeReturnAddr( (uint32_t *)exceptionRegisters);

  /* send out the T packet */
  ptr = &remcomOutBuffer[1];
  *ptr++ = 'T';
  if ( currentMode == 0x17 ) {
    *ptr++ = hexchars[SIGTRAP >> 4];
    *ptr++ = hexchars[SIGTRAP & 0xf];
  }
  else {
    *ptr++ = hexchars[SIGILL >> 4];
    *ptr++ = hexchars[SIGILL & 0xf];
  }

  for ( i = 0; i < 15; i++) {
    *ptr++ = hexchars[i >> 4];
    *ptr++ = hexchars[i & 0xf];
    *ptr++ = ':';
    mem2hex( (uint8_t *)&exceptionRegisters[i], ptr, 4);
    ptr += 8;
    *ptr++ = ';';
  }
  /* the PC register */
  *ptr++ = hexchars[0xf >> 4];
  *ptr++ = hexchars[0xf & 0xf];
  *ptr++ = ':';
  mem2hex( (uint8_t *)&debug_stub_descr.ret_addr, ptr, 4);
  ptr += 8;
  *ptr++ = ';';

  /* the CPSR register */
  spsr_value = getSPSR_debug();
  *ptr++ = hexchars[0x19 >> 4];
  *ptr++ = hexchars[0x19 & 0xf];
  *ptr++ = ':';
  mem2hex( (uint8_t *)&spsr_value, ptr, 4);
  ptr += 8;
  *ptr++ = ';';
  *ptr = 0;

  /* Enable any interrupts needed for debug comms */
  enableCommsIRQs( &debug_stub_descr);

  putpacket( debug_stub_descr.comms_if, &remcomOutBuffer[1]);

  debug_stub( &debug_stub_descr);

  /* restore the old irq state */
  restoreIRQstate( &debug_stub_descr);

  jumpBack( debug_stub_descr.ret_addr);
}


static void firstRunHandler() {
  LOG("First time handler\n");

  exceptionRegisters[15] = *(u32*)0x027FFD98;

  debug_stub_descr.ret_addr = computeReturnAddr( (uint32_t *)exceptionRegisters);

  /* install the normal exception handler */
  setExceptionHandler( debugHandler);

  /* Enable any interrupts needed for debug comms */
  enableCommsIRQs( &debug_stub_descr);

  debug_stub( &debug_stub_descr);

  /* restore the irq state */
  restoreIRQstate( &debug_stub_descr);

  jumpBack( debug_stub_descr.ret_addr);
}


/** \brief Initialise the debugger stub.
 */
int
init_debug( struct comms_fn_iface_debug *comms_if, void *comms_data) {
  int success_flag;
  int i;

  LOG("Entering debug init\n");

  debug_stub_descr.comms_if = comms_if;

  /* install the exception handler */
  LOG("Setting exception handler\n");
  setExceptionHandler( firstRunHandler);

  LOG("Initializing breakpoint pool\n");
  /* initialise the breakpoint descrs */
  for ( i = 0; i < MAX_BREAKPOINTS; i++) {
    breakpoint_pool[i].next = NULL;
    if ( i > 0) {
      breakpoint_pool[i - 1].next = &breakpoint_pool[i];
    }
  }
  debug_stub_descr.free_breakpts = &breakpoint_pool[0];
  debug_stub_descr.stepping_breakpts = NULL;
  debug_stub_descr.active_breakpts = NULL;
  debug_stub_descr.disabled_breakpts = NULL;

  debug_stub_descr.in_stub = 0;

  /* initialise the communications link */
  LOG("Calling comms init\n");
  success_flag = debug_stub_descr.comms_if->init_fn( comms_data);

  /* save our interrupt handler pointer */
  debug_stub_descr.debug_irq_handler = (uint32_t)IRQ_HANDLER;

  return success_flag;
}
