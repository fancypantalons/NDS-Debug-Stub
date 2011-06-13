/*
 * $Id: thumb_instr.c,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */


#include "logging.h"

#define THUMB_COND_B_MASK 0xf000
#define THUMB_COND_B 0xd000
#define THUMB_COND_B_CONDITION_MASK 0x0f00
static enum instruction_match
thumb_condB( uint16_t op_code, uint32_t cpsr) {
  enum instruction_match branch = NO_MATCH_INSTR;

  if ( (op_code & THUMB_COND_B_MASK) == THUMB_COND_B) {
    uint32_t cond = (op_code & THUMB_COND_B_CONDITION_MASK) >> 8;
    if ( cond != 0xf) {
      branch = MATCH_INSTR;

      /* If the condition is meet then the branch is taken */
      if ( condition_check( cond, cpsr)) {
	branch = MATCH_AND_JUMP_INSTR;
      }
    }
  }

  return branch;
}
static uint32_t
thumb_condB_jump( uint16_t opcode, int *thumb_flag __attribute__((unused)), const uint32_t *reg_set) {
  uint32_t dest_addr;
  int32_t change = opcode & 0xff;

  if ( opcode & 0x80) {
    change |= 0xffffff00;
  }
  change <<= 1;

  dest_addr = reg_set[PC] + change;

  return dest_addr;
}



#define THUMB_B_MASK 0xf800
#define THUMB_B 0xe000
/** Determine the destination address for an unconditional B branch instruction.
 */
static uint32_t
thumb_B_jump( uint16_t opcode, int *thumb_flag __attribute__((unused)), const uint32_t *reg_set) {
  uint32_t dest_addr;
  int32_t change = opcode & 0x7ff;

  if ( change & 0x400) {
    change |= 0xfffff800;
  }
  change <<= 1;

  dest_addr = reg_set[PC] + change;

  return dest_addr;
}


#define THUMB_BL_BLX1_MASK 0xf800
#define THUMB_BL 0xf800
#define THUMB_BLX1 0xe800
#define THUMB_BL_BLX1_H_MASK 0x1800
#define THUMB_BL_BLX1_H_10 0x1000
#define THUMB_BL_BLX1_H_11 0x1800
#define THUMB_BL_BLX1_H_01 0x0800
/** Determine the destination address for a BL or BLX1 branch instruction.
 */
static uint32_t
thumb_BL_BLX1_jump( uint16_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t change = opcode & 0x7ff;

  change <<= 1;
  dest_addr = reg_set[LR] + change;

  if ( (opcode & THUMB_BL_BLX1_H_MASK) == THUMB_BL_BLX1_H_01) {
    /* THUMB_BL_BLX1_H_01 - exchange */
    dest_addr &= 0xFFFFFFFC;
    *thumb_flag = 0;
  }

  return dest_addr;
}



#define THUMB_BX_BLX2_MASK 0xff87
#define THUMB_BX 0x4700
#define THUMB_BLX2 0x4780
#define THUMB_BX_BLX2_H2_RM_MASK 0x0078
/** Determine the destination address for a BX or BLX2 branch instruction.
 */
static uint32_t
thumb_BX_BLX2_jump( uint16_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  int reg_num = (opcode & THUMB_BX_BLX2_H2_RM_MASK) >> 3;

  dest_addr = reg_set[reg_num] & 0xfffffffe;

  *thumb_flag = (reg_set[reg_num] & 0x1);

  return dest_addr;
}

#define THUMB_POP_MASK 0xff00
#define THUMB_POP      0xbd00
#define THUMB_POP_REG_MASK 0x00ff
/** Determine the destination address for a POP instruction.
 */
static uint32_t
thumb_POP_jump( uint16_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  int reg_list = opcode & THUMB_POP_REG_MASK;
  int reg_count = 0;
  uint32_t pc_value_addr = reg_set[SP];

  while ( reg_list) {
    if ( reg_list & 0x1)
      reg_count += 1;
    reg_list >>= 1;
  }

  pc_value_addr += 4 * (reg_count);
  dest_addr = *(uint32_t *)pc_value_addr;

  *thumb_flag = (dest_addr & 0x1);

  return dest_addr & 0xfffffffe;
}

