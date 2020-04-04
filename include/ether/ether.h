#ifndef ETHER_H
#define ETHER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <iso646.h>

#define eth_likely(expr) __builtin_expect(!!(expr), 1)
#define eth_unlikely(expr) __builtin_expect((expr), 0)

#ifdef __cplusplus
# define ETH_EXTERN extern "C"
#else
# define ETH_EXTERN extern
#endif


typedef uint32_t eth_word_t;
typedef int32_t eth_sword_t;
typedef uint64_t eth_dword_t;
typedef int64_t eth_sdword_t;
typedef uint32_t eth_hash_t;

typedef enum eth_ir_tag eth_ir_tag;
typedef enum eth_insn_tag eth_insn_tag;

typedef struct eth_type eth_type;
typedef struct eth_magic eth_magic;
typedef struct eth_header eth_header;
typedef struct eth_header* eth_t;
typedef struct eth_ast eth_ast;
typedef struct eth_ir_node eth_ir_node;
typedef struct eth_ir eth_ir;
typedef struct eth_env eth_env;
typedef struct eth_module eth_module;
typedef struct eth_var eth_var;
typedef struct eth_var_list eth_var_list;
typedef struct eth_insn eth_insn;
typedef struct eth_ssa eth_ssa;
typedef struct eth_ssa_tape eth_ssa_tape;
typedef struct eth_bc_insn eth_bc_insn;
typedef struct eth_bytecode eth_bytecode;
typedef struct eth_scp eth_scp;

typedef struct eth_function eth_function;

void
eth_vfprintf(FILE *out, const char *fmt, va_list arg);

void
eth_vprintf(const char *fmt, va_list arg);

void
eth_fprintf(FILE *out, const char *fmt, ...);

void
eth_printf(const char *fmt, ...);


#define ETH_MODULE(name) static const char eth_this_module[] = name;

enum eth_log_level {
  ETH_LOG_DEBUG,
  ETH_LOG_WARNING,
  ETH_LOG_ERROR,
};

ETH_EXTERN
enum eth_log_level eth_log_level;

void
eth_log_aux(bool enable, const char *module, const char *file, const char *func,
    int line, const char *style, FILE *os, const char *fmt, ...);

#define eth_debug(fmt, ...)                          \
  eth_log_aux(                                       \
      eth_log_level <= ETH_LOG_DEBUG,                \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[7;1m", stderr, fmt, ##__VA_ARGS__)

#define eth_warning(fmt, ...)                        \
  eth_log_aux(                                       \
      eth_log_level <= ETH_LOG_WARNING,              \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[38;5;16;48;5;11;1m", stderr, fmt, ##__VA_ARGS__)

#define eth_error(fmt, ...)                          \
  eth_log_aux(                                       \
      eth_log_level <= ETH_LOG_ERROR,                \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[38;5;16;48;5;9;1m", stderr, fmt, ##__VA_ARGS__)


const char*
eth_get_prefix(void);

const char*
eth_get_version(void);

const char*
eth_get_build(void);

const char*
eth_get_build_flags(void);

const uint8_t*
eth_get_siphash_key(void);


void
eth_init(void);

void
eth_cleanup(void);

typedef enum {
  ETH_ADD,
  ETH_SUB,
  ETH_MUL,
  ETH_DIV,
  ETH_MOD,
  ETH_POW,
  // ---
  ETH_LAND,
  ETH_LOR,
  ETH_LXOR,
  ETH_LSHL,
  ETH_LSHR,
  ETH_ASHL,
  ETH_ASHR,
  // ---
  ETH_LT,
  ETH_LE,
  ETH_GT,
  ETH_GE,
  ETH_EQ,
  ETH_NE,
  // ---
  ETH_IS,
  // ---
  ETH_CONS,
} eth_binop;

const char*
eth_binop_sym(eth_binop op);

const char*
eth_binop_name(eth_binop op);

typedef enum {
  ETH_NOT,
  ETH_LNOT,
} eth_unop;

const char*
eth_unop_sym(eth_unop op);

const char*
eth_unop_name(eth_unop op);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                            CALL PROPAGATION
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_t *eth_sp;

ETH_EXTERN
int eth_nargs;

ETH_EXTERN
eth_function *eth_this;

static inline void
eth_reserve_stack(int n)
{
  eth_sp -= n;
}

static inline void
eth_pop_stack(int n)
{
  eth_sp += n;
}


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             ALLOCATORS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                          header + 1 dword
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_H1_SIZE (sizeof(eth_header) + sizeof(eth_dword_t) * 1)
void* __attribute__((malloc))
eth_alloc_h1(void);

void
eth_free_h1(void *ptr);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                          header + 2 dwords
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_H2_SIZE (sizeof(eth_header) + sizeof(eth_dword_t) * 2)
void* __attribute__((malloc))
eth_alloc_h2(void);

void
eth_free_h2(void *ptr);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                          header + 3 dwords
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_H3_SIZE (sizeof(eth_header) + sizeof(eth_dword_t) * 3)
void* __attribute__((malloc))
eth_alloc_h3(void);

void
eth_free_h3(void *ptr);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                          header + 4 dwords
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_H4_SIZE (sizeof(eth_header) + sizeof(eth_dword_t) * 4)
void* __attribute__((malloc))
eth_alloc_h4(void);

void
eth_free_h4(void *ptr);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                          header + 5 dwords
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_H5_SIZE (sizeof(eth_header) + sizeof(eth_dword_t) * 5)
void* __attribute__((malloc))
eth_alloc_h5(void);

void
eth_free_h5(void *ptr);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                          header + 6 dwords
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_H6_SIZE (sizeof(eth_header) + sizeof(eth_dword_t) * 6)

void* __attribute__((malloc))
eth_alloc_h6(void);

void
eth_free_h6(void *ptr);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                               TYPE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  char *name;
  ptrdiff_t offs;
} eth_field;

#define ETH_TFLAG_PLAIN  0x1
#define ETH_TFLAG_TUPLE  0x3
#define ETH_TFLAG_RECORD 0x5

struct eth_type {
  char *name;
  void (*destroy)(eth_type *type, eth_t x);
  void (*write)(eth_type *type, eth_t x, FILE *out);
  void (*display)(eth_type *type, eth_t x, FILE *out);

  uint8_t flag;

  void *clos;
  void (*dtor)(void *clos);

  int nfields;
  eth_field *fields;
  size_t fieldids[];
};

void
eth_default_write(eth_type *type, eth_t x, FILE *out);

eth_type* __attribute__((malloc))
eth_create_type(const char *name);

eth_type* __attribute__((malloc))
eth_create_struct_type(const char *name, char *const fields[],
    ptrdiff_t const offs[], int n);

static inline bool
eth_is_plain(eth_type *type)
{
  return type->flag & ETH_TFLAG_PLAIN;
}

static inline bool
eth_is_tuple(eth_type *type)
{
  return type->flag == ETH_TFLAG_TUPLE;
}

static inline bool
eth_is_record(eth_type *type)
{
  return type->flag == ETH_TFLAG_RECORD;
}

eth_field* __attribute__((pure))
eth_get_field(eth_type *type, const char *field);

static inline int __attribute__((pure))
eth_get_field_by_id(eth_type *restrict type, size_t id)
{
  const int n = type->nfields;
  size_t *restrict ids = type->fieldids;
  ids[n] = id;
  for (int i = 0; true; ++i)
  {
    if (ids[i] == id)
      return i;
  }
}

void
eth_destroy_type(eth_type *type);

void
eth_write(eth_t x, FILE *out);

void
eth_display(eth_t x, FILE *out);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                                 MAGIC
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
eth_magic*
eth_create_magic(void);

void
eth_ref_magic(eth_magic *magic);

void
eth_unref_magic(eth_magic *magic);

void
eth_drop_magic(eth_magic *magic);

eth_magic*
eth_get_magic(eth_word_t magicid);

eth_word_t
eth_get_magic_id(eth_magic *magic);

eth_scp* const*
eth_get_scopes(eth_magic *magic, int *n);

int
eth_add_scope(eth_magic *magic, eth_scp *scp);

int
eth_remove_scope(eth_magic *magic, eth_scp *scp);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             OBJECT HEADER
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_MAGIC_NONE (-1)

struct eth_header {
  eth_type *type;
  eth_word_t rc;
  eth_sword_t magic;
};
#define ETH(x) ((eth_t)(x))

static inline void
eth_init_header(void *ptr, eth_type *type)
{
  eth_header *hdr = ptr;
  hdr->type = type;
  hdr->rc = 0;
  hdr->magic = ETH_MAGIC_NONE;
}

static inline void
eth_force_delete(eth_t x)
{
  x->type->destroy(x->type, x);
}

static inline void
eth_delete(eth_t x)
{
  ETH_EXTERN void _eth_delete_magic(eth_t x);

  if (eth_likely(x->magic == -1))
    eth_force_delete(x);
  else
    _eth_delete_magic(x);
}

static inline void
eth_ref(eth_t x)
{
  x->rc += 1;
}

static inline eth_word_t
eth_dec(eth_t x)
{
  return x->rc -= 1;
}

static inline void
eth_unref(eth_t x)
{
  if (--x->rc == 0)
    eth_delete(x);
}

static inline void
eth_drop(eth_t x)
{
  if (x->rc == 0)
    eth_delete(x);
}

eth_magic*
eth_require_magic(eth_t x);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                            RECURSIVE SCOPE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct eth_scp {
  eth_function **clos; /* Scope closures. Note: clos ⊆ wrefs. */
  eth_t *wrefs; /* Recursive variables and closures created inside scope. */
  size_t nclos; /* Length of clos-array. */
  size_t nwref; /* Length of wrefs-array. */
  size_t rc; /* Count wrefs with non-zero RC. */
};

eth_scp*
eth_create_scp(eth_function **clos, int nclos, eth_t *wrefs, int nwref);

void
eth_destroy_scp(eth_scp *scp);

void
eth_drop_out(eth_scp *scp);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             BUILTIN TYPES
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               number
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_NUMBER_LONGDOUBLE 1
#define ETH_NUMBER_DOUBLE 2
#define ETH_NUMBER_FLOAT 3

#ifndef ETH_NUMBER_TYPE
# define ETH_NUMBER_TYPE ETH_NUMBER_LONGDOUBLE
#endif

#if ETH_NUMBER_TYPE == ETH_NUMBER_LONGDOUBLE
typedef long double eth_number_t;
# define eth_mod fmodl
# define eth_pow powl
# define eth_strtonum strtold
#elif ETH_NUMBER_TYPE == ETH_NUMBER_DOUBLE
typedef double eth_number_t;
# define eth_mod fmod
# define eth_pow pow
# define eth_strtonum strtod
#elif ETH_NUMBER_TYPE == ETH_NUMBER_FLOAT
typedef float eth_number_t;
# define eth_mod fmodf
# define eth_pow powf
# define eth_strtonum strtof
#else
# error Undefined value of ETH_NUMBER_TYPE.
#endif

ETH_EXTERN
eth_type *eth_number_type;

typedef struct {
  eth_header header;
  eth_number_t val;
} eth_number;
#define ETH_NUMBER(x) ((eth_number*)(x))

static inline eth_t __attribute__((malloc))
eth_create_number(eth_number_t val)
{
  eth_number *num = eth_alloc_h2();
  eth_init_header(num, eth_number_type);
  num->val = val;
  return ETH(num);
}
#define eth_num eth_create_number

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               function
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_function_type;

struct eth_function {
  eth_header header;
  bool islam;
  int arity;

  union {
    struct {
      eth_t (*handle)(void);
      void *data;
      void (*dtor)(void *data);
    } proc;

    struct {
      int ncap;
      eth_ir *ir;
      eth_bytecode *bc;
      eth_t *cap;
      eth_scp *scp;
    } clos;
  };
};
#define ETH_FUNCTION(x) ((eth_function*)(x))


eth_t __attribute__((malloc))
eth_create_proc(eth_t (*f)(void), int n, void *data, void (*dtor)(void*));
#define eth_proc eth_create_proc

eth_t __attribute__((malloc))
eth_create_clos(eth_ir *ir, eth_bytecode *bc, eth_t *cap, int ncap, int arity);

eth_t __attribute__((malloc))
eth_create_dummy_func(int arity);

void
eth_finalize_clos(eth_function *func, eth_ir *ir, eth_bytecode *bc, eth_t *cap,
    int ncap, int arity);

void
eth_deactivate_clos(eth_function *func);

static inline eth_t
_eth_raw_apply(eth_t fn, int narg)
{
  extern eth_t eth_vm(eth_bytecode *bc);
  eth_function *proc = ETH_FUNCTION(fn);
  eth_this = proc;
  eth_nargs = narg;
  if (proc->islam)
    return eth_vm(proc->clos.bc);
  else
    return proc->proc.handle();
}

static inline eth_t
eth_apply(eth_t fn, int narg)
{
  extern eth_t _eth_partial_apply(eth_function *fn, int narg);
  if (eth_likely(ETH_FUNCTION(fn)->arity == narg))
    return _eth_raw_apply(fn, narg);
  else
    return _eth_partial_apply(ETH_FUNCTION(fn), narg);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               exception
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_exception_type;

typedef struct {
  eth_header header;
  eth_t what;
} eth_exception;
#define ETH_EXCEPTION(x) ((eth_exception*)(x))

eth_t __attribute__((malloc))
eth_create_exception(eth_t what);
#define eth_exn eth_create_exception

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                string
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_string_type;

typedef struct {
  eth_header header;
  char *cstr;
  int len;
} eth_string;
#define ETH_STRING(x) ((eth_string*)(x))
#define eth_str_cstr(x) (ETH_STRING(x)->cstr)
#define eth_str_len(x)  (ETH_STRING(x)->len)

eth_t __attribute__((malloc))
eth_create_string(const char *cstr);
#define eth_str eth_create_string

eth_t __attribute__((malloc))
eth_create_string2(const char *str, int len);

eth_t __attribute__((malloc))
eth_create_string_from_ptr(char *cstr);

eth_t __attribute__((malloc))
eth_create_string_from_ptr2(char *cstr, int len);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               boolean
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_boolean_type;

ETH_EXTERN
eth_t eth_true;

ETH_EXTERN
eth_t eth_false;

ETH_EXTERN
eth_t eth_false_true[2];

static inline eth_t
eth_boolean(bool val)
{
  return eth_false_true[val];
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                nil
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_nil_type;

ETH_EXTERN
eth_t eth_nil;

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               pair
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_pair_type;

typedef struct {
  eth_header header;
  eth_t car;
  eth_t cdr;
} eth_pair;
#define ETH_PAIR(x) ((eth_pair*)(x))

static inline eth_t __attribute__((malloc))
eth_cons(eth_t car, eth_t cdr)
{
  eth_pair *pair = eth_alloc_h2();
  eth_init_header(pair, eth_pair_type);
  eth_ref(pair->car = car);
  eth_ref(pair->cdr = cdr);
  return ETH(pair);
}

static inline eth_t __attribute__((pure))
eth_car(eth_t x)
{
  return ETH_PAIR(x)->car;
}

static inline eth_t __attribute__((pure))
eth_cdr(eth_t x)
{
  return ETH_PAIR(x)->cdr;
}

static inline size_t __attribute__((pure))
eth_length(eth_t l, bool *isproper)
{
  size_t len = 0;
  while (l->type == eth_pair_type)
  {
    l = eth_cdr(l);
    len += 1;
  }
  if (isproper)
    *isproper = l == eth_nil;
  return len;
}

static inline bool __attribute__((pure))
eth_is_proper_list(eth_t l)
{
  while (l->type == eth_pair_type)
    l = eth_cdr(l);
  return l == eth_nil;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                              symbol
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
ETH_EXTERN
eth_type *eth_symbol_type;

eth_t
eth_create_symbol(const char *str);
#define eth_sym eth_create_symbol

static inline size_t
eth_get_symbol_id(eth_t x)
{
  return (size_t)x;
}

const char*
eth_get_symbol_cstr(eth_t x);
#define eth_sym_cstr eth_get_symbol_cstr

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                         records & tuples
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  eth_header header;
  eth_t data[];
} eth_tuple;
#define ETH_TUPLE(x) ((eth_tuple*)(x))
#define eth_tup_get(x, i) (ETH_TUPLE(x)->data[i])

eth_type*
eth_tuple_type(size_t n);

eth_type*
eth_record_type(char *const fields[], size_t n);

static inline eth_t __attribute__((malloc))
eth_create_tuple_2(eth_t _1, eth_t _2)
{
  static eth_type *type = NULL;
  if (eth_unlikely(type == NULL))
    type = eth_tuple_type(2);
  eth_tuple *tup = eth_alloc_h2();
  eth_init_header(tup, type);
  eth_ref(tup->data[0] = _1);
  eth_ref(tup->data[1] = _2);
  return ETH(tup);
}
#define eth_tup2 eth_create_tuple_2

static inline eth_t __attribute__((malloc))
eth_create_tuple_3(eth_t _1, eth_t _2, eth_t _3)
{
  static eth_type *type = NULL;
  if (eth_unlikely(type == NULL))
    type = eth_tuple_type(3);
  eth_tuple *tup = eth_alloc_h3();
  eth_init_header(tup, type);
  eth_ref(tup->data[0] = _1);
  eth_ref(tup->data[1] = _2);
  eth_ref(tup->data[2] = _3);
  return ETH(tup);
}
#define eth_tup3 eth_create_tuple_3

static inline eth_t __attribute__((malloc))
eth_create_tuple_4(eth_t _1, eth_t _2, eth_t _3, eth_t _4)
{
  static eth_type *type = NULL;
  if (eth_unlikely(type == NULL))
    type = eth_tuple_type(4);
  eth_tuple *tup = eth_alloc_h4();
  eth_init_header(tup, type);
  eth_ref(tup->data[0] = _1);
  eth_ref(tup->data[1] = _2);
  eth_ref(tup->data[2] = _3);
  eth_ref(tup->data[3] = _4);
  return ETH(tup);
}
#define eth_tup4 eth_create_tuple_4

static inline eth_t __attribute__((malloc))
eth_create_tuple_5(eth_t _1, eth_t _2, eth_t _3, eth_t _4, eth_t _5)
{
  static eth_type *type = NULL;
  if (eth_unlikely(type == NULL))
    type = eth_tuple_type(5);
  eth_tuple *tup = eth_alloc_h5();
  eth_init_header(tup, type);
  eth_ref(tup->data[0] = _1);
  eth_ref(tup->data[1] = _2);
  eth_ref(tup->data[2] = _3);
  eth_ref(tup->data[3] = _4);
  eth_ref(tup->data[4] = _5);
  return ETH(tup);
}
#define eth_tup5 eth_create_tuple_5

static inline eth_t __attribute__((malloc))
eth_create_tuple_6(eth_t _1, eth_t _2, eth_t _3, eth_t _4, eth_t _5, eth_t _6)
{
  static eth_type *type = NULL;
  if (eth_unlikely(type == NULL))
    type = eth_tuple_type(6);
  eth_tuple *tup = eth_alloc_h6();
  eth_init_header(tup, type);
  eth_ref(tup->data[0] = _1);
  eth_ref(tup->data[1] = _2);
  eth_ref(tup->data[2] = _3);
  eth_ref(tup->data[3] = _4);
  eth_ref(tup->data[4] = _5);
  eth_ref(tup->data[5] = _6);
  return ETH(tup);
}
#define eth_tup6 eth_create_tuple_6

eth_t __attribute__((malloc))
eth_create_tuple_n(eth_type *type, eth_t const data[]);
#define eth_tupn eth_create_tuple_n

static inline int __attribute__((pure))
eth_tuple_size(eth_type *type)
{
  return type->nfields;
}

static inline int __attribute__((pure))
eth_record_size(eth_type *type)
{
  return type->nfields;
}

eth_t __attribute__((malloc))
eth_create_record(eth_type *type, eth_t const data[]);

void
eth_destroy_record_h1(eth_type *type, eth_t x);

void
eth_destroy_record_h2(eth_type *type, eth_t x);

void
eth_destroy_record_h3(eth_type *type, eth_t x);

void
eth_destroy_record_h4(eth_type *type, eth_t x);

void
eth_destroy_record_h5(eth_type *type, eth_t x);

void
eth_destroy_record_h6(eth_type *type, eth_t x);

void
eth_destroy_record_hn(eth_type *type, eth_t x);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                       MODULES AND ENVIRONMENT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                              module
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  char *ident;
  eth_t val;
} eth_def;

eth_module* __attribute__((malloc))
eth_create_module(const char *name);

void
eth_destroy_module(eth_module *mod);

const char*
eth_get_module_name(const eth_module *mod);

int
eth_get_ndefs(const eth_module *mod);

eth_def*
eth_get_defs(const eth_module *mod, eth_def out[]);

eth_env*
eth_get_env(const eth_module *mod);

void
eth_define(eth_module *mod, const char *ident, eth_t val);

eth_def*
eth_find_def(const eth_module *mod, const char *ident);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                            builtins
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
const eth_module*
eth_builtins(void);

eth_t
eth_get_builtin(const char *name);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                           environment
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_MFLAG_READY (1 << 0)

eth_env* __attribute__((malloc))
eth_create_env(void);

eth_env* __attribute__((malloc))
eth_create_empty_env(void);

void
eth_destroy_env(eth_env *env);

bool
eth_add_module_path(eth_env *env, const char *path);

bool
eth_resolve_path(eth_env *env, const char *path, char *fullpath);

bool
eth_add_module(eth_env *env, eth_module *mod);

eth_module*
eth_require_module(eth_env *env, const char *name);

bool
eth_load_module_from_script(eth_env *env, eth_module *mod, const char *path);

bool
eth_load_module_from_elf(eth_env *env, eth_module *mod, const char *path);

int
eth_get_nmodules(const eth_env *env);

void
eth_get_modules(const eth_env *env, const eth_module *out[], int n);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                              LOCATIONS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  int rc;
  int fl, fc, ll, lc;
  char *filepath;
} eth_location;

eth_location*
eth_create_location(int fl, int fc, int ll, int lc, const char *path);

void
eth_ref_location(eth_location *loc);

void
eth_unref_location(eth_location *loc);

void
eth_drop_location(eth_location *loc);

int
eth_print_location(eth_location *loc, FILE *stream);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                         ABSTRACT SYNTAX TREE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef enum {
  ETH_PATTERN_IDENT,
  ETH_PATTERN_UNPACK,
  ETH_PATTERN_SYMBOL,
  ETH_PATTERN_RECORD,
} eth_pattern_tag;

typedef struct eth_ast_pattern eth_ast_pattern;
struct eth_ast_pattern {
  eth_pattern_tag tag;
  union {
    struct { char *str; bool pub; } ident;
    struct { bool isctype; union { char *str; eth_type *ctype; } type; char **fields;
             eth_ast_pattern **subpats; int n; }
      unpack;
    struct { eth_t sym; } symbol;
    struct { char **fields; eth_ast_pattern **subpats; int n; } record;
  };
};

eth_ast_pattern*
eth_ast_ident_pattern(const char *ident);

eth_ast_pattern*
eth_ast_unpack_pattern(const char *type, char *const fields[],
    eth_ast_pattern *pats[], int n);

eth_ast_pattern*
eth_ast_unpack_pattern_with_type(eth_type *type, char *const fields[],
    eth_ast_pattern *pats[], int n);

eth_ast_pattern*
eth_ast_symbol_pattern(eth_t sym);

eth_ast_pattern*
eth_ast_record_pattern(char *const fields[], eth_ast_pattern *pats[], int n);

void
eth_destroy_ast_pattern(eth_ast_pattern *pat);

typedef enum {
  ETH_AST_CVAL,
  ETH_AST_IDENT,
  ETH_AST_APPLY,
  ETH_AST_IF,
  ETH_AST_SEQ,
  ETH_AST_LET,
  ETH_AST_LETREC,
  ETH_AST_BINOP,
  ETH_AST_UNOP,
  ETH_AST_FN,
  ETH_AST_MATCH,
  ETH_AST_IMPORT,
  ETH_AST_AND,
  ETH_AST_OR,
  ETH_AST_TRY,
  ETH_AST_MKRCRD,
} eth_ast_tag;

typedef enum {
  ETH_TOPLVL_NONE,
  ETH_TOPLVL_THEN,
  ETH_TOPLVL_ELSE,
} eth_toplvl_flag;

struct eth_ast {
  eth_ast_tag tag;
  eth_word_t rc;
  union {
    struct { eth_t val; } cval;
    struct { char *str; } ident;
    struct { eth_ast *fn, **args; int nargs; } apply;
    struct { eth_ast *cond, *then, *els; eth_toplvl_flag toplvl; } iff;
    struct { eth_ast *e1, *e2; } seq;
    struct { eth_ast_pattern **pats; eth_ast **vals; eth_ast *body; int n; }
      let, letrec;
    struct { eth_binop op; eth_ast *lhs, *rhs; } binop;
    struct { eth_unop op; eth_ast *expr; } unop;
    struct { eth_ast_pattern **args; int arity; eth_ast *body; } fn;
    struct { eth_ast_pattern *pat; eth_ast *expr, *thenbr, *elsebr;
             eth_toplvl_flag toplvl; }
      match;
    struct { char *module; eth_ast *body; char *alias; char **nams; int nnam; }
      import;
    struct { eth_ast *lhs, *rhs; } scand, scor;
    struct { eth_ast_pattern *pat; eth_ast *trybr, *catchbr; int likely; } try;
    struct { bool isctype; union { eth_type *ctype; char *str; } type;
             char **fields; eth_ast **vals; int n; }
      mkrcrd;
  };
  eth_location *loc;
};

void
eth_ref_ast(eth_ast *ast);

void
eth_unref_ast(eth_ast *ast);

void
eth_drop_ast(eth_ast *ast);

void
eth_set_ast_location(eth_ast *ast, eth_location *loc);

eth_ast* __attribute__((malloc))
eth_ast_cval(eth_t val);

eth_ast* __attribute__((malloc))
eth_ast_ident(const char *ident);

eth_ast* __attribute__((malloc))
eth_ast_apply(eth_ast *fn, eth_ast *const *args, int nargs);

void
eth_ast_append_arg(eth_ast *apply, eth_ast *arg);

eth_ast* __attribute__((malloc))
eth_ast_if(eth_ast *cond, eth_ast *then, eth_ast *els);

eth_ast* __attribute__((malloc))
eth_ast_seq(eth_ast *e1, eth_ast *e2);

eth_ast* __attribute__((malloc))
eth_ast_let(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body);

eth_ast* __attribute__((malloc))
eth_ast_letrec(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body);

eth_ast* __attribute__((malloc))
eth_ast_binop(eth_binop op, eth_ast *lhs, eth_ast *rhs);

eth_ast* __attribute__((malloc))
eth_ast_unop(eth_unop op, eth_ast *expr);

eth_ast* __attribute__((malloc))
eth_ast_fn(char **args, int arity, eth_ast *body);

eth_ast*
eth_ast_fn_with_patterns(eth_ast_pattern *const args[], int arity, eth_ast *body);

eth_ast* __attribute__((malloc))
eth_ast_match(eth_ast_pattern *pat, eth_ast *expr, eth_ast *thenbr,
    eth_ast *elsebr);

eth_ast* __attribute__((malloc))
eth_ast_import(const char *module, const char *alias, char *const nams[],
    int nnam, eth_ast *body);

eth_ast* __attribute__((malloc))
eth_ast_and(eth_ast *lhs, eth_ast *rhs);

eth_ast* __attribute__((malloc))
eth_ast_or(eth_ast *lhs, eth_ast *rhs);

eth_ast* __attribute__((malloc))
eth_ast_try(eth_ast_pattern *pat, eth_ast *try, eth_ast *catch, int likely);

eth_ast*
eth_ast_make_record(const char *type, char *const fields[],
    eth_ast *const vals[], int n);

eth_ast*
eth_ast_make_record_with_type(eth_type *type, char *const fields[],
   eth_ast *const vals[], int n);

typedef struct eth_scanner eth_scanner;

eth_scanner* __attribute__((malloc))
eth_create_scanner(FILE *stream);

void
eth_destroy_scanner(eth_scanner *scan);

FILE*
eth_get_scanner_input(eth_scanner *scan);

eth_ast* __attribute__((malloc))
eth_parse(FILE *stream);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                  INTERMEDIATE SYNTAX TREE REPRESENTATION
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct eth_ir_pattern eth_ir_pattern;
struct eth_ir_pattern {
  eth_pattern_tag tag;
  union {
    struct { int varid; } ident;
    struct { eth_type *type; int n, *offs; eth_ir_pattern **subpats; } unpack;
    struct { eth_t sym; } symbol;
    struct { size_t *ids; int n; eth_ir_pattern **subpats; } record;
  };
};

eth_ir_pattern*
eth_ir_ident_pattern(int varid);

eth_ir_pattern*
eth_ir_unpack_pattern(eth_type *type, int offs[], eth_ir_pattern *pats[], int n);

eth_ir_pattern*
eth_ir_symbol_pattern(eth_t sym);

eth_ir_pattern*
eth_ir_record_pattern(size_t const ids[], eth_ir_pattern *const pats[], int n);

void
eth_destroy_ir_pattern(eth_ir_pattern *pat);

enum eth_ir_tag {
  ETH_IR_ERROR,
  ETH_IR_CVAL,
  ETH_IR_VAR,
  ETH_IR_APPLY,
  ETH_IR_IF,
  ETH_IR_TRY,
  ETH_IR_SEQ,
  ETH_IR_BINOP,
  ETH_IR_UNOP,
  ETH_IR_FN,
  ETH_IR_MATCH,
  ETH_IR_STARTFIX,
  ETH_IR_ENDFIX,
  ETH_IR_MKRCRD,
  ETH_IR_THROW,
};

struct eth_ir_node {
  eth_ir_tag tag;
  int rc;
  union {
    struct { eth_t val; } cval;
    struct { int vid; } var;
    struct { eth_ir_node *fn, **args; int nargs; } apply;
    struct { eth_ir_node *cond, *thenbr, *elsebr; eth_toplvl_flag toplvl; } iff;
    struct { int exnvar; eth_ir_node *trybr, *catchbr; int likely; } try;
    struct { eth_ir_node *e1, *e2; } seq;
    struct { eth_binop op; eth_ir_node *lhs, *rhs; } binop;
    struct { eth_unop op; eth_ir_node *expr; } unop;
    struct { int arity, *caps, *capvars, ncap; eth_ir *body; } fn;
    struct { eth_ir_pattern *pat; eth_ir_node *expr, *thenbr, *elsebr;
             eth_toplvl_flag toplvl; }
      match;
    struct { int *vars, n; eth_ir_node *body; } startfix;
    struct { int *vars, n; eth_ir_node *body; } endfix;
    struct { eth_type *type; eth_ir_node **fields; } mkrcrd;
    struct { eth_ir_node *exn; } throw;
  };
  eth_location *loc;
};

void
eth_ref_ir_node(eth_ir_node *node);

void
eth_unref_ir_node(eth_ir_node *node);

void
eth_drop_ir_node(eth_ir_node *node);

void
eth_set_ir_location(eth_ir_node *node, eth_location *loc);

eth_ir_node* __attribute__((malloc))
eth_ir_error(void);

eth_ir_node* __attribute__((malloc))
eth_ir_cval(eth_t val);

eth_ir_node* __attribute__((malloc))
eth_ir_var(int vid);

eth_ir_node* __attribute__((malloc))
eth_ir_apply(eth_ir_node *fn, eth_ir_node *const *args, int nargs);

eth_ir_node* __attribute__((malloc))
eth_ir_if(eth_ir_node *cond, eth_ir_node *thenbr, eth_ir_node *elsebr);

eth_ir_node* __attribute__((malloc))
eth_ir_try(int exnvar, eth_ir_node *trybr, eth_ir_node *catchbr, int likely);

eth_ir_node* __attribute__((malloc))
eth_ir_seq(eth_ir_node *e1, eth_ir_node *e2);

eth_ir_node* __attribute__((malloc))
eth_ir_binop(eth_binop op, eth_ir_node *lhs, eth_ir_node *rhs);

eth_ir_node* __attribute__((malloc))
eth_ir_unop(eth_unop op, eth_ir_node *expr);

eth_ir_node* __attribute__((malloc))
eth_ir_fn(int arity, int *caps, int *capvars, int ncap, eth_ir *body);

eth_ir_node* __attribute__((malloc))
eth_ir_match(eth_ir_pattern *pat, eth_ir_node *expr, eth_ir_node *thenbr,
    eth_ir_node *elsebr);

eth_ir_node* __attribute__((malloc))
eth_ir_startfix(int const vars[], int n, eth_ir_node *body);

eth_ir_node* __attribute__((malloc))
eth_ir_endfix(int const vars[], int n, eth_ir_node *body);

eth_ir_node*
eth_ir_bind(int const varids[], eth_ir_node *const vals[], int n,
    eth_ir_node *body);

eth_ir_node*
eth_ir_mkrcrd(eth_type *type, eth_ir_node *const fields[]);

eth_ir_node*
eth_ir_throw(eth_ir_node *exn);

struct eth_ir {
  size_t rc;
  eth_ir_node *body;
  int nvars;
};

eth_ir* __attribute__((malloc))
eth_create_ir(eth_ir_node *body, int nvars);

void
eth_ref_ir(eth_ir *ir);

void
eth_drop_ir(eth_ir *ir);

void
eth_unref_ir(eth_ir *ir);

typedef struct eth_var_cfg eth_var_cfg;
struct eth_var_cfg {
  char *ident;
  eth_t cval;
  int vid;
};

static inline eth_var_cfg
eth_dyn_var(char *ident, int vid)
{
  return (eth_var_cfg) { .ident = ident, .cval = NULL, .vid = vid };
}

static inline eth_var_cfg
eth_const_var(char *ident, eth_t cval)
{
  return (eth_var_cfg) { .ident = ident, .cval = cval, .vid = -1 };
}

struct eth_var {
  char *ident;
  eth_t cval;
  int vid;
  eth_var *next;
};

static inline eth_var_cfg
eth_copy_var_cfg(eth_var* var, int vid)
{
  return (eth_var_cfg) {
    .ident = var->ident,
    .cval = var->cval,
    .vid = vid,
  };
}

struct eth_var_list {
  int len;
  eth_var *head;
};

/*
 * Create new list for variables.
 */
eth_var_list* __attribute__((malloc))
eth_create_var_list(void);

/*
 * Destroy list with variables.
 */
void
eth_destroy_var_list(eth_var_list *lst);

/*
 * Add variable on top of the list.
 */
eth_var* __attribute__((malloc))
eth_prepend_var(eth_var_list *lst, eth_var_cfg cfg);

/*
 * Add variable at the end of the list.
 */
eth_var* __attribute__((malloc))
eth_append_var(eth_var_list *lst, eth_var_cfg cfg);

/*
 * Pop variable from the top of the list.
 */
void
eth_pop_var(eth_var_list *lst, int n);

/*
 * Find a variable with mathing identifier string. Optionaly returnes offset of
 * the variable counting from the top of the list.
 */
eth_var*
eth_find_var(eth_var *head, const char *ident, int *cnt);

typedef struct {
  char **idents;
  int *varids;
  int n;
} eth_ir_defs;

void
eth_destroy_ir_defs(eth_ir_defs *defs);

eth_ir*
eth_build_ir(eth_ast *ast, eth_env *env);

eth_ir*
eth_build_module_ir(eth_ast *ast, eth_env *env, eth_module *mod,
    eth_ir_defs *defs);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                STATIC SINGLE ASSIGNEMENT REPRESENTATION
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*
 * Note: unpack.type is NULL if no type check required.
 */
typedef struct eth_ssa_pattern eth_ssa_pattern;
struct eth_ssa_pattern {
  eth_pattern_tag tag;
  union {
    struct { int vid; } ident;
    struct { eth_type *type; int *offs, *vids, n; eth_ssa_pattern **subpat;
             bool dotest; }
      unpack;
    struct { eth_t sym; bool dotest; } symbol;
    struct { size_t *ids; int *vids, n; eth_ssa_pattern **subpat; bool dotest; }
      record;
  };
};

eth_ssa_pattern* __attribute__((malloc))
eth_ssa_ident_pattern(int vid);

eth_ssa_pattern* __attribute__((malloc))
eth_ssa_unpack_pattern(eth_type *type, int const offs[], int const vids[],
    eth_ssa_pattern *const pats[], int n, bool dotest);

eth_ssa_pattern*
eth_ssa_symbol_pattern(eth_t sym, bool dotest);

eth_ssa_pattern*
eth_ssa_record_pattern(size_t const ids[], int const vids[],
    eth_ssa_pattern *const pats[], int n);

void
eth_destroy_ssa_pattern(eth_ssa_pattern *pat);

enum eth_insn_tag {
  ETH_INSN_NOP,
  ETH_INSN_CVAL,
  ETH_INSN_APPLY,
  ETH_INSN_APPLYTC,
  ETH_INSN_IF,
  ETH_INSN_MOV,
  ETH_INSN_REF,
  ETH_INSN_DEC,
  ETH_INSN_UNREF,
  ETH_INSN_DROP,
  ETH_INSN_RET,
  ETH_INSN_BINOP,
  ETH_INSN_UNOP,
  ETH_INSN_FN,
  ETH_INSN_ALCFN,
  ETH_INSN_FINFN,
  ETH_INSN_POP,
  ETH_INSN_CAP,
  ETH_INSN_MKSCP,
  ETH_INSN_CATCH,
  ETH_INSN_TRY,
  ETH_INSN_GETEXN,
  ETH_INSN_MKRCRD,
};

enum {
  /* Disable writes BEFORE given instruction. */
  ETH_IFLAG_NOBEFORE = (1 << 0),
};

typedef enum {
  ETH_TEST_NOTFALSE,
  ETH_TEST_TYPE,
  ETH_TEST_MATCH,
} eth_test_tag;

struct eth_insn {
  eth_insn_tag tag;
  int out;
  int flag;
  union {
    struct { eth_t val; } cval;
    struct { int vid; } var;
    struct { int fn, *args; int nargs; } apply;
    struct { int cond, likely; eth_insn *thenbr, *elsebr; eth_test_tag test;
             union { eth_type *type; eth_ssa_pattern *pat; };
             eth_toplvl_flag toplvl; }
      iff;
    struct { eth_binop op; int lhs, rhs; } binop;
    struct { eth_unop op; int vid; } unop;
    struct { int arity, *caps, ncap; eth_ir *ir; eth_ssa *ssa; } fn;
    struct { int arity, *caps, ncap, *movs, nmov; eth_ir *ir; eth_ssa *ssa; }
      finfn;
    struct { int arity; } alcfn;
    struct { int n; } pop;
    struct { int n; } cap;
    struct { int *clos, nclos, *wrefs, nwref; } mkscp;
    struct { int tryid, vid; } catch;
    struct { int id, likely; eth_insn *trybr, *catchbr; } try;
    struct { } getexn;
    struct { int *vids; eth_type *type; } mkrcrd;
  };
  eth_insn *prev;
  eth_insn *next;
};

void
eth_destroy_insn(eth_insn *insn);

void
eth_destroy_insn_list(eth_insn *head);

eth_insn* __attribute__((malloc))
eth_insn_nop(void);

eth_insn* __attribute__((malloc))
eth_insn_cval(int out, eth_t val);

eth_insn* __attribute__((malloc))
eth_insn_apply(int out, int fn, const int *args, int nargs);

eth_insn* __attribute__((malloc))
eth_insn_applytc(int out, int fn, const int *args, int nargs);

eth_insn* __attribute__((malloc))
eth_insn_if(int out, int cond, eth_insn *thenbr, eth_insn *elsebr);

eth_insn* __attribute__((malloc))
eth_insn_if_test_type(int out, int cond, eth_type *type, eth_insn *thenbr,
    eth_insn *elsebr);

eth_insn* __attribute__((malloc))
eth_insn_if_match(int out, int cond, eth_ssa_pattern *pat, eth_insn *thenbr,
    eth_insn *elsebr);

eth_insn* __attribute__((malloc))
eth_insn_mov(int out, int vid);

eth_insn* __attribute__((malloc))
eth_insn_ref(int vid);

eth_insn* __attribute__((malloc))
eth_insn_dec(int vid);

eth_insn* __attribute__((malloc))
eth_insn_unref(int vid);

eth_insn* __attribute__((malloc))
eth_insn_drop(int vid);

eth_insn* __attribute__((malloc))
eth_insn_ret(int vid);

eth_insn* __attribute__((malloc))
eth_insn_binop(eth_binop op, int out, int lhs, int rhs);

eth_insn* __attribute__((malloc))
eth_insn_unop(eth_unop op, int out, int vid);

eth_insn* __attribute__((malloc))
eth_insn_fn(int out, int arity, int *caps, int ncap, eth_ir *ir, eth_ssa *ssa);

eth_insn* __attribute__((malloc))
eth_insn_alcfn(int out, int arity);

eth_insn* __attribute__((malloc))
eth_insn_finfn(int c, int arity, int *caps, int ncap, int *movs, int nmov,
    eth_ir *ir, eth_ssa *ssa);

eth_insn* __attribute__((malloc))
eth_insn_pop(int vid0, int n);

eth_insn* __attribute__((malloc))
eth_insn_cap(int vid0, int n);

eth_insn* __attribute__((malloc))
eth_insn_mkscp(int *clos, int nclos, int *wrefs, int nwref);

eth_insn* __attribute__((malloc))
eth_insn_catch(int tryid, int vid);

eth_insn* __attribute__((malloc))
eth_insn_try(int out, int id, eth_insn *trybr, eth_insn *catchbr);

eth_insn* __attribute__((malloc))
eth_insn_getexn(int out);

eth_insn* __attribute__((malloc))
eth_insn_mkrcrd(int out, int const vids[], eth_type *type);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                             SSA tape
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct eth_ssa_tape {
  eth_insn *head;
  eth_insn *point;
};

eth_ssa_tape* __attribute__((malloc))
eth_create_ssa_tape(void);

eth_ssa_tape* __attribute__((malloc))
eth_create_ssa_tape_at(eth_insn *at);

void
eth_destroy_ssa_tape(eth_ssa_tape *tape);

void
eth_write_insn(eth_ssa_tape *tape, eth_insn *insn);

void
eth_insert_insn_before(eth_insn *where, eth_insn *insn);

void
eth_insert_insn_after(eth_insn *where, eth_insn *insn);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                             SSA chunk
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct eth_ssa {
  int rc;
  int nvals;
  int ntries;
  eth_insn *body;
};

eth_ssa* __attribute__((malloc))
eth_build_ssa(eth_ir *ir, eth_ir_defs *defs);

void
eth_destroy_ssa(eth_ssa *ssa);

void
eth_dump_ssa(const eth_insn *insn, FILE *stream);

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                               BYTECODE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef enum {
  ETH_OPC_CVAL,

  ETH_OPC_PUSH,
  ETH_OPC_POP,

  ETH_OPC_APPLY,
  ETH_OPC_APPLYTC,

  ETH_OPC_TEST,
  ETH_OPC_TESTTY,
  ETH_OPC_TESTIS,
  ETH_OPC_GETTEST,

  ETH_OPC_DUP,

  ETH_OPC_JNZ,
  ETH_OPC_JZE,
  ETH_OPC_JMP,

  ETH_OPC_REF,
  ETH_OPC_DEC,
  ETH_OPC_UNREF,
  ETH_OPC_DROP,

  ETH_OPC_RET,

  ETH_OPC_ADD,
  ETH_OPC_SUB,
  ETH_OPC_MUL,
  ETH_OPC_DIV,
  ETH_OPC_MOD,
  ETH_OPC_POW,
  // ---
  ETH_OPC_LAND,
  ETH_OPC_LOR,
  ETH_OPC_LXOR,
  ETH_OPC_LSHL,
  ETH_OPC_LSHR,
  ETH_OPC_ASHL,
  ETH_OPC_ASHR,
  // ---
  ETH_OPC_LT,
  ETH_OPC_LE,
  ETH_OPC_GT,
  ETH_OPC_GE,
  ETH_OPC_EQ,
  ETH_OPC_NE,
  // ---
  ETH_OPC_IS,
  // ---
  ETH_OPC_CONS,

  ETH_OPC_NOT,
  ETH_OPC_LNOT,

  ETH_OPC_FN,
  ETH_OPC_ALCFN,
  ETH_OPC_FINFN,
  ETH_OPC_CAP,
  ETH_OPC_MKSCP,

  ETH_OPC_LOAD,
  ETH_OPC_LOADRCRD,
  ETH_OPC_LOADRCRD1,

  ETH_OPC_SETEXN,
  ETH_OPC_GETEXN,

  ETH_OPC_MKRCRD,
} eth_opc;

struct eth_bc_insn {
#ifdef ETH_DEBUG_MODE
  eth_opc opc;
#else
  uint64_t opc;
#endif
  union {
    struct { uint64_t out; eth_t val; } cval;

    struct { uint64_t *vids, n; } push;
    struct { uint64_t vid0, n; } pop;

    struct { uint64_t out, fn; } apply;

    struct { uint64_t vid; } test;
    struct { uint64_t vid; eth_type *type; } testty;
    struct { uint64_t vid; eth_t cval; } testis;
    struct { uint64_t out; } gettest;

    struct { uint64_t out, vid; } dup;

    struct { ptrdiff_t offs; } jnz, jze, jmp;

    struct { uint64_t vid; } ref;
    struct { uint64_t vid; } dec;
    struct { uint64_t vid; } unref;
    struct { uint64_t vid; } drop;

    struct { uint64_t vid; } ret;

    struct { uint64_t out; uint32_t lhs, rhs; } binop;
    struct { uint64_t out, vid; } unop;

    struct {
      uint64_t out;
      struct {
        int arity;
        eth_ir *ir;
        eth_bytecode *bc;
        int ncap;
        int caps[];
      } *restrict data;
    } fn, finfn;
    struct { uint64_t out; int arity; } alcfn;
    struct { uint64_t vid0, n; } cap;
    struct { struct { uint64_t *clos, nclos, *wrefs, nwref; } *restrict data; }
      mkscp;

    struct { uint64_t out; uint32_t vid, offs; } load;
    struct { uint32_t src, n; uint64_t *vids; size_t *ids; } loadrcrd;
    struct { uint64_t out, vid; size_t id; } loadrcrd1;

    struct { uint64_t vid; } setexn;
    struct { uint64_t out; } getexn;

    struct { uint64_t out;
             struct { eth_type *type; uint64_t vids[]; } *restrict data; }
      mkrcrd;
  };
} __attribute__((aligned(32)));

struct eth_bytecode {
  int rc;
  int nreg;
  int len;
  eth_bc_insn *code;
};

eth_bytecode* __attribute__((malloc))
eth_build_bytecode(eth_ssa *ssa);

void
eth_ref_bytecode(eth_bytecode *bc);

void
eth_unref_bytecode(eth_bytecode *bc);

void
eth_drop_bytecode(eth_bytecode *bc);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                            VIRTUAL MACHINE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
eth_t
eth_vm(eth_bytecode *bc);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                              API HELPER
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  const size_t n;
  size_t cnt;
} eth_args;

static inline eth_t
_eth_type_error(size_t n, size_t ntot)
{
  for (size_t i = 0; i < n; ++i)
    eth_unref(eth_sp[i]);
  eth_pop_stack(ntot);
  return eth_exn(eth_sym("Type_error"));
}

#define eth_start(arity) \
  { .n = arity, .cnt = 0 }

#define eth_arg(args)                          \
  ({                                           \
    const eth_t _eth_arg = eth_sp[args.cnt++]; \
    eth_ref(_eth_arg);                         \
    _eth_arg;                                  \
  })

#define eth_arg2(args, ty)                          \
  ({                                                \
    const eth_t _eth_arg = eth_sp[args.cnt++];      \
    if (_eth_arg->type != ty)                       \
      return _eth_type_error(args.cnt - 1, args.n); \
    eth_ref(_eth_arg);                              \
    _eth_arg;                                       \
  })

#define eth_end(args) \
  eth_pop_stack(args.n);

#define eth_end_dec(args)               \
  do {                                  \
    for (size_t i = 0; i < args.n; ++i) \
      eth_dec(*eth_sp++);               \
  } while (0)

#define eth_end_unref(args)             \
  do {                                  \
    for (size_t i = 0; i < args.n; ++i) \
      eth_unref(*eth_sp++);             \
  } while (0)

#define eth_return(args, ret)           \
  do {                                  \
    eth_ref(ret);                       \
    for (size_t i = 0; i < args.n; ++i) \
      eth_unref(*eth_sp++);             \
    eth_dec(ret);                       \
    return ret;                         \
  } while (0)

#endif
