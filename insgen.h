#ifndef INSGEN_H_
#define INSGEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
  uint8_t *ins;
  int32_t len, size;  // or error (len < 0): -1: alloc error, -2: arg error, -3: used unbound label

  int32_t *imms;
  int32_t ilen, isize;

  int32_t *brtargets;  // i.e. lidx  (before _finish())  /  label ipos  (after _finish())
  int32_t blen, bsize;

  int32_t *labels;     // i.e. ipos  or -1 (unbound)
  int32_t llen, lsize;
} insgen_t;

// NOTE: init via:  insgen_t gen = {};

// returns 0 in success, <0 on error
// {{{ int insgen_add(insgen_t *gen, uint8_t op = NOP, [branch_lidx], [imm0, [imm1, [imm2]]]);
struct insgen_add_args_t {
  // struct insgen_t *gen;
  uint8_t op;
  int32_t arg0, arg1, arg2, arg3;
};
#define INSGEN_ARGC_HLP6(a0, a1, a2, a3, a4, a5, ret, ...) ret
#define insgen_add(gen, ...)  \
  insgen_add_impl(gen,                         \
    INSGEN_ARGC_HLP6(__VA_ARGS__, 5, 4, 3, 2, 1, 0), \
    (struct insgen_add_args_t){__VA_ARGS__})

int insgen_add_impl(insgen_t *gen, int argc, struct insgen_add_args_t args);
// }}}

int32_t insgen_label(insgen_t *gen);  // -1 on error
int32_t insgen_label_here(insgen_t *gen);  // -1 on error
void insgen_label_bind(insgen_t *gen, int32_t lpos);

int insgen_finish(insgen_t *gen); // final len  or -1 on error

void insgen_destroy(insgen_t *gen);

void insgen_dump(insgen_t *gen);

#ifdef __cplusplus
}
#endif

#endif
