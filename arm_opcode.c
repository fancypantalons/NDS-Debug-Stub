/*
 * $Id: arm_opcode.c,v 1.1.1.1 2006/09/06 10:13:18 ben Exp $
 */
/** \file
 * \brief
 */
#include <stdint.h>
#include <stdlib.h>

#include "opcode_decode.h"
#include "logging.h"


static uint32_t
getSP( void) {
  uint32_t reg_value;
  asm ( "mov %0, sp\n"
	: "=r"(reg_value)
	:
	);

  return reg_value;
}

#define ARM_B_BL_MASK 0x0e000000
#define ARM_B_BL 0x0a000000
/** Determine the destination address for an B or BL branch instruction.
 */
static uint32_t
arm_B_BL_jump( uint32_t opcode, int *thumb_flag __attribute__((unused)), const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t change = opcode & 0xffffff;

  if ( change & 0x800000) {
    /* sign extend */
    change |= 0xff000000;
  }

  change <<= 2;
  dest_addr = reg_set[PC] + change;

  return dest_addr;
}

#define ARM_BLX1_MASK 0xfe000000
#define ARM_BLX1      0xfa000000
/** Determine the destination address for an BLX1 branch instruction.
 */
static uint32_t
arm_BLX1_jump( uint32_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t change = opcode & 0xffffff;

  *thumb_flag = 1;

  if ( change & 0x800000) {
    /* sign extend */
    change |= 0xff000000;
  }

  change <<= 2;
  /* add the change value and the H bit value shifted */
  dest_addr = reg_set[PC] + change + ((opcode & 0x01000000) ? (1 << 1) : 0);

  return dest_addr;
}


#define ARM_BLX2_MASK 0x0ffffff0
#define ARM_BLX2      0x012fff30
#define ARM_BX_MASK   0x0ffffff0
#define ARM_BX        0x012fff10
/** Determine the destination address for an BLX1 branch instruction.
 */
static uint32_t
arm_BLX2_BX_jump( uint32_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t addr_reg = opcode & 0xf;
  uint32_t step_addr = reg_set[addr_reg];

  /* set the thumb state of the destination */
  if ( step_addr & 0x1) {
    *thumb_flag = 1;
  }

  dest_addr = step_addr & 0xfffffffe;
  return dest_addr;
}


#define ARM_LDR_MASK 0x0c50f000
#define ARM_LDR      0x0410f000
#define ARM_LDR_Rn_MASK 0x000f0000
#define ARM_LDR_Rm_MASK 0x0000000f
#define ARM_LDR_I_BIT 0x02000000
#define ARM_LDR_P_BIT 0x01000000
#define ARM_LDR_U_BIT 0x00800000
#define ARM_LDR_OFFSET_MASK 0x00000fff
#define ARM_LDR_SHIFT_MASK 0x00000ff0
#define ARM_LDR_SHIFT_TYPE_MASK 0x00000060
#define ARM_LDR_SHIFT_IMM_MASK 0x00000f80
/** Determine the destination address for a LDR instruction.
 */
static uint32_t
arm_LDR_jump( uint32_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t Rn_reg = (opcode & ARM_LDR_Rn_MASK) >> 16;
  uint32_t base_addr = reg_set[Rn_reg];

  if ( opcode & ARM_LDR_P_BIT) {
    if ( opcode & ARM_LDR_I_BIT) {
      uint32_t Rm_reg = (opcode & ARM_LDR_Rm_MASK);
      uint32_t Rm_value = reg_set[Rm_reg];

      if ( opcode & ARM_LDR_SHIFT_MASK) {
	uint32_t index = 0;
	uint32_t shift_type = (opcode & ARM_LDR_SHIFT_TYPE_MASK) >> 5;
	uint32_t shift_imm = (opcode & ARM_LDR_SHIFT_IMM_MASK) >> 7;

	switch ( shift_type) {
	case 0x0: /* LSL */
	  index = Rm_value << shift_imm;
	  break;

	case 0x1: /* LSR */
	  if ( shift_imm > 0) {
	    index = Rm_value >> shift_imm;
	  }
	  break;

	case 0x2: /* ASR */
	  if ( shift_imm == 0) {
	    if ( Rm_value & 0x80000000) {
	      index = 0xffffffff;
	    }
	  }
	  else {
	    int32_t temp = Rm_value;

	    temp >>= shift_imm;
	    index = temp;
	  }
	  break;

	case 0x3: /* ROR or RRX */
	  if ( shift_imm == 0) { /* RRX */
	    index = Rm_value >> 1;
	    if ( reg_set[CPSR] & CPSR_C_FLAG)
	      index |= 0x80000000;
	  }
	  else { /* ROR */
	    index = Rm_value;
	    while ( shift_imm > 0) {
	      uint32_t bit = index & 0x1;

	      index >>= 1;
	      index |= bit << 31;
	      shift_imm -= 1;
	    }
	  }
	  break;
	}

	if ( opcode & ARM_LDR_U_BIT) {
	  base_addr += index;
	}
	else {
	  base_addr -= index;
	}
      }
      else {
	/* no shifting */
	if ( opcode & ARM_LDR_U_BIT) {
	  base_addr += Rm_value;
	}
	else {
	  base_addr -= Rm_value;
	}
      }
    }
    else {
      /* immediate */
      if ( opcode & ARM_LDR_U_BIT) {
	base_addr += opcode & ARM_LDR_OFFSET_MASK;
      }
      else {
	base_addr -= opcode & ARM_LDR_OFFSET_MASK;
      }
    }
  }
  else {
    /* post-indexing - just use the register value */
  }

  dest_addr = base_addr;

  /* set the thumb state of the destination */
  if ( dest_addr & 0x1) {
    *thumb_flag = 1;
  }

  dest_addr = dest_addr & 0xfffffffe;
  return dest_addr;
}


#define ARM_LDM_MASK 0x0e108000
#define ARM_LDM      0x08108000
#define ARM_LDM_REG_LIST 0x0000ffff
#define ARM_LDM_P_BIT (1 << 24)
#define ARM_LDM_U_BIT (1 << 23)
#define ARM_LDM_S_BIT (1 << 22)
/** Determine the destination address for a LDM instruction.
 */
static uint32_t
arm_LDM_jump( uint32_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t dest_addr;
  uint32_t addr_reg = (opcode & 0x000f0000) >> 16;
  uint32_t pc_value_addr = reg_set[addr_reg];
  uint32_t reg_list = opcode & ARM_LDM_REG_LIST;
  int reg_count = 0;

  while ( reg_list) {
    if ( reg_list & 0x1)
      reg_count += 1;
    reg_list >>= 1;
  }


  if ( opcode & ARM_LDM_P_BIT) {
    if ( opcode & ARM_LDM_U_BIT) {
      /* Increment before */
      pc_value_addr += reg_count * 4;
    }
    else {
      /* Decrement before */
      pc_value_addr -= 4;
    }
  }
  else {
    if ( opcode & ARM_LDM_U_BIT) {
      /* Increment after */
      pc_value_addr += (reg_count * 4) - 4;
    }
    else {
      /* Decrement after - no change needed */
    }
  }

  dest_addr = *(uint32_t *)pc_value_addr;

  /* set the thumb state of the destination */
  if ( opcode & ARM_LDM_S_BIT) {
    /* FIXME: need access to the excepted mode's SPSR */
  }
  else {
    /* thumb mode indicated by state of bit 0 of destination */
    *thumb_flag = dest_addr & 0x1;
  }

  dest_addr = dest_addr & 0xfffffffe;
  return dest_addr;
}


#define ARM_ADD_MASK 0x0de0f000
#define ARM_ADD      0x0080f000
#define ARM_ADD_Rn_MASK 0x000f0000
#define ARM_ADD_I_BIT 0x02000000
#define ARM_ADD_S_BIT 0x00100000
/** Determine the destination address for an ADD instruction.
 */
static uint32_t
arm_ADD_jump( uint32_t opcode, int *thumb_flag, const uint32_t *reg_set) {
  uint32_t Rn_reg = (opcode & ARM_ADD_Rn_MASK) >> 16;
  uint32_t dest_addr = reg_set[PC];

  return dest_addr;
}



/** A ARM opcode jump check.
 */
struct arm_jump_check {
  /** the value of the masked bits */
  uint32_t opcode_value;

  /** the mask used to determine the instruction */
  uint32_t opcode_mask;

  /** function to match opcode and indicate if a jump is made */
  //enum instruction_match (*opcode_match_and_jump)( uint16_t opcode, const uint32_t *reg_set);

  /** function to compute jump and mode change */
  uint32_t (*compute_jump)( uint32_t opcode, int *thumb_flag, const uint32_t *reg_set);
};



/** The tables of ARM jump instructions.
 */
static struct arm_jump_check armJumpTestTable_cond[] = {
  { ARM_B_BL,       ARM_B_BL_MASK, arm_B_BL_jump},
  { ARM_BLX2,       ARM_BLX2_MASK, arm_BLX2_BX_jump},
  { ARM_BX,         ARM_BX_MASK, arm_BLX2_BX_jump},
  { ARM_LDR,        ARM_LDR_MASK, arm_LDR_jump},
  { ARM_LDM,        ARM_LDM_MASK, arm_LDM_jump},
  { ARM_ADD,        ARM_ADD_MASK, arm_ADD_jump},
  //  { ARM_SUB,        ARM_SUB_MASK, arm_SUB_jump},
  { 0, 0, NULL}
};

static struct arm_jump_check armJumpTestTable_extd[] = {
  { ARM_BLX1,       ARM_BLX1_MASK, arm_BLX1_jump},
  { 0, 0, NULL}
};


/** Determine if the ARM instruction causes a branch, i.e. changes r15 the program counter
 * and work out where it goes to.
 */
int
causeJump_arm( uint32_t op_code, int *thumb_flag, const uint32_t *reg_set, uint32_t *dest_addr) {
  int branch_flag = 0;
  const struct arm_jump_check *jump_table;
  int test_index;

  LOG( "ARM opcode %08x\n", op_code);

  if ( (op_code & ARM_CONDITION_MASK) != ARM_CONDITION_EXTD) {
    /* use the table for conditional instructions */
    jump_table = armJumpTestTable_cond;
  }
  else {
    /* use the table for extended instructions */
    jump_table = armJumpTestTable_extd;
  }

  /* go through the jump instruction table to see if this opcode does jump and,
   * if so, where it goes to.
   */
  for ( test_index = 0; jump_table[test_index].opcode_mask != 0 &&
	  !branch_flag; test_index++) {
    const struct arm_jump_check *jump_test = &jump_table[test_index];

    LOG("sp = %08x - ", getSP());
    LOG("Matching op %08x, mask %08x, val %08x res %08x\n", op_code,
	jump_test->opcode_mask, jump_test->opcode_value,
	op_code & jump_test->opcode_mask);

    if ( (op_code & jump_test->opcode_mask) == jump_test->opcode_value) {
      branch_flag = 1;

      *dest_addr = jump_test->compute_jump( op_code, thumb_flag, reg_set);

      LOG("arm branch %08x (thumb %d) (%d)\n", *dest_addr, *thumb_flag,
	  test_index);
    }
  }

  return branch_flag;
}
