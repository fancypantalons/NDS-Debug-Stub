/*
 * $Id: opcode_decode.c,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */
/** \file
 * \brief Functions for interpreting and handling ARM and Thumb instructions.
 */
#include <stdint.h>
#include <stdlib.h>

#include "opcode_decode.h"
#include "debug_utilities.h"
#include "logging.h"

int
conditionCheck_opcode( uint32_t condition, uint32_t cpsr) {
  int executed_flag = 0;

  /* there are 16 condition values */
  switch ( condition) {
  case 0x0: /* Equal */
    if ( cpsr & CPSR_Z_FLAG)
      executed_flag = 1;
    break;

  case 0x1: /* Not Equal */
    if ( !(cpsr & CPSR_Z_FLAG))
      executed_flag = 1;
    break;

  case 0x2: /* Carry set.unsigned higher or same */
    if ( cpsr & CPSR_C_FLAG)
      executed_flag = 1;
    break;

  case 0x3: /* Carry clear/unsigned lower */
    if ( !(cpsr & CPSR_C_FLAG))
      executed_flag = 1;
    break;

  case 0x4: /* Minux/negative */
    if ( cpsr & CPSR_N_FLAG)
      executed_flag = 1;
    break;

  case 0x5: /* Plus/positive or zero  */
    if ( !(cpsr & CPSR_N_FLAG))
      executed_flag = 1;
    break;

  case 0x6: /* Overflow */
    if ( cpsr & CPSR_V_FLAG)
      executed_flag = 1;
    break;

  case 0x7: /* No overflow */
    if ( !(cpsr & CPSR_V_FLAG))
      executed_flag = 1;
    break;

  case 0x8: /* Unsigned higher */
    if ( cpsr & CPSR_C_FLAG && !(cpsr & CPSR_Z_FLAG))
      executed_flag = 1;
    break;

  case 0x9: /* Unsigned lower or same */
    if ( !(cpsr & CPSR_C_FLAG) && cpsr & CPSR_Z_FLAG)
      executed_flag = 1;
    break;

  case 0xa: /* Signed greater than or equal */
    if ( (cpsr & CPSR_N_FLAG && cpsr & CPSR_V_FLAG) ||
	 (!(cpsr & CPSR_N_FLAG) && !(cpsr & CPSR_V_FLAG)))
      executed_flag = 1;
    break;

  case 0xb: /* Signed less than */
    if ( (cpsr & CPSR_N_FLAG && !(cpsr & CPSR_V_FLAG)) ||
	 (!(cpsr & CPSR_N_FLAG) && cpsr & CPSR_V_FLAG))
      executed_flag = 1;
    break;

  case 0xc: /* Signed greater than */
    if ( !(cpsr & CPSR_Z_FLAG) &&
	 ((cpsr & CPSR_N_FLAG && cpsr & CPSR_V_FLAG) ||
	  (!(cpsr & CPSR_N_FLAG) && !(cpsr & CPSR_V_FLAG))))
      executed_flag = 1;
    break;

  case 0xd: /* Signed less than or equal */
    if ( cpsr & CPSR_Z_FLAG ||
	 ((cpsr & CPSR_N_FLAG && !(cpsr & CPSR_V_FLAG)) ||
	  (!(cpsr & CPSR_N_FLAG) && cpsr & CPSR_V_FLAG)))
      executed_flag = 1;
    break;

  case 0xe: /* Always */
  case 0xf: /* Extended */
  default:
    executed_flag = 1;
    break;
  }

  return executed_flag;
}



/** Determine if the instruction at the supplied address is executed.
 */
int
instructionExecuted_opcode( uint32_t instr_addr, int thumb_flag, uint32_t cpsr) {
  int executed_flag = 0;

  LOG("Inside instructionExecuted thinger: %d\n", thumb_flag);
  if ( thumb_flag) {
    /* instructions are not conditionally executed in thumb mode */
    executed_flag = 1;
  }
  else {
    uint32_t instr_cond = ((*(uint32_t *)instr_addr) & ARM_CONDITION_MASK) >> 28;

    LOG("Check executed flag\n");
    executed_flag = conditionCheck_opcode( instr_cond, cpsr);
    LOG("DONE\n");
  }

  return executed_flag;
}




/** Determine if the instruction causes a branch, i.e. changes r15 the program counter.
 */
int
causeJump_opcode( uint32_t instr_addr, int *thumb_flag, const uint32_t *reg_set, uint32_t *dest_addr) {
  int branch_flag = 0;
  *dest_addr = instr_addr;

  if ( *thumb_flag) {
    uint16_t op_code = *(uint16_t *)instr_addr;
    branch_flag = causeJump_thumb( op_code, thumb_flag, reg_set, dest_addr);
  }
  else {
    /* The arm conditions are assumed to have been checked else where */
    uint32_t op_code = *(uint32_t *)instr_addr;
    branch_flag = causeJump_arm( op_code, thumb_flag, reg_set, dest_addr);
  }

  return branch_flag;
}
