#include "ether/ether.h"

#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <sys/resource.h>

#define STACK_SIZE 0x1000

ETH_MODULE("ether")

int eth_nargs = 0;
eth_function *eth_this;
eth_t *eth_stack, *eth_reg_stack;
eth_t *eth_sp, *eth_reg_sp;

ssize_t eth_c_stack_size;
char *eth_c_stack_start;


const char*
eth_get_prefix(void)
{
  static char buf[PATH_MAX];
  static bool first = true;
  static char *prefix = NULL;

  if (first)
  {
    char *path = getenv("ETHER_PATH");
    if (path)
    {
      sprintf(buf, path);
    }
    else
    {
      FILE *in = popen("pkg-config ether --variable=prefix", "r");
      if (in)
      {
        char *line = NULL;
        size_t n = 0;
        getline(&line, &n, in);
        if (pclose(in) == 0)
        {
          assert(n > 1);
          line[n - 2] = 0; // remove newline
          memcpy(buf, line, n);
          prefix = buf;
        }
        free(line);
      }
    }

#ifdef ETHER_PREFIX
    if (prefix == NULL)
    {
      sprintf(buf, ETHER_PREFIX);
      prefix = buf;
    }
#endif

    first = false;
  }

  return prefix;
}

#ifndef ETHER_VERSION
# define ETHER_VERSION "<undefined>"
#endif

const char*
eth_get_version(void)
{
  return ETHER_VERSION;
}

#ifndef ETHER_BUILD
# define ETHER_BUILD "<undefined>"
#endif

const char*
eth_get_build(void)
{
  return ETHER_BUILD;
}

#ifndef ETHER_BUILD_FLAGS
# define ETHER_BUILD_FLAGS "<undefined>"
#endif

const char*
eth_get_build_flags(void)
{
  return ETHER_BUILD_FLAGS;
}


static
uint8_t g_siphash_key[16] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, 14, 15, 16
};

const uint8_t*
eth_get_siphash_key(void)
{
  return g_siphash_key;
}



void
eth_init(const int *argc)
{
  if (eth_get_prefix() == NULL)
  {
    eth_warning("can't determine installation prefix, "
                "may fail to resolve installed modules");
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  eth_stack = malloc(sizeof(eth_t) * STACK_SIZE);
  eth_sp = eth_stack + STACK_SIZE;

  struct rlimit limit;
  getrlimit(RLIMIT_STACK, &limit);
  eth_c_stack_size = limit.rlim_cur / 2; // ...I don't know...
  eth_c_stack_start = (char*)argc;

  extern void _eth_init_alloc(void);
  _eth_init_alloc();

  extern void _eth_init_magic(void);
  _eth_init_magic();

  extern void _eth_init_number_type(void);
  _eth_init_number_type();

  extern void _eth_init_strings(void);
  _eth_init_strings();

  extern void _eth_init_regexp_type(void);
  _eth_init_regexp_type();

  extern void _eth_init_boolean_type(void);
  _eth_init_boolean_type();

  extern void _eth_init_nil_type(void);
  _eth_init_nil_type();

  extern void _eth_init_symbol_type(void);
  _eth_init_symbol_type();

  extern void _eth_init_pair_type(void);
  _eth_init_pair_type();

  extern void _eth_init_function_type(void);
  _eth_init_function_type();

  extern void _eth_init_exception_type(void);
  _eth_init_exception_type();

  extern void _eth_init_exit_type(void);
  _eth_init_exit_type();

  extern void _eth_init_record_types(void);
  _eth_init_record_types();

  extern void _eth_init_variant_types(void);
  _eth_init_variant_types();

  extern void _eth_init_file_type(void);
  _eth_init_file_type();

  extern void _eth_init_range_types(void);
  _eth_init_range_types();

  extern void _eth_init_ref_type(void);
  _eth_init_ref_type();

  extern void _eth_init_vector_type(void);
  _eth_init_vector_type();

  extern void _eth_init_builtins(void);
  _eth_init_builtins();
}

void
eth_cleanup(void)
{
  extern void _eth_cleanup_builtins(void);
  _eth_cleanup_builtins();

  eth_destroy_type(eth_number_type);
  eth_destroy_type(eth_boolean_type);
  eth_destroy_type(eth_nil_type);
  eth_destroy_type(eth_pair_type);
  eth_destroy_type(eth_function_type);
  eth_destroy_type(eth_exception_type);
  eth_destroy_type(eth_exit_type);
  eth_destroy_type(eth_file_type);
  eth_destroy_type(eth_rangelr_type);
  eth_destroy_type(eth_rangel_type);
  eth_destroy_type(eth_ranger_type);
  eth_destroy_type(eth_regexp_type);
  eth_destroy_type(eth_strong_ref_type);
  eth_destroy_type(eth_weak_ref_type);
  eth_destroy_type(eth_vector_type);

  extern void _eth_cleanup_strings(void);
  _eth_cleanup_strings();

  extern void _eth_cleanup_symbol_type(void);
  _eth_cleanup_symbol_type();

  extern void _eth_cleanup_record_types(void);
  _eth_cleanup_record_types();

  extern void _eth_cleanup_variant_types(void);
  _eth_cleanup_variant_types();

  extern void _eth_cleanup_magic(void);
  _eth_cleanup_magic();

  extern void _eth_cleanup_alloc(void);
  _eth_cleanup_alloc();

  if (eth_sp - eth_stack != STACK_SIZE)
    eth_warning("stack pointer is not on the top of the stack");
  free(eth_stack);
  free(eth_reg_stack);
}

const char*
eth_binop_sym(eth_binop op)
{
  static char sym[][5] = {
    "+", "-", "*", "/", "mod", "^",
    "land", "lor", "lxor", "lshl", "lshr", "ashl", "ashr",
    "<", "<=", ">", ">=", "==", "/=",
    "is", "eq",
    "::"
  };
  return sym[op];
}

const char*
eth_binop_name(eth_binop op)
{
  static char sym[][5] = {
    "add", "sub", "mul", "div", "mod", "pow",
    "land", "lor", "lxor", "lshl", "lshr", "ashl", "ashr",
    "lt", "le", "gt", "ge", "eq", "ne",
    "is", "eq",
    "cons"
  };
  return sym[op];
}

const char*
eth_unop_sym(eth_unop op)
{
  static char sym[][5] = {
    "not",
    "lnot",
  };
  return sym[op];
}

const char*
eth_unop_name(eth_unop op)
{
  static char sym[][5] = {
    "not",
    "lnot",
  };
  return sym[op];
}


eth_magic*
eth_require_magic(eth_t x)
{
  if (x->magic == ETH_MAGIC_NONE)
  {
    eth_magic *magic = eth_create_magic();
    x->magic = eth_get_magic_id(magic);
    eth_ref_magic(magic);
    return magic;
  }
  else
  {
    return eth_get_magic(x->magic);
  }
}

void
_eth_delete_magic(eth_t x)
{
  eth_magic *magic = eth_get_magic(x->magic);

  // drop out from each scope
  int nscp;
  eth_scp *const *scps = eth_get_scopes(magic, &nscp);
  if (nscp > 0)
  {
    for (int i = 0; i < nscp; ++i)
      eth_drop_out(scps[i]);
  }
  else
  {
    // evaluate destructor
    eth_force_delete(x);
    // release magic
    eth_unref_magic(magic);
  }
}

eth_t
eth_system_error(int err)
{
  eth_use_variant(System_error)
  return System_error(eth_sym(eth_errno_to_str(err)));
}

eth_t
eth_type_error(void)
{
  eth_use_symbol(Type_error)
  return Type_error;
}

eth_t
eth_invalid_argument(void)
{
  eth_use_symbol(Invalid_argument)
  return Invalid_argument;
}

eth_t
eth_failure(void)
{
  eth_use_symbol(Failure)
  return Failure;
}

