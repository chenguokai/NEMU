#include <monitor/difftest.h>

extern FILE *fp;

static inline make_EHelper(jal) {
  rtl_li(s, ddest, s->seq_pc);
  rtl_j(s, s->jmp_pc);
  int jal_type = 1;
  if (id_dest->reg == 0) {
    jal_type = 2;
  }
  fprintf(fp, "%llx %d %llx 1\n", cpu.pc, jal_type, s->jmp_pc);
  print_asm_template2(jal);
}

static inline make_EHelper(jalr) {
  int jalr_type = 4; // Call by default
  if (id_src1->reg == 1 && id_src2->imm == 0 && id_dest->reg == 0) {
    jalr_type = 3; // Ret
  }
  fprintf(fp, "%llx %d %llx 1\n", cpu.pc, jalr_type, *s0);
  rtl_addi(s, s0, dsrc1, id_src2->imm);
#ifdef __ENGINE_interpreter__
  rtl_nemuandi(s, s0, s0, ~0x1lu);
#endif
  rtl_jr(s, s0);

  rtl_li(s, ddest, s->seq_pc);

#ifndef __DIFF_REF_NEMU__
  difftest_skip_dut(1, 2);
#endif

  print_asm_template3(jalr);
}

static inline make_EHelper(beq) {
  rtl_jrelop(s, RELOP_EQ, dsrc1, dsrc2, s->jmp_pc);
  print_asm_template3(beq);
}

static inline make_EHelper(bne) {
  rtl_jrelop(s, RELOP_NE, dsrc1, dsrc2, s->jmp_pc);
  print_asm_template3(bne);
}

static inline make_EHelper(blt) {
  rtl_jrelop(s, RELOP_LT, dsrc1, dsrc2, s->jmp_pc);
  print_asm_template3(blt);
}

static inline make_EHelper(bge) {
  rtl_jrelop(s, RELOP_GE, dsrc1, dsrc2, s->jmp_pc);
  print_asm_template3(bge);
}

static inline make_EHelper(bltu) {
  rtl_jrelop(s, RELOP_LTU, dsrc1, dsrc2, s->jmp_pc);
  print_asm_template3(bltu);
}

static inline make_EHelper(bgeu) {
  rtl_jrelop(s, RELOP_GEU, dsrc1, dsrc2, s->jmp_pc);
  print_asm_template3(bgeu);
}
