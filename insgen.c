#include "insgen.h"
#include "insns.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

const uint8_t itype[] = {
#define X(name, code, type)   type,
  INSNS(X)
#undef X
};

static int insgen_destroy_impl(insgen_t *gen, int32_t err) // {{{
{
  assert(gen);  //  && gen->len >= 0)  ?
  free(gen->ins);
  free(gen->imms);
  free(gen->brtargets);
  free(gen->labels);
  memset(gen, 0, sizeof(*gen));
  gen->len = err; // error indicator
  return err;
}
// }}}

void insgen_destroy(insgen_t *gen) // {{{
{
  insgen_destroy_impl(gen, 0);
}
// }}}

static int _insgen_ins(insgen_t *gen, uint8_t op) // {{{
{
  if (gen->len >= gen->size) {
    gen->size = gen->size * 2 + 50;  // TODO?
    void *tmp = realloc(gen->ins, gen->size * sizeof(*gen->ins));
    if (!tmp) {
      return -1;
    }
    gen->ins = (uint8_t *)tmp;
  }

  gen->ins[gen->len] = op;
  return gen->len++;
}
// }}}

static int _insgen_imm(insgen_t *gen, int num) // {{{
{
  if (gen->ilen + num > gen->isize) {
    gen->isize = gen->isize * 1.5 + num + 50;  // TODO?
    void *tmp = realloc(gen->imms, gen->isize * sizeof(*gen->imms));
    if (!tmp) {
      return -1;
    }
    gen->imms = (int32_t *)tmp;
  }

  const int ret = gen->ilen;
  gen->ilen += num;
  return ret;
}
// }}}

static int _insgen_brt(insgen_t *gen, int32_t lidx) // {{{
{
  if (gen->blen >= gen->bsize) {
    gen->bsize = gen->bsize * 1.5 + 100;  // TODO?
    void *tmp = realloc(gen->brtargets, gen->bsize * sizeof(*gen->brtargets));
    if (!tmp) {
      return -1;
    }
    gen->brtargets = (int32_t *)tmp;
  }

  gen->brtargets[gen->blen] = lidx;
  return gen->blen++;
}
// }}}

static int _insgen_lab(insgen_t *gen, int32_t ipos) // {{{
{
  if (gen->llen >= gen->lsize) {
    gen->lsize = gen->lsize * 1.5 + 100;  // TODO?
    void *tmp = realloc(gen->labels, gen->lsize * sizeof(*gen->labels));
    if (!tmp) {
      return -1;
    }
    gen->labels = (int32_t *)tmp;
  }

  gen->labels[gen->llen] = ipos;
  return gen->llen++;
}
// }}}

int insgen_add_impl(insgen_t *gen, int argc, struct insgen_add_args_t args) // {{{
{
  assert(gen && gen->len >= 0);
  if (!gen || gen->len < 0) {
    return -1;
  }

  const uint8_t op = args.op;
  if (op >= sizeof(itype)/sizeof(*itype)) {
    return insgen_destroy_impl(gen, -2);
  }

  if (_insgen_ins(gen, op) < 0) {
    return insgen_destroy_impl(gen, -1);
  }

  const int type = itype[op];
  if (ITYPE_IS_BRANCH(type)) {
    const int32_t lidx = args.arg0; // safe, because .arg0 is at least zero-initialized
    if (argc < 1 || lidx < 0 || lidx >= gen->llen) {
      return insgen_destroy_impl(gen, -2);
    }
    if (_insgen_brt(gen, lidx) < 0) {
      return insgen_destroy_impl(gen, -1);
    }
    args.arg0 = args.arg1;
    args.arg1 = args.arg2;
    args.arg2 = args.arg3;
    argc--;
  }

  const int extralen = ITYPE_EXTRALENGTH(type);
  if (argc != extralen) {
    return insgen_destroy_impl(gen, -2);
  }
  if (extralen) {
    const int iidx = _insgen_imm(gen, extralen);
    if (iidx < 0) {
      return insgen_destroy_impl(gen, -1);
    }
    switch (extralen) {
    case 3:
      gen->imms[iidx + 2] = args.arg2;
      // fallthru
    case 2:
      gen->imms[iidx + 1] = args.arg1;
      // fallthru
    case 1:
      gen->imms[iidx] = args.arg0;
    }
  }

  return 0;
}
// }}}

static int32_t insgen_label_impl(insgen_t *gen, int here) // {{{
{
  assert(gen && gen->len >= 0);
  if (!gen || gen->len < 0) {
    return -1;
  }

  const int32_t ret = _insgen_lab(gen, here ? gen->len : -1);
  if (ret < 0) {
    return insgen_destroy_impl(gen, -1);
  }
  return ret;
}
// }}}

int32_t insgen_label(insgen_t *gen) // {{{
{
  return insgen_label_impl(gen, 0);
}
// }}}

int32_t insgen_label_here(insgen_t *gen) // {{{
{
  return insgen_label_impl(gen, 1);
}
// }}}

void insgen_label_bind(insgen_t *gen, int32_t lidx) // {{{
{
  assert(gen && gen->len >= 0);
  assert(lidx >= 0 && lidx < gen->llen);
  if (!gen || gen->len < 0 || lidx < 0 || lidx >= gen->llen) {
    return;
  }
  assert(gen->labels[lidx] == -1); // we don't want rebinds ...
  gen->labels[lidx] = gen->len;
}
// }}}

int insgen_finish(insgen_t *gen) // {{{
{
  if (!gen || gen->len < 0) {
    return -1;
  }

  // finish with sure I_END, also last label now surely points to value insn
  if (_insgen_ins(gen, I_END) < 0) {
    return insgen_destroy_impl(gen, -1);
  }

  // convert brtargets from lidx to ipos
  const int blen = gen->blen;
  for (int bidx = 0; bidx < blen; bidx++) {
    const int32_t lidx = gen->brtargets[bidx];
    // assert(lidx >= 0 && lidx < gen->llen);  // by _add_impl
    const int32_t ipos = gen->labels[lidx];
    if (ipos < 0) {
      return insgen_destroy_impl(gen, -3); // used unbound label
    }
    // assert(ipos <= gen->len);  // by _bind/_label_here + final I_END
    gen->brtargets[bidx] = ipos;
  }

  // not used/usable any more
  free(gen->labels);
  gen->llen = gen->lsize = 0;

  return gen->len;
}
// }}}


void insgen_dump(insgen_t *gen) // {{{
{
  if (!gen) {
    printf("  (null)\n");
    return;
  } else if (gen->len < 0) {
    printf("  error: %d\n", gen->len);
    return;
  }

  static const char *inames[] = {
#define X(name, code, type)   #name,
    INSNS(X)
#undef X
  };

  int iidx = 0, bidx = 0;
  for (int ipos = 0; ipos < gen->len; ipos++) {
    const uint8_t op = gen->ins[ipos];
    if (op == I_END) {
      printf("%04d: END\n", ipos);
      break;

    } else if (op >= sizeof(itype)/sizeof(*itype)) {
      printf("  bad insn: %d\n", op);
      continue; // NOTE: we do not know the number of immediates to skip!
    }

    printf("%04d: %s", ipos, inames[op]);

    const int type = itype[op];
    if (ITYPE_IS_BRANCH(type)) {
      printf(" lidx/ipos: %d", gen->brtargets[bidx]);
      bidx++;
    }

    const int extralen = ITYPE_EXTRALENGTH(type);
    for (int i = 0; i < extralen; i++) {
      printf(" (%d:) %d", iidx, gen->imms[iidx]);
      iidx++;
    }

    printf("\n");
  }
  printf("\n");
}
// }}}

