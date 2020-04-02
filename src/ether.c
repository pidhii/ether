#include "ether/ether.h"

#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#define STACK_SIZE 0x100000

ETH_MODULE("ether")

eth_t *eth_sp;

int eth_nargs;

eth_function *eth_this;

static
eth_t *g_stack;


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
eth_init(void)
{
  if (eth_get_prefix() == NULL)
  {
    eth_warning("can't determine installation prefix, "
                "may fail to resolve installed modules");
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  g_stack = malloc(sizeof(eth_t) * STACK_SIZE);
  eth_sp = g_stack + STACK_SIZE;

  extern void _eth_init_alloc(void);
  _eth_init_alloc();

  extern void _eth_init_magic(void);
  _eth_init_magic();

  extern void _eth_init_number_type(void);
  _eth_init_number_type();

  extern void _eth_init_function_type(void);
  _eth_init_function_type();

  extern void _eth_init_exception_type(void);
  _eth_init_exception_type();

  extern void _eth_init_string_type(void);
  _eth_init_string_type();

  extern void _eth_init_boolean_type(void);
  _eth_init_boolean_type();

  extern void _eth_init_pair_type(void);
  _eth_init_pair_type();

  extern void _eth_init_nil_type(void);
  _eth_init_nil_type();

  extern void _eth_init_symbol_type(void);
  _eth_init_symbol_type();

  extern void _eth_init_tuple_types(void);
  _eth_init_tuple_types();

  extern void _eth_init_builtins(void);
  _eth_init_builtins();
}

void
eth_cleanup(void)
{
  extern void _eth_cleanup_builtins(void);
  _eth_cleanup_builtins();

  eth_destroy_type(eth_number_type);
  eth_destroy_type(eth_function_type);
  eth_destroy_type(eth_exception_type);
  eth_destroy_type(eth_string_type);
  eth_destroy_type(eth_boolean_type);
  eth_destroy_type(eth_nil_type);
  eth_destroy_type(eth_pair_type);

  extern void _eth_cleanup_symbol_type(void);
  _eth_cleanup_symbol_type();

  extern void _eth_cleanup_tuple_types(void);
  _eth_cleanup_tuple_types();

  extern void _eth_cleanup_magic(void);
  _eth_cleanup_magic();

  extern void _eth_cleanup_alloc(void);
  _eth_cleanup_alloc();

  if (eth_sp - g_stack != STACK_SIZE)
    eth_warning("stack pointer is not on the top of the stack");
  free(g_stack);
}

const char*
eth_binop_sym(eth_binop op)
{
  static char sym[][5] = {
    "+", "-", "*", "/", "%", "^",
    "land", "lor", "lxor", "lshl", "lshr", "ashl", "ashr",
    "<", "<=", ">", ">=", "==", "/=",
    "is",
    "cons"
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
    "is",
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

