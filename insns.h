#ifndef INSNS_H_
#define INSNS_H_

#define ITYPE_BR   0x04

#define ITYPE_EXTRALENGTH(v)   ((v) & 0x03)
#define ITYPE_IS_BRANCH(v)     (((v) & ITYPE_BR) != 0)

#define INSNS(X) \
  X(NOP, { }, 0)                       \
  X(RET, { return r1; }, 0)            \
  X(ADD, { r1 += r2; }, 0)             \
  X(ADDI, { r1 += *++ip; }, 1)         \
  X(JMP, { ip += ip[1]; }, ITYPE_BR)   \

enum {
#define X(name, code, type)   I_ ## name,
  INSNS(X)
#undef X
  I_END = 0xff
};

#endif
