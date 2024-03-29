#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include "insns.h"
#include "insgen.h"
#include <stdlib.h>

#define DIRTY_EXTRACT 0

//#define TRACE_EXEC

extern const uint8_t itype[];

int get_assemble_maxlen(int len, int ilen, int blen) // {{{
{
  return len + ilen + blen;
}
// }}}

// ret must be allocated at least of size get_assemble_maxlength()
// returns used olen  or -1: alloc failed, -2: idx/len error
static int do_assemble(intptr_t *ret, const uint8_t *ins, int len, const int32_t *imms, int ilen, const int32_t *brts, int blen, const void *const insns[]) // {{{
{
  if (!ret) {
    return -1;
  }

  const int olen = get_assemble_maxlen(len, ilen, blen);

  int32_t *addrs = (int32_t *)malloc(olen * sizeof(int32_t));
  if (!addrs) {
    return -1;
  }

  int32_t *bfixup = (int32_t *)malloc(blen * sizeof(int32_t));
  if (!bfixup) {
    free(addrs);
    return -1;
  }

  // 1. generate output and addresses
  int iidx = 0, bidx = 0;
  for (int ipos = 0, opos = 0; ipos < len; ipos++) { // len is maxlen/olen. I_END must come before!
    const uint8_t op = ins[ipos];

    addrs[ipos] = opos;
    if (op != I_END) {
      ret[opos++] = (const intptr_t)insns[op];

    } else { // done - replace I_END with I_RET.
      ret[opos++] = (const intptr_t)insns[I_RET];

      // 2. fix branches
      for (int i = 0; i < bidx; i++) {
        // assert(brs[i] >= 0 && brts[i] < len);
        ret[bfixup[i]] = addrs[brts[i]] - (ret[bfixup[i]] + 1);
      }

      free(bfixup);
      free(addrs);
      return opos;
    }

    const int type = itype[op];
    if (ITYPE_IS_BRANCH(type)) {
      if (bidx >= blen) {
        goto error;
      }
      bfixup[bidx++] = opos++;   // ret[opos] will be filled in 2.
    }

    // copy immediate to output
    const int extralen = ITYPE_EXTRALENGTH(type);
    for (int i = 0; i < extralen; i++) {
      if (iidx >= ilen) {
        goto error;
      }
      ret[opos++] = imms[iidx++];
    }
  }

error:
  free(bfixup);
  free(addrs);
  return -2;  // idx/len error
}
// }}}

static intptr_t do_run(const intptr_t *ip, void *const mem, int len) // {{{
{
  int32_t r1 = 0, r2 = 0, r3 = 0, r4 = 0;

  (void)r1; (void)r2; (void)r3; (void)r4;  // mark used

#if DIRTY_EXTRACT == 1
  __attribute__((used, section("rc_insns_data")))
#endif
  static const void *const insns[]
#if DIRTY_EXTRACT == 2
  __asm__("rc_insns_data")
  __attribute__((used))
#endif
  = {
#define X(name, code, type)   &&insn_ ## name,
    INSNS(X)
#undef X
  };
#if DIRTY_EXTRACT == 2
//  __asm__ ("1: .section rc_insns_addr, \"a\", @progbits \n\t .long 1b \n\t .previous"); // store address in code...
//  __asm__ (".section rc_insns_addr, \"a\", @progbits \n\t .long %P0 \n\t .previous" : : "m"(insns));  // NOTE: only works with -fno-pic
  // or: .pushsection / .popsection   // TODO? 1: .long 1b ?   (<- i.e. jump target. does work.)
#elif DIRTY_EXTRACT != 1
  if (!mem) {
    return (const intptr_t)&insns;
  }
#endif

  goto *(const void *)*ip;

#ifdef TRACE_EXEC
#  define X(name, code, type)   insn_ ## name: printf("%s\n", #name); code; goto *(const void*)*++ip;
#else
#  define X(name, code, type)   insn_ ## name: code; goto *(const void*)*++ip;
#endif
  INSNS(X)
#undef X
}
// }}}


int assemble_code(intptr_t *ret, const uint8_t *ins, int len, const int32_t *imms, int ilen, const int32_t *brts, int blen) // {{{
{
#if DIRTY_EXTRACT == 1
  extern const void *const __start_rc_insns_data[];
#elif DIRTY_EXTRACT == 2
  extern const void *const rc_insns_data[] __asm__("rc_insns_data");
#endif

#if DIRTY_EXTRACT == 1
  const void *const *insns = __start_rc_insns_data;
#elif DIRTY_EXTRACT == 2
  const void *const *insns = rc_insns_data;
#else
  const void *const *insns = (const void *const *)do_run(NULL, NULL, 0);
#endif

  return do_assemble(ret, ins, len, imms, ilen, brts, blen, insns);
}
// }}}

int32_t run_code(const intptr_t *const ip, void *const mem, int len) // {{{
{
  // assert(mem);
  return do_run(ip, mem, len);
}
// }}}


intptr_t *insgen_assemble(insgen_t *gen) // {{{
{
  const int maxlen = get_assemble_maxlen(gen->len, gen->ilen, gen->blen);
  intptr_t *ret = (intptr_t *)malloc(maxlen * sizeof(intptr_t));
  if (!ret) {
    return NULL;
  }
  if (assemble_code(ret, gen->ins, gen->len, gen->imms, gen->ilen, gen->brtargets, gen->blen) < 0) {
    free(ret);
    return NULL;
  }
  return ret;
}
// }}}

int main()
{
  insgen_t gen = {};

// int32_t l1 = insgen_label(&gen);
// insgen_label_bind(&gen, l1);

// int32_t l1 = insgen_label_here(&gen);

#if 0
//  insgen_add(&gen, I_ADD);
//  insgen_add(&gen);  // nop
  insgen_add(&gen, I_ADDI, 12);
//  insgen_add(&gen, I_ADDI, 12);
//  insgen_add(&gen, I_RET);
#else
  int32_t l1 = insgen_label(&gen);

  insgen_add(&gen, I_JMP, l1);

  insgen_add(&gen, I_ADDI, 12);

  insgen_label_bind(&gen, l1);

  insgen_add(&gen);  // nop
  insgen_add(&gen, I_RET);

#endif

  if (insgen_finish(&gen) < 0) {
    fprintf(stderr, "Insgen failed: %d\n", gen.len);
    return 1;
  }

  insgen_dump(&gen);

  intptr_t *code = insgen_assemble(&gen);
  uint8_t mem[100] = {};

  const int32_t res = run_code(code, mem, sizeof(mem)/sizeof(*mem));
  printf("%d\n", res);

  free(code);

  return 0;
}
