#ifndef _OPCODE_DECODE_H_
#define _OPCODE_DECODE_H_ 1
/*
 * $Id: opcode_decode.h,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */
/** \file
 */

#define ARM_CONDITION_MASK 0xf0000000
#define ARM_CONDITION_EXTD 0xf0000000


#define R0 0
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R6 6
#define R7 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define SP 13
#define R14 14
#define LR 14
#define R15 15
#define R15 15
#define PC 15
#define CPSR 16


/*
 * The CPSR Flag masks
 */
#define CPSR_N_FLAG 0x80000000
#define CPSR_Z_FLAG 0x40000000
#define CPSR_C_FLAG 0x20000000
#define CPSR_V_FLAG 0x10000000
#define CPSR_Q_FLAG 0x08000000



/** Indicate if an instruction match is made and if the instruction causes
 * a change to the PC.
 */
enum instruction_match {
  NO_MATCH_INSTR,
  MATCH_INSTR,
  MATCH_AND_JUMP_INSTR
};


int
conditionCheck_opcode( uint32_t condition, uint32_t cpsr);


int
instructionExecuted_opcode( uint32_t instr_addr, int thumb_flag, uint32_t cpsr);


int
causeJump_opcode( uint32_t instr_addr, int *thumb_flag, const uint32_t *reg_set, uint32_t *dest_addr);

int
causeJump_arm( uint32_t op_code, int *thumb_flag, const uint32_t *reg_set, uint32_t *dest_addr);

int
causeJump_thumb( uint16_t op_code, int *thumb_flag, const uint32_t *reg_set, uint32_t *dest_addr);



#endif /* End of _OPCODE_DECODE_H_ */
