/***************************************************************************************
* Copyright (c) 2014-2021 Zihao Yu, Nanjing University
* Copyright (c) 2020-2022 Institute of Computing Technology, Chinese Academy of Sciences
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <cpu/exec.h>
#include <difftest.h>
#include "../local-include/intr.h"
#include "../local-include/csr.h"

// csr_prepare() & csr_writeback() are used to maintain 
// a compact mirror of critical CSRs
// For processor difftest only 
static void csr_prepare() {
  cpu.mstatus = mstatus->val;
  cpu.mcause  = mcause->val;
  cpu.mepc    = mepc->val;

  cpu.sstatus = csrid_read(0x100); // sstatus
  cpu.scause  = scause->val;
  cpu.sepc    = sepc->val;

  cpu.satp     = satp->val;
  cpu.mip      = mip->val;
  cpu.mie      = mie->val;
  cpu.mscratch = mscratch->val;
  cpu.sscratch = sscratch->val;
  cpu.mideleg  = mideleg->val;
  cpu.medeleg  = medeleg->val;
  cpu.mtval    = mtval->val;
  cpu.stval    = stval->val;
  cpu.mtvec    = mtvec->val;
  cpu.stvec    = stvec->val;
#ifdef CONFIG_RVV_010
  cpu.vtype   = vtype->val;
  cpu.vstart  = vstart->val;
  cpu.vxsat   = vxsat->val;
  cpu.vxrm    = vxrm->val;
  cpu.vl      = vl->val;
#endif // CONFIG_RVV_010
}

static void csr_writeback() {
  mstatus->val = cpu.mstatus;
  mcause ->val = cpu.mcause ;
  mepc   ->val = cpu.mepc   ;
  //sstatus->val = cpu.sstatus;  // sstatus is a shadow of mstatus
  scause ->val = cpu.scause ;
  sepc   ->val = cpu.sepc   ;

  satp->val     = cpu.satp;
  mip->val      = cpu.mip;
  mie->val      = cpu.mie;
  mscratch->val = cpu.mscratch;
  sscratch->val = cpu.sscratch;
  mideleg->val  = cpu.mideleg;
  medeleg->val  = cpu.medeleg;
  mtval->val    = cpu.mtval;
  stval->val    = cpu.stval;
  mtvec->val    = cpu.mtvec;
  stvec->val    = cpu.stvec;
#ifdef CONFIG_RVV_010
  vtype->val   = cpu.vtype;
  vstart->val  = cpu.vstart;
  vxsat->val   = cpu.vxsat;
  vxrm->val    = cpu.vxrm;
  vl->val      = cpu.vl;
#endif //CONFIG_RVV_010
}

extern uint64_t stable_log_begin, spec_log_begin;

void isa_difftest_regcpy(void *dut, bool direction, bool restore, uint64_t restore_count) {
  if (restore) {
    printf("restore count %lu\n", restore_count);
    uint64_t left_exec = lightqs_restore_reg_snapshot(restore_count);
    printf("left exec = %lx\n", left_exec);
    pmem_record_restore(restore_count);
    // clint_restore_snapshot(restore_count);

    if (spec_log_begin <= restore_count) {
      // must have restored using spec snapshot
      stable_log_begin = spec_log_begin;
    }
    cpu_exec(left_exec);
  }

  if (direction == DIFFTEST_TO_REF) {
    memcpy(&cpu, dut, DIFFTEST_REG_SIZE);
    csr_writeback();
  } else {
    csr_prepare();
    memcpy(dut, &cpu, DIFFTEST_REG_SIZE);
  }

  // after processing, take another snapshot
  // FIXME: update spec_log_begin
  if (restore) {
    lightqs_take_reg_snapshot();
    clint_take_snapshot();
    // pmem ops are logged automatically
    cpu_exec(AHEAD_LENGTH);
    stable_log_begin = restore_count;
    spec_log_begin = restore_count + AHEAD_LENGTH;
    lightqs_take_spec_reg_snapshot();
    clint_take_spec_snapshot();
  }
  
}

void isa_difftest_csrcpy(void *dut, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    memcpy(csr_array, dut, 4096 * sizeof(rtlreg_t));
  } else {
    memcpy(dut, csr_array, 4096 * sizeof(rtlreg_t));
  }
}

void isa_difftest_uarchstatus_cpy(void *dut, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    struct SyncState* ms = (struct SyncState*)dut;
    cpu.lr_valid = ms->lrscValid;
  } else {
    struct SyncState ms;
    ms.lrscValid = cpu.lr_valid;
    ms.lrscAddr = cpu.lr_addr;
    memcpy(dut, &ms, sizeof(struct SyncState));
  }
}

void isa_difftest_raise_intr(word_t NO, uint64_t restore_count) {

  uint64_t left_exec = lightqs_restore_reg_snapshot(restore_count);
  pmem_record_restore(restore_count);
  // clint_restore_snapshot(restore_count);

  if (spec_log_begin <= restore_count) {
    // must have restored using spec snapshot
    stable_log_begin = spec_log_begin;
  }
  cpu_exec(left_exec);

  cpu.pc = raise_intr(NO, cpu.pc);

  
  // after processing, take another snapshot
  // FIXME: update spec_log_begin
  lightqs_take_reg_snapshot();
  clint_take_snapshot();
  // pmem ops are logged automatically
  stable_log_begin = restore_count;
  spec_log_begin = restore_count + AHEAD_LENGTH;
  cpu_exec(AHEAD_LENGTH);
  
  lightqs_take_spec_reg_snapshot();
  clint_take_spec_snapshot();
}

#ifdef CONFIG_GUIDED_EXEC
void isa_difftest_guided_exec(void * guide, uint64_t restore_count) {

  uint64_t left_exec = lightqs_restore_reg_snapshot(restore_count);
  pmem_record_restore(restore_count);
  // clint_restore_snapshot(restore_count);

  if (spec_log_begin <= restore_count) {
    // must have restored using spec snapshot
    stable_log_begin = spec_log_begin;
  }
  cpu_exec(left_exec);

  memcpy(&cpu.execution_guide, guide, sizeof(struct ExecutionGuide));

  cpu.guided_exec = true;
  cpu_exec(1);
  cpu.guided_exec = false;
  
  // after processing, take another snapshot
  // FIXME: update spec_log_begin
  lightqs_take_reg_snapshot();
  clint_take_snapshot();
  // pmem ops are logged automatically
  cpu_exec(AHEAD_LENGTH);
  stable_log_begin = restore_count;
  spec_log_begin = restore_count + AHEAD_LENGTH;
  lightqs_take_spec_reg_snapshot();
  clint_take_spec_snapshot();
}
#endif

#ifdef CONFIG_QUERY_REF
void isa_difftest_query_ref(void *result_buffer, uint64_t type) {
  size_t size;
  switch(type) {
    case REF_QUERY_MEM_EVENT:
      cpu.query_mem_event.pc = cpu.debug.current_pc; // update pc
      size = sizeof(cpu.query_mem_event);
      memcpy(result_buffer, &cpu.query_mem_event, size);
      // nemu result buffer will be flushed after query 
      // printf_with_pid("mem_access %x\n", cpu.query_mem_event.mem_access);
      // printf_with_pid("mem_access_is_load %x\n", cpu.query_mem_event.mem_access_is_load);
      // printf_with_pid("mem_access_vaddr %lx\n", cpu.query_mem_event.mem_access_vaddr);
      memset(&cpu.query_mem_event, 0, size);
      break;
    default:
      panic("Invalid ref query type");
  }
}
#endif

char *reg_dump_file = NULL;

void dump_regs() {
  if (reg_dump_file == NULL) {
    printf("No register dump file is specified, register dump skipped.\n");
    return ;
  }
  FILE *fp = fopen(reg_dump_file, "w");
  if (fp == NULL) {
    printf("Cannot open file %s, register dump skipped.\n", reg_dump_file);
    return ;
  }
  fprintf(fp, "mstatus %lx\n", cpu.mstatus);
  fprintf(fp, "mcause %lx\n", cpu.mcause);
  fprintf(fp, "mepc %lx\n", cpu.mepc);
  fprintf(fp, "sstatus %lx\n", cpu.sstatus);
  fprintf(fp, "scause %lx\n", cpu.scause);
  fprintf(fp, "sepc %lx\n", cpu.sepc);
  fprintf(fp, "satp %lx\n", cpu.satp);
  fprintf(fp, "mip %lx\n", cpu.mip);
  fprintf(fp, "mie %lx\n", cpu.mie);
  fprintf(fp, "mscratch %lx\n", cpu.mscratch);
  fprintf(fp, "sscratch %lx\n", cpu.sscratch);
  fprintf(fp, "mideleg %lx\n", cpu.mideleg);
  fprintf(fp, "medeleg %lx\n", cpu.medeleg);
  fprintf(fp, "mtval %lx\n", cpu.mtval);
  fprintf(fp, "stval %lx\n", cpu.stval);
  fprintf(fp, "mtvec %lx\n", cpu.mtvec);
  fprintf(fp, "stvec %lx\n", cpu.stvec);
#ifdef CONFIG_RVV_010
  fprintf(fp, "vtype %lx\n", vtype);
  fprintf(fp, "vstart %lx\n", vstart);
  fprintf(fp, "vxsat %lx\n", vxsat);
  fprintf(fp, "vxrm %lx\n", vxrm);
  fprintf(fp, "vl %lx\n", vl);
#endif // CONFIG_RVV_010
  for (int i = 0; i < 32; i++) {
    fprintf(fp, "gpr %d %lx\n", i, cpu.gpr[i]._64);
  }
  for (int i = 0; i < 32; i++) {
    fprintf(fp, "fpr %d %lx\n", i, cpu.fpr[i]._64);
  }
}

#ifdef CONFIG_MULTICORE_DIFF
void isa_difftest_set_mhartid(int n) {
  mhartid->val = n;
}
#endif
