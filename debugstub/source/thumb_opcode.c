/*
 * $Id: thumb_opcode.c,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */
/** \file
 * \brief Decode jump causing Thumb instructions.
 */
#include <stdint.h>
#include <stdlib.h>

#include "opcode_decode.h"
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
      if ( conditionCheck_opcode( cond, cpsr)) {
	branch = MATCH_AND_JUMP_INSTR;
      }
    }
  }

  return branch;
}
static uint32_t
thumb_condB_jump( uint16_t opcode, int *thumb_flag __attribute__((unused)), const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t change = opcode & 0xff;

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
  uint32_t change = opcode & 0x7ff;

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


#define THUMB_HIGH_REG_INSTR_MASK 0xff87
#define THUMB_MOV_MASK THUMB_HIGH_REG_INSTR_MASK
#define THUMB_MOV      0x4687
#define THUMB_ADD_MASK THUMB_HIGH_REG_INSTR_MASK
#define THUMB_ADD      0x4487

#define Rm_MASK 0x0078
/** Determine the destination address for a MOV or ADD instruction.
 */
static uint32_t
thumb_MOV_ADD_jump( uint16_t opcode, int *thumb_flag __attribute__((unused)), const uint32_t *reg_set) {
  uint32_t dest_addr;
  int Rm = (opcode & Rm_MASK) >> 3;

  if ( (opcode & THUMB_MOV_MASK) == THUMB_MOV) {
    dest_addr = reg_set[Rm];
  }
  else {
    dest_addr = reg_set[PC] + reg_set[Rm];
  }

  return dest_addr;
}


/** A Thumb opcode jump check.
 */
struct thumb_jump_check {
  /** the value of the masked bits */
  uint16_t opcode_value;

  /** the mask used to determine the instruction */
  uint16_t opcode_mask;

  /** function to match opcode and indicate if a jump is made */
  //enum instruction_match (*opcode_match_and_jump)( uint16_t opcode, const uint32_t *reg_set);

  /** function to compute jump and mode change */
  uint32_t (*compute_jump)( uint16_t opcode, int *thumb_flag, const uint32_t *reg_set);
};


/** The table of Thumb jump instructions.
 */
static struct thumb_jump_check thumbJumpTestTable[] = {
  { THUMB_B,       THUMB_B_MASK, thumb_B_jump},
  { THUMB_BL,      THUMB_BL_BLX1_MASK, thumb_BL_BLX1_jump},
  { THUMB_BLX1,    THUMB_BL_BLX1_MASK, thumb_BL_BLX1_jump},
  { THUMB_BX,      THUMB_BX_BLX2_MASK, thumb_BX_BLX2_jump},
  { THUMB_BLX2,    THUMB_BX_BLX2_MASK, thumb_BX_BLX2_jump},
  { THUMB_POP,     THUMB_POP_MASK, thumb_POP_jump},
  { THUMB_MOV,     THUMB_MOV_MASK, thumb_MOV_ADD_jump},
  { THUMB_ADD,     THUMB_ADD_MASK, thumb_MOV_ADD_jump},
  { 0, 0, NULL}
};


/** Determine if the Thumb instruction causes a branch, i.e. changes r15 the program counter
 * and work out where it goes to.
 */
int
causeJump_thumb( uint16_t op_code, int *thumb_flag, const uint32_t *reg_set, uint32_t *dest_addr) {
  int branch_flag = 0;
  int test_index;
  enum instruction_match condB_match = thumb_condB( op_code, reg_set[CPSR]);

  if ( condB_match == MATCH_INSTR || condB_match == MATCH_AND_JUMP_INSTR) {
    /* special case of the conditional branch instruction (saves having a number
     * of entries in the jump table */
    if ( condB_match == MATCH_AND_JUMP_INSTR) {
      *dest_addr = thumb_condB_jump( op_code, thumb_flag, reg_set);
      branch_flag = 1;
    }
  }
  else {
    /* go through the jump instruction table to see if this opcode does jump and,
     * if so, where it goes to.
     */
    for ( test_index = 0; thumbJumpTestTable[test_index].opcode_mask != 0 &&
	    !branch_flag; test_index++) {
      struct thumb_jump_check *jump_test = &thumbJumpTestTable[test_index];

      LOG("Matching op %04x, %04x, %04x\n", op_code,
	  jump_test->opcode_mask, jump_test->opcode_value);

      if ( (op_code & jump_test->opcode_mask) == jump_test->opcode_value) {
	branch_flag = 1;

	*dest_addr = jump_test->compute_jump( op_code, thumb_flag, reg_set);

	LOG("thumb branch %08x (thumb %d) (%d)\n", *dest_addr, *thumb_flag,
	    test_index);
      }
    }
  }

  return branch_flag;
}
