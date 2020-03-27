#include "ether/ether.h"

#include <stdlib.h>
#include <assert.h>

#define STACK_SIZE 0x100000

ETH_MODULE("ether")

eth_t *eth_sp;

int eth_nargs;

eth_function *eth_this;

static
eth_t *g_stack;

void
eth_init(void)
{
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
}

void
eth_cleanup(void)
{
  eth_destroy_type(eth_number_type);
  eth_destroy_type(eth_function_type);
  eth_destroy_type(eth_exception_type);
  eth_destroy_type(eth_string_type);
  eth_destroy_type(eth_boolean_type);
  eth_destroy_type(eth_nil_type);
  eth_destroy_type(eth_pair_type);

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
    "+",
    "-",
    "*",
    "/",
    "<",
    "<=",
    ">",
    ">=",
    "==",
    "/=",
    "is",
    "cons"
  };
  return sym[op];
}

const char*
eth_binop_name(eth_binop op)
{
  static char sym[][5] = {
    "add",
    "sub",
    "mul",
    "div",
    "lt",
    "le",
    "gt",
    "ge",
    "eq",
    "ne",
    "is",
    "cons"
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

