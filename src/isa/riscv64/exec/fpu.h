#include "cpu/exec.h"
#include "../local-include/fpu.h"
#include "softfloat/softfloat.h"
#include "../local-include/csr.h"

float32_t f32_min(float32_t a, float32_t b){
  bool less = f32_lt_quiet(a, b) || (f32_eq(a, b) && (a.v & F32_SIGN));
  if(isNaNF32UI(a.v) && isNaNF32UI(b.v)) return f32(defaultNaNF32UI);
  else return(less || isNaNF32UI(b.v) ? a : b);
}

float64_t f64_min(float64_t a, float64_t b){
  bool less = f64_lt_quiet(a, b) || (f64_eq(a, b) && (a.v & F64_SIGN));
  if(isNaNF64UI(a.v) && isNaNF64UI(b.v)) return f64(defaultNaNF64UI);
  else return(less || isNaNF64UI(b.v) ? a : b);
}

float32_t f32_max(float32_t a, float32_t b){
  bool greater = f32_lt_quiet(b, a) || (f32_eq(b, a) && (b.v & F32_SIGN));
  if(isNaNF32UI(a.v) && isNaNF32UI(b.v)) return f32(defaultNaNF32UI);
  else return(greater || isNaNF32UI(b.v) ? a : b);
}

float64_t f64_max(float64_t a, float64_t b){
  bool greater = f64_lt_quiet(b, a) || (f64_eq(b, a) && (b.v & F64_SIGN));
  if(isNaNF64UI(a.v) && isNaNF64UI(b.v)) return f64(defaultNaNF64UI);
  else return(greater || isNaNF64UI(b.v) ? a : b);
}

static inline BUILD_EXEC_F(add);
static inline BUILD_EXEC_F(sub);
static inline BUILD_EXEC_F(mul);
static inline BUILD_EXEC_F(div);
static inline BUILD_EXEC_F(min);
static inline BUILD_EXEC_F(max);

static inline make_EHelper(fp_ld) {
  require_fp;
  rtl_lm(s, &s0, dsrc1, s->width);
  sfpr(id_dest->reg, &s0, s->width);
  switch (s->width) {
    case 8: print_asm_template2(fld); break;
    case 4: print_asm_template2(flw); break;
    default: assert(0);
  }
}

static inline make_EHelper(fp_st) {
  require_fp;
  rtl_sm(s, dsrc1, &id_dest->val, s->width);
  switch (s->width) {
    case 8: print_asm_template2(fsd); break;
    case 4: print_asm_template2(fsw); break;
    default: assert(0);
  }
}

static inline make_EHelper(fsgnj) {
  require_fp;
  require(s->isa.instr.fpu.rm < 3);
  
  bool is_neg = s->isa.instr.fpu.rm == 1;
  bool is_xor = s->isa.instr.fpu.rm == 2;

  if(s->width == 4){
    s0 = fsgnj32(id_src1->val, id_src2->val, is_neg, is_xor);
  }else{
    s0 = fsgnj64(id_src1->val, id_src2->val, is_neg, is_xor);
  }
  sfpr(id_dest->reg, &s0, s->width);

  switch (s->isa.instr.fpu.rm)
  {
  case 0: // sgnj
    if(s->width == 4){
      print_asm_template3(fsgnj.s);  
    }
    else{
      print_asm_template3(fsgnj.d);  
    }
    break;
  case 1: // sgnjn
    if(s->width == 4){
      print_asm_template3(fsgnjn.s);  
    }
    else{
      print_asm_template3(fsgnjn.d);  
    }
    break;
  case 2: // sgnjx
    if(s->width == 4){
      print_asm_template3(fsgnjx.s);  
    }
    else{
      print_asm_template3(fsgnjx.d);  
    }
    break;
  default:
    assert(0);
    break;
  }
}

static inline make_EHelper(fmin_max) {
  require( s->isa.instr.fpu.rm < 2 ); // min or max
  if(s->isa.instr.fpu.rm == 0)
    exec_fmin(pc);
  else
    exec_fmax(pc);
}

// fmv.x.d/fmv.x.w and fclass 
static inline make_EHelper(fmv_F_to_G){
  require_fp;
  require( s->isa.instr.fpu.rm < 2 );

  if(s->isa.instr.fpu.rm == 0){
    // fmv fpr to gpr
    rtl_sext(s, &s0, &id_src1->val, s->width);
    switch (s->width) {
      case 8: print_asm_template2(fmv.x.d); break;
      case 4: print_asm_template2(fmv.x.w); break;
      default: assert(0);
    }
  }
  else{
    // fclass
    if(s->width == 4) s0 = f32_classify(f32(id_src1->val));
    else s0 = f64_classify(f64(id_src1->val));
    switch (s->width) {
      case 8: print_asm_template2(fclass.d); break;
      case 4: print_asm_template2(fclass.s); break;
      default: assert(0);
    }
  }
  rtl_sr(s, id_dest->reg, &s0, 8);
}

static inline make_EHelper(fmv_G_to_F){
  require_fp;
  sfpr(id_dest->reg, &id_src1->val, s->width);
  switch (s->width) {
    case 8: print_asm_template2(fmv.d.x); break;
    case 4: print_asm_template2(fmv.w.x); break;
    default: assert(0);
  }
}

static inline make_EHelper(fcmp){
  require_fp;
  require(s->isa.instr.fpu.rm < 3);

  static bool (* f_table[3])(float32_t, float32_t) = {
    f32_le, f32_lt, f32_eq
  };
  static bool (* d_table[3])(float64_t, float64_t) = {
    f64_le, f64_lt, f64_eq
  };

  if(s->width == 4){
    s0 = (f_table[s->isa.instr.fpu.rm])(f32(id_src1->val), f32(id_src2->val));
  }else{
    s0 = (d_table[s->isa.instr.fpu.rm])(f64(id_src1->val), f64(id_src2->val));
  }
  rtl_sr(s, id_dest->reg, &s0, s->width);
  set_fp_exceptions;

  switch (s->isa.instr.fpu.rm)
  {
  case 0: // fle
    if(s->width == 4){
      print_asm_template3(fle.s);  
    }
    else{
      print_asm_template3(fle.d);  
    }
    break;
  case 1: // flt
    if(s->width == 4){
      print_asm_template3(flt.s);  
    }
    else{
      print_asm_template3(flt.d);  
    }
    break;
  case 2: // feq
    if(s->width == 4){
      print_asm_template3(feq.s);  
    }
    else{
      print_asm_template3(feq.d);  
    }
    break;
  default:
    assert(0);
    break;
  }
}

static inline make_EHelper(fsqrt){
  require_fp;
  require(s->isa.instr.i.rs2 == 0);

  softfloat_roundingMode = RM;

  if(s->width == 4){
    s0 = f32_sqrt(f32(id_src1->val)).v;
  }
  else{
    s0 = f64_sqrt(f64(id_src1->val)).v;
  }
  sfpr(id_dest->reg, &s0, s->width);
  set_fp_exceptions;
  switch (s->width)
  {
    case 8: print_asm_template2(fsqrt.d); break;
    case 4: print_asm_template2(fsqrt.s); break;
    default: assert(0);
  }
}

static inline make_EHelper(fcvt_F_to_G){
  require_fp;
  require(s->isa.instr.i.rs2 < 4);

  softfloat_roundingMode = RM;
  switch (s->isa.instr.i.rs2)
  {
    case 0: // fcvt.w.[s/d]
      if(s->width == 4){
        s0 = f32_to_i32(f32(id_src1->val), RM, true);
        print_asm_template2(fcvt.w.s);
      }
      else{
        s0 = f64_to_i32(f64(id_src1->val), RM, true);
        print_asm_template2(fcvt.w.d);
      }
      rtl_sext(s, &s0, &s0, 4);
      break;
    case 1: // fcvt.wu.[s/d]
      if(s->width == 4){
        s0 = f32_to_ui32(f32(id_src1->val), RM, true);
        print_asm_template2(fcvt.uw.s);
      }
      else{
        s0 = f64_to_ui32(f64(id_src1->val), RM, true);
        print_asm_template2(fcvt.uw.d);
      }
      rtl_sext(s, &s0, &s0, 4);
      break;
    case 2: // fcvt.l.[s/d]
      if(s->width == 4){
        s0 = f32_to_i64(f32(id_src1->val), RM, true);
        print_asm_template2(fcvt.l.s);
      }
      else{
        s0 = f64_to_i64(f64(id_src1->val), RM, true);
        print_asm_template2(fcvt.l.d);
      }
      break;
    case 3: // fcvt.ul.[s/d]
      if(s->width == 4){
        s0 = f32_to_ui64(f32(id_src1->val), RM, true);
        print_asm_template2(fcvt.ul.s);
      }
      else{
        s0 = f64_to_ui64(f64(id_src1->val), RM, true);
        print_asm_template2(fcvt.ul.d);
      }
      break;
  }
  rtl_sr(s, id_dest->reg, &s0, 8);
  set_fp_exceptions;
}

static inline make_EHelper(fcvt_G_to_F){
  require_fp;
  require(s->isa.instr.i.rs2 < 4);

  softfloat_roundingMode = RM;
  switch (s->isa.instr.i.rs2)
  {
    case 0: // fcvt.[s/d].w
      if(s->width == 4){
        s0 = i32_to_f32((int32_t)id_src1->val).v;
        print_asm_template2(fcvt.s.w);
      }
      else{
        s0 = i32_to_f64((int32_t)id_src1->val).v;
        print_asm_template2(fcvt.d.w);
      }
      break;
    case 1: // fcvt.[s/d].wu
      if(s->width == 4){
        s0 = ui32_to_f32((uint32_t)id_src1->val).v;
        print_asm_template2(fcvt.s.uw);
      }
      else{
        s0 = ui32_to_f64((uint32_t)id_src1->val).v;
        print_asm_template2(fcvt.d.uw);
      }
      break;
    case 2: // fcvt.[s/d].l
      if(s->width == 4){
        s0 = i64_to_f32(id_src1->val).v;
        print_asm_template2(fcvt.s.l);
      }
      else{
        s0 = i64_to_f64(id_src1->val).v;
        print_asm_template2(fcvt.d.l);
      }
      break;
    case 3: // fcvt.[s/d].ul
      if(s->width == 4){
        s0 = ui64_to_f32(id_src1->val).v;
        print_asm_template2(fcvt.s.ul);
      }
      else{
        s0 = ui64_to_f64(id_src1->val).v;
        print_asm_template2(fcvt.d.ul);
      }
      break;
  }
  sfpr(id_dest->reg, &s0, s->width);
  set_fp_exceptions;
}

static inline make_EHelper(fcvt_F_to_F){
  require_fp;
  require(
    s->isa.instr.fpu.fmt < 2 && 
    s->isa.instr.i.rs2 < 2 && 
    s->isa.instr.fpu.fmt != s->isa.instr.i.rs2
  );

  softfloat_roundingMode = RM;
  if(s->isa.instr.fpu.fmt == 0 && s->isa.instr.i.rs2 == 1){
    // fcvt.s.d
    s0 = f64_to_f32(f64(id_src1->val)).v;
    print_asm_template2(fcvt.s.d);
  }
  else if(s->isa.instr.fpu.fmt == 1 && s->isa.instr.i.rs2 == 0){
    // fcvt.d.s
    s0 = f32_to_f64(f32(id_src1->val)).v;
    print_asm_template2(fcvt.d.s);
  }
  else{
    assert(0);
  }
  sfpr(id_dest->reg, &s0, s->width);
  set_fp_exceptions;
}

static inline BUILD_EXEC_FM(fmadd);
static inline BUILD_EXEC_FM(fmsub);
static inline BUILD_EXEC_FM(fnmsub);
static inline BUILD_EXEC_FM(fnmadd);