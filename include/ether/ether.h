/* Copyright (C) 2020  Ivan Pidhurskyi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef ETHER_H
#define ETHER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <iso646.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>

#define eth_likely(expr) __builtin_expect(!!(expr), 1)
#define eth_unlikely(expr) __builtin_expect((expr), 0)

#ifdef __cplusplus

/* C++ does not have a 'restrict'-keyword by standard, also most compilers do
 * implement it. */
# ifdef __GNUC__
#   define restrict __restrict__
# else
#   warning "No support for `restrict'-qualifier"
# endif

/* Disable mangling during linkage. */
extern "C" {

/* I used 'try', 'catch' and 'throw' keywords in the header (shame on me), so
 * here we temporary hide these. */
# define try   _try
# define catch _catch
# define throw _throw

#endif


typedef int16_t eth_sshort_t;
typedef uint16_t eth_short_t;
typedef uint32_t eth_word_t;
typedef int32_t eth_sword_t;
typedef uint64_t eth_dword_t;
typedef int64_t eth_sdword_t;
typedef uint32_t eth_hash_t;

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

typedef struct eth_type eth_type; /**< @ingroup Type */
typedef struct eth_header eth_header;
typedef struct eth_header* eth_t;
typedef struct eth_ast eth_ast;
typedef struct eth_ir_node eth_ir_node;
typedef struct eth_ir eth_ir;
typedef struct eth_env eth_env;
typedef struct eth_root eth_root;
typedef struct eth_module eth_module;
typedef struct eth_var eth_var;
typedef struct eth_var_list eth_var_list;
typedef struct eth_insn eth_insn;
typedef struct eth_ssa eth_ssa;
typedef struct eth_ssa_tape eth_ssa_tape;
typedef struct eth_bc_insn eth_bc_insn;
typedef struct eth_bytecode eth_bytecode;

typedef struct eth_function eth_function;
typedef struct eth_mut_ref eth_mut_ref;

typedef struct eth_location eth_location;

void
eth_vfprintf(FILE *out, const char *fmt, va_list arg);

void
eth_vprintf(const char *fmt, va_list arg);

void
eth_fprintf(FILE *out, const char *fmt, ...);

void
eth_printf(const char *fmt, ...);

bool
eth_format(FILE *out, const char *fmt, eth_t arr[], int arrlen);

int
eth_study_format(const char *fmt);


#define ETH_MODULE(name) static const char eth_this_module[] = name;

enum eth_log_level {
  ETH_LOG_DEBUG,
  ETH_LOG_WARNING,
  ETH_LOG_ERROR,
};

extern
enum eth_log_level eth_log_level;

void
eth_log_aux(bool enable, const char *module, const char *file, const char *func,
    int line, const char *style, FILE *os, const char *fmt, ...);

void
eth_indent_log(void);

void
eth_dedent_log(void);

#define eth_debug_enabled (eth_log_level <= ETH_LOG_DEBUG)
#define eth_warnings_enabled (eth_log_level <= ETH_LOG_WARNING)
#define eth_errors_enabled (eth_log_level <= ETH_LOG_ERROR)

#define eth_debug(fmt, ...)                          \
  eth_log_aux(                                       \
      eth_log_level <= ETH_LOG_DEBUG,                \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[7;1m", stderr, fmt, ##__VA_ARGS__, true)

#define eth_warning(fmt, ...)                        \
  eth_log_aux(                                       \
      eth_log_level <= ETH_LOG_WARNING,              \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[38;5;16;48;5;11;1m", stderr, fmt, ##__VA_ARGS__, true)

#define eth_error(fmt, ...)                          \
  eth_log_aux(                                       \
      eth_log_level <= ETH_LOG_ERROR,                \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[38;5;16;48;5;9;1m", stderr, fmt, ##__VA_ARGS__, true)

#define eth_trace(fmt, ...)                          \
  eth_log_aux(                                       \
      true,                                          \
      eth_this_module, __FILE__, __func__, __LINE__, \
      "\e[48;5;16;38;5;9;1m", stderr, fmt, ##__VA_ARGS__, true)

#define eth_unimplemented()                             \
  do {                                                  \
    eth_log_aux(                                        \
      true,                                             \
      eth_this_module, __FILE__, __func__, __LINE__,    \
      "\e[38;5;16;48;5;9;1m", stderr,                   \
      "unimplemented: %s, `%s()`", __FILE__, __func__); \
    abort();                                            \
  } while (0)


const char*
eth_get_prefix(void);

const char*
eth_get_module_path(void);

const char*
eth_get_version(void);

const char*
eth_get_build(void);

const char*
eth_get_build_flags(void);

const uint8_t*
eth_get_siphash_key(void);


const char*
eth_errno_to_str(int e);


void
eth_init(const int *argc);

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
  ETH_EQUAL,
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
extern
eth_t *eth_stack;

extern
eth_t *eth_sp;

extern
int eth_nargs;

extern
eth_function *eth_this;

extern
ssize_t eth_c_stack_size;

extern
char *eth_c_stack_start;

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

static inline bool
eth_reserve_c_stack(ssize_t size)
{
  char x;
  ssize_t avsize = eth_c_stack_size - (eth_c_stack_start - &x);
  return avsize > size;
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
//                              METHODS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef eth_t (*eth_method_cb)(eth_t self);
typedef struct eth_methods eth_methods;

eth_methods*
eth_create_methods();

void
eth_destroy_methods(eth_methods *ms);

bool
eth_add_method(eth_methods *ms, eth_t sym, eth_method_cb cb);

eth_method_cb
eth_get_method(eth_methods *ms, eth_t sym);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                               TYPE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup Type Type
 * @brief Internal representation for types.
 * @{ */

/** @ingroup Type */
struct eth_field {
  char *name; /**< @brief Field name */
  ptrdiff_t offs; /**< @brief Field offset. @todo ...offset from what? */
};
typedef struct eth_field eth_field;

#define ETH_TFLAG_PLAIN   0x01
#define ETH_TFLAG_TUPLE   (ETH_TFLAG_PLAIN | 1 << 1)
#define ETH_TFLAG_RECORD  (ETH_TFLAG_PLAIN | 1 << 2)
#define ETH_TFLAG_VARIANT (ETH_TFLAG_PLAIN | 1 << 3)
/*#define ETH_TFLAG_OBJECT  (ETH_TFLAG_RECORD | 1 << 4)*/

struct eth_type {
  char *name;
  void (*destroy)(eth_type *type, eth_t x);

  void (*write)(eth_type *type, eth_t x, FILE *out);
  void (*display)(eth_type *type, eth_t x, FILE *out);
  bool (*equal)(eth_type *type, eth_t x, eth_t y);
  eth_methods *methods;

  uint8_t flag;

  void *clos;
  void (*dtor)(void *clos);

  int nfields;
  eth_field *fields;
  size_t fieldids[];
};

void
eth_default_write(eth_type *type, eth_t x, FILE *out);

eth_t
eth_cast_id(eth_type *type, eth_t x);

eth_type* __attribute__((malloc))
eth_create_type(const char *name);

eth_type* __attribute__((malloc))
eth_create_struct_type(const char *name, char *const *fields,
    ptrdiff_t const *offs, int n);

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

static inline bool
eth_is_variant(eth_type *type)
{
  return type->flag == ETH_TFLAG_VARIANT;
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

size_t
eth_get_field_id_by_offs(const eth_type *type, ptrdiff_t offs);

void
eth_destroy_type(eth_type *type);

/** @} Type */

void
eth_write(eth_t x, FILE *out);

void
eth_display(eth_t x, FILE *out);

bool
eth_equal(eth_t x, eth_t y);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             OBJECT HEADER
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define ETH_RC_MAX UINT32_MAX
struct eth_header {
  eth_type *type;
#if defined(ETH_OBJECT_FLAGS)
  uint8_t flags:4;
  eth_dword_t rc:60;
#else
  eth_dword_t rc;
#endif
};
#define ETH(x) ((eth_t)(x))

static inline void
eth_init_header(void *ptr, eth_type *type)
{
  eth_header *hdr = (eth_header*)ptr;
  hdr->type = type;
  hdr->rc = 0;
}

static inline void
eth_force_delete(eth_t x)
{
  x->type->destroy(x->type, x);
}

static inline void
eth_delete(eth_t x)
{
  eth_force_delete(x);
}

static inline void __attribute__((hot))
eth_ref(eth_t x)
{
#if defined(ETH_DEBUG_MODE)
  if (x->rc >= ETH_RC_MAX)
    fprintf(stderr, "FATAL ERROR: overflow of reference count\n");
#endif
  x->rc += 1;
}

static inline eth_word_t __attribute__((hot))
eth_dec(eth_t x)
{
  return x->rc -= 1;
}

static inline void __attribute__((hot))
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


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                            RECURSIVE SCOPE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup RecScp Recursive Scope
 * @{ */

typedef struct {
  eth_function **clos; /**< Scope closures. */
  size_t nclos; /**< Length of clos-array. */
  size_t rc; /**< Count members with non-zero RC. */
} eth_scp;

eth_scp*
eth_create_scp(eth_function **clos, int nclos);

void
eth_destroy_scp(eth_scp *scp);

void
eth_drop_out(eth_scp *scp);

/** @} RecScp */

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             BUILTIN TYPES
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup BuiltinTypes Builtin Types
 * @brief Builtin data types.
 * @{ */

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               number
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup Number Number
 * @brief The only numeric type natively supported by Ether.
 * @{ */

extern
eth_type *eth_number_type;

/** @brief Check wheter the object is a number. */
#define eth_is_num(x) ((x)->type == eth_number_type)

/** @brief Internal representation of the number. */
typedef struct {
  eth_header header;
  eth_number_t val;
} eth_number;
/** @brief Cast arbitrary pointer into a pointer to \ref eth_number. */
#define ETH_NUMBER(x) ((eth_number*)(x))
/**
 * @brief Get value of a number
 * @return \ref eth_number_t (which usually corresponds to a long double).
 */
#define eth_num_val(x) (ETH_NUMBER(x)->val)

/** @brief Create a number object. */
inline static eth_t __attribute__((malloc))
eth_create_number(eth_number_t val)
{
  eth_number *num = (eth_number*)eth_alloc_h2();
  eth_init_header(num, eth_number_type);
  num->val = val;
  return ETH(num);
}
/** @copydoc eth_create_number */
#define eth_num eth_create_number

#define _ETH_TEST_SNUM(name, type)                    \
  static inline bool                                  \
  eth_is_##name(eth_t x)                              \
  {                                                   \
    eth_number_t num = eth_num_val(x);                \
    return (num <= type##_MAX) & (num >= type##_MIN); \
  }
#define _ETH_TEST_UNUM(name, type)                    \
  static inline bool                                  \
  eth_is_##name(eth_t x)                              \
  {                                                   \
    eth_number_t num = eth_num_val(x);                \
    return (num <= type##_MAX) & (num >= 0);          \
  }
/** @name Test compatibility with native C numeric types
 * @{ */
// native C integer types
_ETH_TEST_SNUM(char, CHAR)
_ETH_TEST_SNUM(signed_char, SCHAR)
_ETH_TEST_UNUM(unsigned_char, UCHAR)
_ETH_TEST_SNUM(short, SHRT)
_ETH_TEST_UNUM(unsigned_short, USHRT)
_ETH_TEST_SNUM(int, INT)
_ETH_TEST_UNUM(unsigned_int, UINT)
_ETH_TEST_SNUM(long, LONG)
_ETH_TEST_UNUM(unsigned_long, ULONG)
_ETH_TEST_SNUM(long_long, LLONG)
_ETH_TEST_UNUM(unsigned_long_long, ULLONG)
// exact-width integer types
_ETH_TEST_SNUM(int8, INT8)
_ETH_TEST_UNUM(uint8, UINT8)
_ETH_TEST_SNUM(int16, INT16)
_ETH_TEST_UNUM(uint16, UINT16)
_ETH_TEST_SNUM(int32, INT32)
_ETH_TEST_UNUM(uint32, UINT32)
_ETH_TEST_SNUM(int64, INT64)
_ETH_TEST_UNUM(uint64, UINT64)
// other integer types
_ETH_TEST_UNUM(size, SIZE)
_ETH_TEST_SNUM(intmax, INTMAX)
_ETH_TEST_UNUM(uintmax, UINTMAX)
// native C float types
_ETH_TEST_SNUM(float, FLT)
_ETH_TEST_SNUM(double, DBL)
_ETH_TEST_SNUM(long_double, LDBL)
/** @} */
#undef _ETH_TEST_SNUM
#undef _ETH_TEST_UNUM

/** @} Number */

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               function
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_function_type;
#define eth_is_fn(x) ((x)->type == eth_function_type)

typedef struct {
  int rc;
  eth_ast *ast;
  eth_ir *ir;
  eth_ssa *ssa;
} eth_source;

eth_source*
eth_create_source(eth_ast *ast, eth_ir *ir, eth_ssa *ssa);

void
eth_ref_source(eth_source *src);

void
eth_unref_source(eth_source *src);

void
eth_drop_source(eth_source *src);

struct eth_function {
  eth_header header;
  bool islam;
  int32_t arity;

  union {
    struct {
      eth_t (*handle)(void);
      void *data;
      void (*dtor)(void *data);
    } proc;

    struct {
      int ncap;
      eth_source *src;
      eth_bytecode *bc;
      eth_t *cap;
      eth_scp *scp;
    } clos;
  };
};
#define ETH_FUNCTION(x) ((eth_function*)(x))


eth_t __attribute__((malloc))
eth_create_proc(eth_t (*f)(void), int n, void *data, void (*dtor)(void*), ...);
#define eth_proc(...) eth_create_proc(__VA_ARGS__, 0, 0, 0)

eth_t __attribute__((malloc))
eth_create_clos(eth_source *src, eth_bytecode *bc, eth_t *cap, int ncap, int arity);

eth_t __attribute__((malloc))
eth_create_dummy_func(int arity);

void
eth_finalize_clos(eth_function *func, eth_source *src, eth_bytecode *bc,
    eth_t *cap, int ncap, int arity);

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
extern
eth_type *eth_exception_type;

static inline bool
eth_is_exn(eth_t x)
{
  return x->type == eth_exception_type;
}

typedef struct {
  eth_header header;
  eth_t what;
  int tracelen;
  eth_location **trace;
} eth_exception;
#define ETH_EXCEPTION(x) ((eth_exception*)(x))
#define eth_what(x) (ETH_EXCEPTION(x)->what)

eth_t __attribute__((malloc))
eth_create_exception(eth_t what);
#define eth_exn eth_create_exception

eth_t
eth_copy_exception(eth_t exn);

void
eth_push_trace(eth_t exn, eth_location *loc);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                              exit-object
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_exit_type;

typedef struct {
  eth_header header;
  int status;
} eth_exit_object;
#define eth_get_exit_status(x) (((eth_exit_object*)(x))->status)

eth_t
eth_create_exit_object(int status);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                string
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_string_type;
#define eth_is_str(x) ((x)->type == eth_string_type)

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

eth_t
eth_create_string_from_char(char c);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               regexp
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_regexp_type;

typedef struct eth_regexp eth_regexp;

const int*
eth_ovector(void);

eth_t
eth_create_regexp(const char *pat, int opts, const char **eptr, int *eoffs);

void
eth_study_regexp(eth_t x);

int
eth_get_regexp_ncaptures(eth_t x);

int
eth_exec_regexp(eth_t x, const char *str, int len, int opts);

#ifdef _PCRE_H
#endif

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               boolean
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_boolean_type;

extern
eth_t eth_true;

extern
eth_t eth_false;

extern
eth_t eth_false_true[2];

static inline eth_t
eth_boolean(bool val)
{
  return eth_false_true[val];
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                nil
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_nil_type;

extern
eth_t eth_nil;

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               pair
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_pair_type;
#define eth_is_pair(x) ((x)->type == eth_pair_type)

typedef struct {
  eth_header header;
  eth_t car;
  eth_t cdr;
} eth_pair;
#define ETH_PAIR(x) ((eth_pair*)(x))

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

static inline eth_t
eth_set_car(eth_t p, eth_t x)
{
  return ETH_PAIR(p)->car = x;
}

static inline eth_t
eth_set_cdr(eth_t p, eth_t x)
{
  return ETH_PAIR(p)->cdr = x;
}

static inline eth_t __attribute__((malloc))
eth_cons_noref(eth_t car, eth_t cdr)
{
  eth_pair *pair = (eth_pair*)eth_alloc_h2();
  eth_init_header(pair, eth_pair_type);
  eth_set_car(ETH(pair), car);
  eth_set_cdr(ETH(pair), cdr);
  return ETH(pair);
}

static inline eth_t __attribute__((malloc))
eth_cons(eth_t car, eth_t cdr)
{
  eth_pair *pair = (eth_pair*)eth_alloc_h2();
  eth_init_header(pair, eth_pair_type);
  eth_ref(eth_set_car(ETH(pair), car));
  eth_ref(eth_set_cdr(ETH(pair), cdr));
  return ETH(pair);
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

static inline eth_t
eth_reverse(eth_t l)
{
  eth_t acc = eth_nil;
  while (eth_is_pair(l))
  {
    acc = eth_cons(eth_car(l), acc);
    l = eth_cdr(l);
  }
  return acc;
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
extern
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

eth_hash_t
eth_get_symbol_hash(eth_t x);
#define eth_sym_hash eth_get_symbol_hash

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                             variants
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  eth_header header;
  eth_t val;
} eth_variant;
#define ETH_VARIANT(x) ((eth_variant*)(x))
#define eth_var_val(x) (ETH_VARIANT(x)->val)

eth_type*
eth_variant_type(const char *tag);

eth_t
eth_create_variant(eth_type *type, eth_t val);

static inline const char* __attribute__((pure))
eth_get_variant_tag(eth_t x)
{
  return (const char*)x->type->clos;
}

static inline eth_t __attribute__((pure))
eth_get_variant_value(eth_t x)
{
  return ETH_VARIANT(x)->val;
}

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
  eth_tuple *tup = (eth_tuple*)eth_alloc_h2();
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
  eth_tuple *tup = (eth_tuple*)eth_alloc_h3();
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
  eth_tuple *tup = (eth_tuple*)eth_alloc_h4();
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
  eth_tuple *tup = (eth_tuple*)eth_alloc_h5();
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
  eth_tuple *tup = (eth_tuple*)eth_alloc_h6();
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
eth_tuple_size(const eth_type *type)
{
  return type->nfields;
}

static inline int __attribute__((pure))
eth_record_size(const eth_type *type)
{
  return type->nfields;
}

eth_t __attribute__((malloc))
eth_create_record(eth_type *type, eth_t const data[]);

eth_t __attribute__((malloc))
eth_record(char *const keys[], eth_t const vals[], int n);

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

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               objects
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/*
 * Syntax `class ... = object ... end` creates an object of type "class" (call
 * it class-specification).  Syntax `new ...` takes a class-specification as
 * an argument and creates an instance of this class (call it class-instance).
 *
 * A class-instance is implemented as a record containing all the fields and a
 * reference to the class-specification (field `__class`).
 *
 * Methods are stored in the class-specification and invoked via operator `#`.
 *
 */

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 file
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_file_type;
#define eth_is_file(x) ((x)->type == eth_file_type)

extern
eth_t eth_stdin, eth_stdout, eth_stderr;

eth_t
eth_open(const char *path, const char *mod);

eth_t
eth_open_fd(int fd, const char *mod);

eth_t
eth_open_stream(FILE *stream);

eth_t
eth_open_pipe(const char *command, const char *mod);

int
eth_is_open(eth_t x);

int
eth_is_pipe(eth_t x);

int
eth_close(eth_t x);

FILE*
eth_get_file_stream(eth_t x);

void
eth_set_file_data(eth_t x, void *data, void (*dtor)(void*));

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                ranges
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_rangelr_type, *eth_rangel_type, *eth_ranger_type;

typedef struct {
  eth_header header;
  eth_t l, r;
} eth_rangelr;
#define ETH_RANGELR(x) ((eth_rangelr*)(x))

typedef struct {
  eth_header header;
  eth_t l;
} eth_rangel;
#define ETH_RANGEL(x) ((eth_rangel*)(x))

typedef struct {
  eth_header header;
  eth_t r;
} eth_ranger;
#define ETH_RANGER(x) ((eth_ranger*)(x))

static inline bool
eth_is_range(eth_t x)
{
  return x->type == eth_rangelr_type
      or x->type == eth_rangel_type
      or x->type == eth_ranger_type
  ;
}

static inline bool
eth_is_rangelr(eth_t x)
{
  return x->type == eth_rangelr_type;
}

static inline bool
eth_is_rangel(eth_t x)
{
  return x->type == eth_rangel_type;
}

static inline bool
eth_is_ranger(eth_t x)
{
  return x->type == eth_ranger_type;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 ref
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
extern
eth_type *eth_strong_ref_type;

struct eth_mut_ref {
  eth_header header;
  eth_t val;
};
#define ETH_REF(x) ((eth_mut_ref*)(x))
#define eth_ref_get(x) (ETH_REF(x)->val)

static inline void
eth_set_strong_ref(eth_t ref, eth_t x)
{
  eth_ref(x);
  eth_unref(eth_ref_get(ref));
  ETH_REF(ref)->val = x;
}

eth_t
eth_create_strong_ref(eth_t init);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                               vector
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup Vector Vector
 * @brief Container optimised for fast random access operations.
 * @{ */
#define ETH_VECTOR_SLICE_SIZE_LOG2 5
#define ETH_VECTOR_SLICE_SIZE (1 << ETH_VECTOR_SLICE_SIZE_LOG2)

extern
eth_type *eth_vector_type;

#define eth_is_vec(x) ((x)->type == eth_vector_type)

eth_t
eth_create_vector(void);

int
eth_vec_len(eth_t v);

eth_t
eth_vec_get(eth_t v, int k);

void
eth_push_mut(eth_t vec, eth_t x);

eth_t
eth_push_pers(eth_t v, eth_t x);

void
eth_insert_mut(eth_t v, int k, eth_t x);

eth_t
eth_insert_pers(eth_t v, int k, eth_t x);

eth_t
eth_front(eth_t v);

eth_t
eth_back(eth_t v);

/** @brief Iterator over vector slices.
 *
 * @details This strucutre, together with related functions allows for an
 * efficient iteration over vector elements. This approach should be preferred
 * over a trivial iteration via eth_vector_get() whenever it is possible.
 *
 * The interface is the following:
 * - eth_vector_begin() sets up an iterator to point to the initial slice a
 *   vector. User can now read this slice.
 * - eth_vector_next() will advance the iterator to the next slice of the
 *   vector. **Note that this function allways shifts an iterator**, thus to
 *   read the first slice one has to do *between* calls to eth_vector_begin()
 *   and eth_vector_next().
 * - To check whether theres no more slices left to read, one can either test
 *   a return value of eth_vector_next() or test a corresponding field,
 *   eth_vector_iterator.isend.
 *
 * A typical loop:
 * @code{.c}
 * eth_vector_iterator iter;
 * eth_vector_begin(v, &iter, 0);
 * while (not iter.isend)
 * {
 *   for (eth_t *p = iter.slice.begin; p != iter.slice.end; ++p)
 *   {
 *     eth_t x = *p;
 *     ...
 *   }
 *    eth_vector_next(&iter);
 *  }
 * @endcode
 */
typedef struct {
  struct {
    eth_t *begin;
    eth_t *end;
    int offset;
  } slice;

  eth_t vec;
  bool isend;
} eth_vector_iterator;

/** @brief Initialize vector iterator
 * @details Initialize iterator at a index sipecified by @p start. Vector's
 * slice can be accessed immediately after this call.
 */
void
eth_vector_begin(eth_t vec, eth_vector_iterator *iter, int start);

/** @brief Advance vector iterator.
 * @details Advance iterator to the next vector slice, if available.
 */
bool
eth_vector_next(eth_vector_iterator *iter);
/** @} Vector */

/** @} BuiltinTypes */

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             ATTRIBUTES
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef enum {
  ETH_ATTR_BUILTIN    = (1 << 0),
  ETH_ATTR_PUB        = (1 << 1),
  ETH_ATTR_DEPRECATED = (1 << 2),
  ETH_ATTR_MUT        = (1 << 3),
} eth_attr_t;

typedef struct {
  int rc;
  int flag;
  char *help;
  eth_location *loc;
} eth_attr;

eth_attr*
eth_create_attr(int flag);

void
eth_set_help(eth_attr *attr, const char *help);

void
eth_set_location(eth_attr *attr, eth_location *loc);

void
eth_ref_attr(eth_attr *attr);

void
eth_unref_attr(eth_attr *attr);

void
eth_drop_attr(eth_attr *attr);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                       MODULES AND ENVIRONMENT
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 Root
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @name Root environment
 * @{ */
eth_root* __attribute__((malloc))
eth_create_root(void);

void
eth_destroy_root(eth_root *root);

eth_env*
eth_get_root_env(eth_root *root);

#define ETH_MFLAG_READY (1 << 0)

typedef struct {
  eth_module *mod;
  void *dl;
  int flag;
} eth_module_entry;

eth_module_entry*
eth_memorize_module(eth_root *root, const char *path, eth_module *mod);

void
eth_forget_module(eth_root *root, const char *path);

/**
 * @brief Get memorized module located under given path.
 *
 * @param root Top level environment.
 * @param path Full path to the module entry.
 *
 * @return Module instance or `NULL`.
 */
eth_module_entry*
eth_get_memorized_module(const eth_root *root, const char *path);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                            builtins
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
eth_module*
eth_create_builtins(eth_root *root);

eth_t
eth_get_builtin(eth_root *root, const char *name);

const eth_module*
eth_get_builtins(const eth_root *root);

/** @} Root environment */

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                              module
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  char *ident;
  eth_t val;
  eth_attr *attr;
} eth_def;

eth_module* __attribute__((malloc))
eth_create_module(const char *name, const char *dir);

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

void
eth_define_attr(eth_module *mod, const char *ident, eth_t val, eth_attr *attr);

void
eth_copy_defs(const eth_module *src, eth_module *dst);

eth_def*
eth_find_def(const eth_module *mod, const char *ident);

void
eth_add_destructor(eth_module *mod, void *data, void (*dtor)(void*));

/**
 * @brief Add a callback on module finalization.
 *
 * This callback will be invoked by the environment destuctor.
 *
 * Can be used to implement `atexit (3)` -like functions.
 *
 * A use case motivated this utility is to implement
 * <a href="https://libuv.org/">libuv</a> which can now call start its main loop
 * at finalization of the root environment.
 *
 * @param env Environment.
 * @param cb Callback.
 * @param data User data to be passed to the callback upon evaluation.
 */
void
eth_add_exit_handle(eth_module *mod, void (*cb)(void*), void *data);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                           environment
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup Env Environment
 * @brief The thing managing modules.
 * @{ */
/** @brief Create new empty environment. */
eth_env* __attribute__((malloc))
eth_create_empty_env(void);

/**
 * @brief Create new environment.
 *
 * This environment will automaticly have a canonical module-path being added.
 * It is equivalent to:
 * ```
 * eth_env *env = eth_create_empty_env();
 * eth_add_module_path(env, eth_get_module_path());
 * ```
 * However, when the *prefix* is not available, it will not yield an error, but
 * the corresponding path wont be added. Thus in this case this function is
 * equivalent to the eth_create_empty_env().
 */
eth_env* __attribute__((malloc))
eth_create_env(void);

/** @brief Destroy environemnt. */
void
eth_destroy_env(eth_env *env);

/**
 * @brief Add a directory to be searched during module query.
 * @return `true` on success, or `false` in case of invalid path.
 */
bool
eth_add_module_path(eth_env *env, const char *path);

void
eth_copy_module_path(const eth_env *src, eth_env *dst);

/**
 * @brief Search for a file or directory in a module directories list.
 *
 * For each *directory* in *module direcories list* check wheter a
 * concatenation of *directory* and a *path* yields a valid path to a
 * file/directory. Once a file/directory is found, query stops and obtaind
 * full path is returned.
 *
 * @param env Environment to be queried.
 * @param path Relative path to a file/directory.
 * @param[out] fullpath Buffer to be filled with resolved full path on success.
 *                      On failure, content of supplied array is undefined.
 *
 * @return `true` on success; otherwize, `false` is returned.
 *
 * @see eth_add_module() to append a path to the list.
 */
bool
eth_resolve_path(eth_env *env, const char *path, char *fullpath);

bool
eth_resolve_modpath(eth_env *env, const char *path, char *fullpath);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                         importing code
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @brief Get module instance by name. */
eth_module*
eth_require_module(eth_root *root, eth_env *env, const char *modpath);

bool
eth_load_module_from_ast(eth_root *root, eth_module *mod, eth_ast *ast,
    eth_t *ret);

bool
eth_load_module_from_ast2(eth_root *root, eth_module *mod, eth_ast *ast,
    eth_t *ret, eth_module *uservars);

/** @name Loading modules from scripts or shared libraries
 * @{ */
eth_module*
eth_load_module_from_script(eth_root *root, const char *path, eth_t *ret);

eth_module*
eth_load_module_from_script2(eth_root *root, const char *path, eth_t *ret,
    eth_module *uservars);

eth_module*
eth_load_module_from_elf(eth_root *root, const char *path);
/** @} */

/** @} Env */

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                              LOCATIONS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct eth_location {
  int rc;
  int fl, fc, ll, lc;
  char *filepath;
};

eth_location*
eth_create_location(int fl, int fc, int ll, int lc, const char *path);

void
eth_ref_location(eth_location *loc);

void
eth_unref_location(eth_location *loc);

void
eth_drop_location(eth_location *loc);

char*
eth_get_location_file(eth_location *loc, char *out);

int
eth_print_location(eth_location *loc, FILE *stream);

#define ETH_LOPT_FILE       (1 << 0)
#define ETH_LOPT_NEWLINES   (1 << 1)
#define ETH_LOPT_EXTRALINES (1 << 2)
#define ETH_LOPT_NOCOLOR    (1 << 3)
#define ETH_LOPT_NOLINENO   (1 << 4)

int
eth_print_location_opt(eth_location *loc, FILE *stream, int opt);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                                PATTERNS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup Patterns Patterns
 * @{ */

typedef enum {
  /**
   * @brief Dummy placeholder (i.e. `_`)
   */
  ETH_PATTERN_DUMMY,

  /**
   * @brief Wildcard pattern for variable binding
   */
  ETH_PATTERN_IDENT,

  /**
   * @brief Destructure a value (of known type)
   */
  ETH_PATTERN_UNPACK,

  /**
   * @brief Match with a constant
   */
  ETH_PATTERN_CONSTANT,

  /**
   * @brief Destructure (arbitrary) record
   *
   * @note Currently can be also used to access **a** field of **any** plain
   * type (one-pass load of multiple fields only works for records).
   */
  ETH_PATTERN_RECORD,
} eth_pattern_tag;

/**
 * @brief Check whether a pattern is a wildcard
 *
 * Wildcard patterns match any given value.<br>
 * Patterns recognized as *wildcard* are
 * - ETH_PATTERN_DUMMY
 * - ETH_PATTERN_IDENT
 */
static inline bool
eth_is_wildcard(eth_pattern_tag tag)
{
  return tag == ETH_PATTERN_DUMMY or tag == ETH_PATTERN_IDENT;
}

/** @} Patterns */


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                         ABSTRACT SYNTAX TREE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup AST Abstract Syntax Tree
 * @{ */

/***************************************************************************//**
 * @defgroup AstPatterns AST Patterns
 *
 * Here you can find information for how to create and work with patterns on the
 * level of Abstract Syntax Tree.
 *
 * Constructors for all pattern types listed in section \ref Patterns are given:
 * | Pattern              | Constructor                |
 * |----------------------|----------------------------|
 * | ETH_PATTERN_DUMMY    | eth_ast_dummy_pattern()    |
 * | ETH_PATTERN_IDENT    | eth_ast_ident_pattern()    |
 * | ETH_PATTERN_CONSTANT | eth_ast_constant_pattern() |
 * | ETH_PATTERN_UNPACK   | eth_ast_unpack_pattern()   |
 * | ETH_PATTERN_RECORD   | eth_ast_record_pattern()   |
 * Unpack and record patterns can be also supplied with an alias.
 *
 * Pattern objects are supplied with reference counters. Interface implementing
 * RC-protocol for shared objects is given by
 * - eth_ref_ast_pattern()
 * - eth_unref_ast_pattern()
 * - eth_drop_ast_pattern()
 *
 * However, one usually creates pattern just before feeding it to some AST node,
 * which will increment reference count, and so, will take the ownership. Thus
 * manual management of reference count is (in most cases) not required from
 * the user.
 *
 * @{ */
typedef enum {
  /**
   * @brief Dummy placeholder (i.e. `_`)
   */
  ETH_AST_PATTERN_DUMMY = ETH_PATTERN_DUMMY,

  /**
   * @brief Wildcard pattern for variable binding
   */
  ETH_AST_PATTERN_IDENT = ETH_PATTERN_IDENT,

  /**
   * @brief Destructure a value (of known type)
   */
  ETH_AST_PATTERN_UNPACK = ETH_PATTERN_UNPACK,

  /**
   * @brief Match with a constant
   */
  ETH_AST_PATTERN_CONSTANT = ETH_PATTERN_CONSTANT,

  /**
   * @brief Destructure (arbitrary) record
   *
   * @note Currently can be also used to access **a** field of **any** plain
   * type (one-pass load of multiple fields only works for records).
   */
  ETH_AST_PATTERN_RECORD = ETH_PATTERN_RECORD,

  ETH_AST_PATTERN_RECORD_STAR,
} eth_ast_pattern_tag;

typedef struct eth_ast_pattern eth_ast_pattern;
struct eth_ast_pattern {
  eth_ast_pattern_tag tag; /**< @brief Pattern type */
  int rc; /**< @brief Reference counter */

  union {
    struct {
      char *str; /**< Identifier string */
      eth_attr *attr; /**< Identifier attribute (see eth_attr_t).
                       * @note May be set to NULL. */
    } ident; /**< @brief Identifier pattern */

    struct {
      eth_type *type; /**< Target type */
      char **fields; /**< Field names */
      eth_ast_pattern **subpats; /**< Nested patterns for fields */
      int n; /**< N fields */
      char *alias; /**< Alias */
    } unpack; /**< @brief Known-type destructuring pattern */

    struct { eth_t val; } constant; /**< @brief Constant pattern */

    struct {
      char **fields; /**< Field names */
      eth_ast_pattern **subpats; /**< Nested patterns for fields */
      int n; /**< N fields */
      char *alias; /**< Alias */
    } record; /**< @brief Record destructuring pattern */

    struct { eth_attr *attr; char *alias; } recordstar;
  };
};

/**
 * Corresponding native syntax for this pattern is `_`.
 * @see \ref ETH_PATTERN_DUMMY
 */
eth_ast_pattern*
eth_ast_dummy_pattern(void);

/**
 * @see \ref ETH_PATTERN_IDENT
 */
eth_ast_pattern*
eth_ast_ident_pattern(const char *ident);

/**
 * @brief Set identifier attribute.
 */
void
eth_set_ident_attr(eth_ast_pattern *ident, eth_attr *attr);

/**
 * In native syntax this pattern is used for matching with builtin compund types
 * like tupes, variants and pairs.
 *
 * @see \ref ETH_PATTERN_UNPACK
 */
eth_ast_pattern*
eth_ast_unpack_pattern(eth_type *type, char *const fields[],
    eth_ast_pattern *const pats[], int n);

/**
 * @see \ref ETH_PATTERN_CONSTANT
 */
eth_ast_pattern*
eth_ast_constant_pattern(eth_t cval);

/**
 * Corresponding native syntax for this pattern is `{ ... }`.
 *
 * @see \ref ETH_PATTERN_RECORD
 */
eth_ast_pattern*
eth_ast_record_pattern(char *const fields[], eth_ast_pattern *pats[], int n);

eth_ast_pattern*
eth_ast_record_star_pattern();

/**
 * Aliases are used to bind matched expression with an identifier.
 *
 * Corresponding native syntax for this utility is `<pattern> as <identifier>`.
 *
 * @note Only allowed for \ref eth_ast_unpack_pattern() "unpack"- and
 * \ref eth_ast_record_pattern() "record"-patterns. Repeated application of this
 * function to the same eth_ast_pattern object will overwrite previous alias
 * (if present) with the new one.
 */
void
eth_set_pattern_alias(eth_ast_pattern *pat, const char *alias);

/**
 * @brief Increment reference count
 */
void
eth_ref_ast_pattern(eth_ast_pattern *pat);

/**
 * @brief Decrement reference count
 */
void
eth_unref_ast_pattern(eth_ast_pattern *pat);

/**
 * @brief Release pattern
 */
void
eth_drop_ast_pattern(eth_ast_pattern *pat);

/** @} AstPatterns */

/***************************************************************************//**
 * @defgroup AstNodes AST Nodes
 * @{ */

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
  ETH_AST_AND,
  ETH_AST_OR,
  ETH_AST_ACCESS,
  ETH_AST_TRY,
  ETH_AST_MKRCRD,
  ETH_AST_UPDATE,
  ETH_AST_ASSERT,
  ETH_AST_DEFINED,
  ETH_AST_EVMAC,
  ETH_AST_MULTIMATCH,
  ETH_AST_ASSIGN,
  ETH_AST_RETURN,
  ETH_AST_CLASS,
} eth_ast_tag;

typedef enum {
  ETH_TOPLVL_NONE,
  ETH_TOPLVL_THEN,
  ETH_TOPLVL_ELSE,
} eth_toplvl_flag;

typedef struct {
  int h, w;
  eth_ast_pattern ***tab;
  eth_ast **exprs;
} eth_match_table;

eth_match_table*
eth_create_match_table(eth_ast_pattern **const tab[], eth_ast *const exprs[],
    int h, int w);

void
eth_destroy_match_table(eth_match_table *table);

typedef struct {
  char *name;
  eth_ast *init;
} eth_class_val;

typedef struct {
  char *name;
  eth_ast *fn;
} eth_class_method;

typedef struct {
  char *classname;
  eth_ast **args;
  int nargs;
} eth_class_inherit;

struct eth_ast {
  eth_ast_tag tag;
  eth_word_t rc;
  union {
    struct { eth_t val; } cval;
    struct { char *str; } ident;
    struct { eth_ast *fn, **args; int nargs; } apply;
    struct { eth_ast *cond, *then, *els; } iff;
    struct { eth_ast *e1, *e2; } seq;
    struct { eth_ast_pattern **pats; eth_ast **vals; eth_ast *body; int n; }
      let, letrec;
    struct { eth_binop op; eth_ast *lhs, *rhs; } binop;
    struct { eth_unop op; eth_ast *expr; } unop;
    struct { eth_ast_pattern **args; int arity; eth_ast *body; } fn;
    struct { eth_ast_pattern *pat; eth_ast *expr, *thenbr, *elsebr;
             eth_toplvl_flag toplvl; }
      match;
    struct { eth_ast *lhs, *rhs; } scand, scor;
    struct { eth_ast *expr; char *field; } access;
    struct { eth_ast_pattern *pat; eth_ast *trybr, *catchbr; int likely;
             bool _check_exit; }
      try;
    struct { eth_type *type; char **fields; eth_ast **vals; int n; } mkrcrd;
    struct { eth_ast *src, **vals; char **fields; int n; } update;
    struct { eth_ast *expr; } assert;
    struct { char *ident; } defined;
    struct { eth_match_table *table; eth_ast **exprs; } multimatch;
    struct { eth_ast *expr; } evmac;
    struct { char *ident; eth_ast *val; } assign;
    struct { eth_ast_pattern **pars; int npars; eth_class_inherit *inherits;
      int ninherits; eth_class_val *vals; int nvals;
      eth_class_method *methods; int nmethods; } clas;
    struct { eth_ast *expr; } retrn;
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

static void
eth_set_let_expr(eth_ast *let, eth_ast *expr)
{
  eth_ref_ast(expr);
  eth_unref_ast(let->let.body);
  let->let.body = expr;
}

eth_ast* __attribute__((malloc))
eth_ast_letrec(eth_ast_pattern *const pats[], eth_ast *const *vals, int n,
    eth_ast *body);

static void
eth_set_letrec_expr(eth_ast *let, eth_ast *expr)
{
  eth_ref_ast(expr);
  eth_unref_ast(let->letrec.body);
  let->let.body = expr;
}

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
eth_ast_and(eth_ast *lhs, eth_ast *rhs);

eth_ast* __attribute__((malloc))
eth_ast_or(eth_ast *lhs, eth_ast *rhs);

eth_ast* __attribute__((malloc))
eth_ast_access(eth_ast *expr, const char *field);

eth_ast* __attribute__((malloc))
eth_ast_try(eth_ast_pattern *pat, eth_ast *try, eth_ast *catch, int likely);

eth_ast*
eth_ast_make_record(eth_type *type, char *const fields[], eth_ast *const vals[],
    int n);

eth_ast*
eth_ast_update(eth_ast *src, eth_ast *const vals[], char *const fields[], int n);

eth_ast*
eth_ast_assert(eth_ast *expr);

eth_ast*
eth_ast_defined(const char *ident);

eth_ast*
eth_ast_evmac(eth_ast *expr);

eth_ast*
eth_ast_multimatch(eth_match_table *table, eth_ast *const exprs[]);

eth_ast*
eth_ast_assign(const char *ident, eth_ast *val);

eth_ast*
eth_ast_return(eth_ast *expr);

eth_ast*
eth_ast_class(eth_ast_pattern *const pars[], int npars,
    eth_class_inherit *inherits, int ninherits, eth_class_val *vals,
    int nvals, eth_class_method *methods, int nmethods);

eth_ast_pattern*
eth_ast_to_pattern(eth_ast *ast);

/** @} AstNodes */

typedef struct eth_scanner eth_scanner;
typedef struct eth_scanner_data eth_scanner_data;

eth_scanner* __attribute__((malloc))
eth_create_scanner(eth_root *root, FILE *stream);

eth_scanner* __attribute__((malloc))
eth_create_repl_scanner(eth_root *root, FILE *stream);

bool
eth_is_repl_scanner(eth_scanner *scan);

void
eth_destroy_scanner(eth_scanner *scan);

FILE*
eth_get_scanner_input(eth_scanner *scan);

eth_scanner_data*
eth_get_scanner_data(eth_scanner *scan);

eth_ast* __attribute__((malloc))
eth_parse(eth_root *root, FILE *stream);

/** @} AST */


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                  INTERMEDIATE SYNTAX TREE REPRESENTATION
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct eth_ir_pattern eth_ir_pattern;
struct eth_ir_pattern {
  eth_pattern_tag tag;
  union {
    struct { int varid; } ident;
    struct { eth_type *type; int n, *offs, varid; eth_ir_pattern **subpats; }
      unpack;
    struct { eth_t val; } constant;
    struct { size_t *ids; int n, varid; eth_ir_pattern **subpats; } record;
  };
};

eth_ir_pattern*
eth_ir_dummy_pattern(void);

eth_ir_pattern*
eth_ir_ident_pattern(int varid);

eth_ir_pattern*
eth_ir_unpack_pattern(int varid, eth_type *type, int offs[],
    eth_ir_pattern *pats[], int n);

eth_ir_pattern*
eth_ir_constant_pattern(eth_t val);

eth_ir_pattern*
eth_ir_record_pattern(int varid, size_t const ids[],
    eth_ir_pattern *const pats[], int n);

void
eth_destroy_ir_pattern(eth_ir_pattern *pat);

typedef enum {
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
  ETH_IR_MULTIMATCH,
  ETH_IR_STARTFIX,
  ETH_IR_ENDFIX,
  ETH_IR_MKRCRD,
  ETH_IR_UPDATE,
  ETH_IR_THROW,
  ETH_IR_RETURN,
} eth_ir_tag;

typedef struct {
  int h, w;
  eth_ir_pattern ***tab;
  eth_ir_node **exprs;
} eth_ir_match_table;

eth_ir_match_table*
eth_create_ir_match_table(eth_ir_pattern **const tab[],
    eth_ir_node *const exprs[], int h, int w);

void
eth_destroy_ir_match_table(eth_ir_match_table *table);

struct eth_ir_node {
  eth_ir_tag tag;
  int rc;
  union {
    struct { eth_t val; } cval;
    struct { int vid; } var;
    struct { eth_ir_node *fn, **args; int nargs; } apply;
    struct { eth_ir_node *cond, *thenbr, *elsebr; eth_toplvl_flag toplvl;
             int likely; }
      iff;
    struct { int exnvar; eth_ir_node *trybr, *catchbr; int likely; } try;
    struct { eth_ir_node *e1, *e2; } seq;
    struct { eth_binop op; eth_ir_node *lhs, *rhs; } binop;
    struct { eth_unop op; eth_ir_node *expr; } unop;
    struct { int arity, *caps, *capvars, ncap; eth_ast *ast; eth_ir *body; } fn;
    struct { eth_ir_pattern *pat; eth_ir_node *expr, *thenbr, *elsebr;
             eth_toplvl_flag toplvl; int likely; }
      match;
    struct { int *vars, n; eth_ir_node *body; } startfix;
    struct { int *vars, n; eth_ir_node *body; } endfix;
    struct { eth_type *type; eth_ir_node **fields; } mkrcrd;
    struct { eth_ir_node *src, **fields; size_t *ids; int n; } update;
    struct { eth_ir_node *exn; } throw;
    struct { eth_ir_node *expr; } retrn;
    struct { eth_ir_match_table *table; eth_ir_node **exprs; } multimatch;
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
eth_ir_fn(int arity, int *caps, int *capvars, int ncap, eth_ir *body,
    eth_ast *ast);

eth_ir_node* __attribute__((malloc))
eth_ir_match(eth_ir_pattern *pat, eth_ir_node *expr, eth_ir_node *thenbr,
    eth_ir_node *elsebr);

eth_ir_node*
eth_ir_multimatch(eth_ir_match_table *table, eth_ir_node *const exprs[]);

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
eth_ir_update(eth_ir_node *src, eth_ir_node *const fields[], size_t const ids[],
    int n);

eth_ir_node*
eth_ir_throw(eth_ir_node *exn);

eth_ir_node*
eth_ir_return(eth_ir_node *expr);

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
  const char *ident;
  eth_t cval;
  int vid;
  eth_attr *attr;
};

static inline eth_var_cfg
eth_dyn_var(char *ident, int vid, eth_attr *attr)
{
  return (eth_var_cfg) { .ident = ident, .cval = NULL, .vid = vid, .attr = attr };
}

static inline eth_var_cfg
eth_const_var(const char *ident, eth_t cval, eth_attr *attr)
{
  return (eth_var_cfg) { .ident = ident, .cval = cval, .vid = -1, .attr = attr };
}

struct eth_var {
  char *ident;
  eth_t cval;
  int vid;
  eth_var *next;
  eth_attr *attr;
};

static inline eth_var_cfg
eth_copy_var(eth_var* var, int vid)
{
  return (eth_var_cfg) {
    .ident = var->ident,
    .cval = var->cval,
    .vid = vid,
    .attr = var->attr,
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

typedef enum {
  ETH_IRDEF_VAR, /**< @brief Runtime variable. */
  ETH_IRDEF_CVAL, /**< @brief Constant. */
} eth_ir_def_tag;

typedef struct {
  eth_ir_def_tag tag;
  char *ident;
  eth_attr *attr;
  union { int varid; eth_t cval; };
} eth_ir_def;

typedef struct {
  eth_ir_def *defs;
  int n;
} eth_ir_defs;

void
eth_destroy_ir_defs(eth_ir_defs *defs);

eth_ir*
eth_build_ir(eth_ast *ast, eth_root *root, eth_module *uservars);

eth_ir*
eth_build_module_ir(eth_ast *ast, eth_root *root, eth_module *mod,
    eth_ir_defs *defs, eth_module *uservars);


// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             TYPE ANALYSIS
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef enum {
  ETH_ATYPE_ERROR,
  ETH_ATYPE_UNION,
  ETH_ATYPE_RECORD,
  ETH_ATYPE_TUPLE,
  ETH_ATYPE_LIST,
  ETH_ATYPE_FN,
  ETH_ATYPE_ATOM,
} eth_atype_tag;

typedef struct eth_atype eth_atype;

struct eth_atype {
  eth_atype_tag tag;
  int rc;
  union {
    struct { eth_atype **types; int n; } un;
    struct { eth_atype **types; int *ids, n; } record;
    struct { eth_atype **types; int n; } tuple;
    struct { eth_atype *elttype; } list;
    struct { int arity; eth_ir *body; } fn;
    struct { eth_type *type; } atom;
    struct { eth_t val; } cval;
  };
  eth_type *type;
};

void
eth_ref_atype(eth_atype *aty);

void
eth_unref_atype(eth_atype *aty);

void
eth_drop_atype(eth_atype *aty);

eth_atype*
eth_create_atype_error(void);

eth_atype*
eth_create_atype_atom(eth_type *type);

eth_atype*
eth_create_atype_list(eth_atype *elttype);

eth_atype*
eth_create_atype_record(int const ids[], eth_atype *const types[], int n);

eth_atype*
eth_create_atype_tuple(eth_atype *const types[], int n);

eth_atype*
eth_create_atype_fn(int arity, eth_ir *body);

eth_atype*
eth_create_atype_union(eth_atype *const types[], int n);

static inline eth_atype*
eth_create_atype_any(void)
{ return eth_create_atype_union(NULL, 0); }

static inline bool
eth_atype_is_atom(eth_atype *atype, eth_type *type)
{ return atype->tag == ETH_ATYPE_ATOM && atype->atom.type == type; }

eth_ir_node*
eth_specialize(const eth_ir *ir);

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                STATIC SINGLE ASSIGNEMENT REPRESENTATION
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
/** @defgroup SSA Static Single Assignment representation
 * @{ */

/***************************************************************************//**
 * @defgroup SsaPatterns SSA Patterns
 * @{ */
/**
 * @note Unpack.type is NULL if no type check required.
 */
typedef enum {
  ETH_TEST_IS,
  ETH_TEST_EQUAL,
} eth_test_op;

typedef struct eth_ssa_pattern eth_ssa_pattern;
struct eth_ssa_pattern {
  eth_pattern_tag tag;
  union {
    struct { int vid; } ident;
    struct { eth_type *type; int *offs, *vids, n; eth_ssa_pattern **subpat;
             bool dotest; }
      unpack;
    struct { eth_t val; eth_test_op testop; bool dotest; } constant;
    struct { size_t *ids; int *vids, n; eth_ssa_pattern **subpat; bool dotest; }
      record;
  };
};

eth_ssa_pattern*
eth_ssa_dummy_pattern(void);

eth_ssa_pattern* __attribute__((malloc))
eth_ssa_ident_pattern(int vid);

eth_ssa_pattern* __attribute__((malloc))
eth_ssa_unpack_pattern(eth_type *type, int const offs[], int const vids[],
    eth_ssa_pattern *const pats[], int n, bool dotest);

eth_ssa_pattern*
eth_ssa_constant_pattern(eth_t val, eth_test_op testop, bool dotest);

eth_ssa_pattern*
eth_ssa_record_pattern(size_t const ids[], int const vids[],
    eth_ssa_pattern *const pats[], int n);

void
eth_destroy_ssa_pattern(eth_ssa_pattern *pat);
/** @} SsaPatterns */

/***************************************************************************//**
 * @defgroup MTree Match-Tree
 * @brief Decision tree implementing multi-pattern matching.
 *
 * ### REFERENCES
 * Luc Maranget. 2008. Compiling pattern matching to good decision trees.
 * In Proceedings of the 2008 ACM SIGPLAN workshop on ML (ML '08).
 * Association for Computing Machinery, New York, NY, USA, 35–46.
 * DOI: https://doi.org/10.1145/1411304.1411311
 * @{ */

/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -
 * @defgroup MTreeCase Match-Tree case clauses
 * @{ */
typedef struct eth_mtree eth_mtree;

/**
 * @brief Case clause for compound type.
 *
 */
typedef struct {
  const eth_type *type; /**< @brief Type identifying a *head-pattern* of the
                         * *switch*. */
  int *offs; /**< @brief Offsets of the fields (of the \ref type) to be loaded
              * into \ref eth_mtree_case::ssavids. */
  int *ssavids; /**< @brief SSA-VIDs for the unpacked values. */
  int n; /**< @brief Length of \ref offs and \ref ssavids arrays. */
  eth_mtree *tree; /**< @brief Tree to be evaluated on successfull match to the
                    * \ref eth_mtree_case::type. */
} eth_mtree_case;

/**
 * @brief Initialize a case-clause.
 *
 * @param c[out] Pointer to an eth_mtree_case to be initialized.
 * @param type Type identifying a *head-pattern* of the *switch*.
 * @param offs Offsets of the fields (of the \ref type) to be loaded into \ref
 *             ssavids.
 * @param ssavids SSA-VIDs for the unpacked values.
 * @param n Length of \ref offs and \ref ssavids arrays.
 * @param tree Tree to be evaluated on successfull match to the \ref type.
 */
void
eth_init_mtree_case(eth_mtree_case *c, const eth_type *type, const int offs[],
    const int ssavids[], int n, eth_mtree *tree);

/**
 * @brief Deallocate resources asquired by an eth_mtree_case..
 */
void
eth_cleanup_mtree_case(eth_mtree_case *c);

typedef struct {
  eth_t cval;
  eth_mtree *tree;
} eth_mtree_ccase;

void
eth_init_mtree_ccase(eth_mtree_ccase *c, eth_t cval, eth_mtree *tree);

void
eth_cleanup_mtree_ccase(eth_mtree_ccase *c);

/** @} MTreeCase */

typedef enum {
  ETH_MTREE_FAIL,
  ETH_MTREE_LEAF,
  ETH_MTREE_SWITCH,
  ETH_MTREE_CSWITCH,
} eth_mtree_tag;

/**
 * ```
 * mtree = Fail
 *       | Leaf code
 *       | Switch ssavid case... default
 *       | CSwitch ssavid ccase... default
 *
 * case = Case type (offs ssavid)... mtree
 *
 * ccase = CCase const mtree
 *
 * default = mtree
 * ```
 */
struct eth_mtree {
  eth_mtree_tag tag;
  union {
    eth_insn *leaf;
    struct { int ssavid; eth_mtree_case *cases; int ncases; eth_mtree *dflt; }
      swtch;
    struct { int ssavid; eth_mtree_ccase *cases; int ncases; eth_mtree *dflt; }
      cswtch;
  };
};

eth_mtree*
eth_create_fail(void);

eth_mtree*
eth_create_leaf(eth_insn *body);

eth_mtree*
eth_create_switch(int ssavid, const eth_mtree_case cases[], int ncases,
    eth_mtree *dflt);

eth_mtree*
eth_create_cswitch(int ssavid, const eth_mtree_ccase cases[], int ncases,
    eth_mtree *dflt);

void
eth_destroy_mtree(eth_mtree *t);

/** @} MTree */
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -
 * @defgroup SsaInsn SSA instructions
 * @{ */
enum {
  /* Disable writes BEFORE given instruction. */
  ETH_IFLAG_NOBEFORE = (1 << 0),
  ETH_IFLAG_ENTRYPOINT = (1 << 1),
};

typedef enum {
  ETH_INSN_NOP,
  ETH_INSN_CVAL,
  ETH_INSN_APPLY,
  ETH_INSN_APPLYTC,
  ETH_INSN_LOOP,
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
} eth_insn_tag;

typedef enum {
  ETH_TEST_NOTFALSE,
  ETH_TEST_TYPE,
  ETH_TEST_MATCH,
  ETH_TEST_UPDATE,
} eth_test_tag;

struct eth_insn {
  eth_insn_tag tag;
  int out;
  int flag;
  char *comment;
  union {
    struct { eth_t val; } cval;
    struct { int vid; } var;
    struct { int fn, *args; int nargs; } apply;
    struct { int *args; int nargs; } loop;
    struct {
      int cond, likely;
      eth_insn *thenbr, *elsebr;
      eth_test_tag test;
      union {
        eth_type *type;
        eth_ssa_pattern *pat;
        struct { size_t *ids; int *vids; int n; } update;
      };
      eth_toplvl_flag toplvl;
    } iff;
    struct { eth_binop op; int lhs, rhs; } binop;
    struct { eth_unop op; int vid; } unop;
    struct { int arity, *caps, ncap; eth_ast *ast; eth_ir *ir; eth_ssa *ssa; }
      fn;
    struct { int arity, *caps, ncap, *movs, nmov; eth_ast *ast; eth_ir *ir;
             eth_ssa *ssa; }
      finfn;
    struct { int arity; } alcfn;
    struct { int *vids, n; } pop;
    struct { int *vids, n; } cap;
    struct { int *clos, nclos; } mkscp;
    struct { int tryid, vid; } catch;
    struct { int id, likely; eth_insn *trybr, *catchbr; } try;
    struct { } getexn;
    struct { int *vids; eth_type *type; } mkrcrd;
    struct { int src, *vids; size_t *ids; } updtrcrd;
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

eth_insn*
eth_insn_loop(const int args[], int nargs);

eth_insn* __attribute__((malloc))
eth_insn_if(int out, int cond, eth_insn *thenbr, eth_insn *elsebr);

eth_insn* __attribute__((malloc))
eth_insn_if_test_type(int out, int cond, eth_type *type, eth_insn *thenbr,
    eth_insn *elsebr);

eth_insn* __attribute__((malloc))
eth_insn_if_match(int out, int cond, eth_ssa_pattern *pat, eth_insn *thenbr,
    eth_insn *elsebr);

eth_insn*
eth_insn_if_update(int out, int src, int *vids, size_t *ids, int n,
    eth_insn *thenbr, eth_insn *elsebr);

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
eth_insn_fn(int out, int arity, int *caps, int ncap, eth_ast *ast, eth_ir *ir,
    eth_ssa *ssa);

eth_insn* __attribute__((malloc))
eth_insn_alcfn(int out, int arity);

eth_insn* __attribute__((malloc))
eth_insn_finfn(int c, int arity, int *caps, int ncap, int *movs, int nmov,
    eth_ast *ast, eth_ir *ir, eth_ssa *ssa);

eth_insn* __attribute__((malloc))
eth_insn_pop(int const vids[], int n);

eth_insn* __attribute__((malloc))
eth_insn_cap(int const vids[], int n);

eth_insn* __attribute__((malloc))
eth_insn_mkscp(int *clos, int nclos);

eth_insn* __attribute__((malloc))
eth_insn_catch(int tryid, int vid);

eth_insn* __attribute__((malloc))
eth_insn_try(int out, int id, eth_insn *trybr, eth_insn *catchbr);

eth_insn* __attribute__((malloc))
eth_insn_getexn(int out);

eth_insn* __attribute__((malloc))
eth_insn_mkrcrd(int out, int const vids[], eth_type *type);

void
eth_set_insn_comment(eth_insn *insn, const char *comment);

/** @} SsaInsn */
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -
 * @defgroup SsaTape SSA tape
 * @{ */
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

/** @} SsaTape */
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -
 * @defgroup SsaChunk SSA chunk
 * @{ */
struct eth_ssa {
  int rc;
  int nvals;
  int ntries;
  eth_insn *body;
};

eth_ssa* __attribute__((malloc))
eth_build_ssa(eth_ir *ir, eth_ir_defs *defs);

void
eth_ref_ssa(eth_ssa *ssa);

void
eth_unref_ssa(eth_ssa *ssa);

void
eth_drop_ssa(eth_ssa *ssa);

void
eth_dump_ssa(const eth_insn *insn, FILE *stream);

/** @} SsaChunk */
/** @} SSA */

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
  ETH_OPC_TESTEQUAL,
  ETH_OPC_GETTEST,

  ETH_OPC_DUP,

  ETH_OPC_JNZ,
  ETH_OPC_JZE,
  ETH_OPC_JMP,
  ETH_OPC_LOOP,

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
  ETH_OPC_EQUAL,
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
  ETH_OPC_UPDTRCRD,
} eth_opc;

struct eth_bc_insn {
#ifdef ETH_DEBUG_MODE
  eth_opc opc;
#else
  uint64_t opc;
#endif
  union {
    struct { uint64_t out; eth_t val; } cval;

    // TODO: mege PUSH with APPLY
    struct { uint64_t *vids, n; } push;
    struct { uint64_t vid0, n; } pop;

    struct { uint64_t out, fn; } apply;

    struct { uint64_t vid; } test;
    struct { uint64_t vid; eth_type *type; } testty;
    struct { uint64_t vid; eth_t cval; } testis;
    struct { uint64_t vid; eth_t cval; } testequal;
    struct { uint64_t out; } gettest;

    struct { uint64_t out, vid; } dup;

    // XXX: dont change any of jnz, jze, jmp: identical layout is mandatory
    struct { ptrdiff_t offs; } jnz, jze, jmp;
    struct { uint64_t *vids, n; ptrdiff_t offs; } loop;

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
        eth_source *src;
        eth_bytecode *bc;
        int ncap;
        int caps[];
      } *restrict data;
    } fn, finfn;
    struct { uint64_t out; int arity; } alcfn;
    struct { uint64_t vid0, n; } cap;
    struct { struct { uint64_t *clos, nclos; } *restrict data; }
      mkscp;

    struct { uint64_t out; uint32_t vid, offs; } load;
    // TODO: flatten vids
    struct { uint32_t src, n; uint64_t *vids; size_t *ids; } loadrcrd;
    struct { uint64_t out, vid; size_t id; } loadrcrd1;

    struct { uint64_t vid; } setexn;
    struct { uint64_t out; } getexn;

    struct { uint64_t out; eth_type *type; uint64_t *vids; } mkrcrd;
    struct { uint32_t out; uint16_t src, n; uint64_t *vids; size_t *ids; }
      updtrcrd;
  };
};

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
//                                REPL
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
eth_ast*
eth_parse_repl(eth_scanner *scan);

typedef struct {
  eth_root *root;
  eth_module *mod;
} eth_evaluator;

eth_t
eth_eval(eth_evaluator *evl, eth_ast *ast);

eth_env*
eth_get_evaluator_env(eth_evaluator *evl);

// ><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><+><
//                             API HELPER
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
    const eth_t _eth_ret = (ret);       \
    eth_ref(_eth_ret);                  \
    for (size_t i = 0; i < args.n; ++i) \
      eth_unref(*eth_sp++);             \
    eth_dec(_eth_ret);                  \
    return _eth_ret;                    \
  } while (0)

#define eth_throw(args, err) \
  eth_return(args, eth_exn(err))

#define eth_rethrow(args, exn) \
  eth_return(args, exn)


#define eth_drop_n(xs...)                                 \
  do {                                                    \
    eth_t _eth_drops_xs[] = { xs };                       \
    for (size_t i = 0; i < sizeof xs / sizeof xs[0]; ++i) \
      eth_ref(_eth_drops_xs[i]);                          \
    for (size_t i = 0; i < sizeof xs / sizeof xs[0]; ++i) \
      eth_unref(_eth_drops_xs[i]);                        \
  } while (0)

#define eth_drop_2(x, y) \
  eth_ref(x);            \
  eth_drop(y);           \
  eth_unref(x);

#define eth_drop_3(x, y, z) \
  eth_ref(x);               \
  eth_ref(y);               \
  eth_drop(z);              \
  eth_unref(x);             \
  eth_unref(y);

#define eth_use_symbol_as(ident, sym) \
  static eth_t ident = NULL;          \
  if (eth_unlikely(ident == NULL))    \
    ident = eth_sym(sym);

#define eth_use_symbol(ident) eth_use_symbol_as(ident, #ident)

#define eth_use_variant_as(ident, var)          \
  static eth_type *ident##_type;                \
  if (eth_unlikely(ident##_type == NULL))       \
    ident##_type = eth_variant_type(var);       \
  eth_t ident(eth_t x)                          \
  {                                             \
    return eth_create_variant(ident##_type, x); \
  }

#define eth_use_variant(ident) eth_use_variant_as(ident, #ident)

#define eth_use_tuple_as(ident, n) \
  static eth_type *ident;          \
  if (eth_unlikely(ident == NULL)) \
    ident = eth_tuple_type(n);     \

eth_t
eth_system_error(int err);

eth_t
eth_type_error();

eth_t
eth_invalid_argument();

eth_t
eth_failure();

#ifdef __cplusplus
} /* extern "C" */

/* Restore 'try', 'catch' and 'throw'. */
# undef try
# undef catch
# undef throw
#endif

#endif
