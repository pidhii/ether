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
    FILE *in = popen("pkg-config ether --variable=prefix", "r");
    if (in)
    {
      char *line = NULL;
      size_t n = 0;
      ssize_t nrd = getline(&line, &n, in);
      if (pclose(in) == 0)
      {
        assert(nrd > 1);
        line[nrd - 1] = 0; // remove newline
        strcpy(buf, line);
        prefix = buf;
      }
      free(line);
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

const char*
eth_get_module_path(void)
{
  static char buf[PATH_MAX];
  static char *path;
  static bool first = true;
  if (first)
  {
    first = false;
    char *envpath = getenv("ETHER_PATH");
    if (envpath)
    {
      strcpy(buf, envpath);
      path = buf;
    }
    else if (eth_get_prefix())
    {
      sprintf(buf, "%s/lib/ether", eth_get_prefix());
      path = buf;
    }
    else
      path = NULL;
  }
  return path;
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
}

void
eth_cleanup(void)
{
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
  eth_destroy_type(eth_vector_type);

  extern void _eth_cleanup_strings(void);
  _eth_cleanup_strings();

  extern void _eth_cleanup_symbol_type(void);
  _eth_cleanup_symbol_type();

  extern void _eth_cleanup_record_types(void);
  _eth_cleanup_record_types();

  extern void _eth_cleanup_variant_types(void);
  _eth_cleanup_variant_types();

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


eth_t
eth_system_error(int err)
{
  eth_use_variant(system_error)
  return system_error(eth_sym(eth_errno_to_str(err)));
}

eth_t
eth_type_error(void)
{
  eth_use_symbol(type_error)
  return type_error;
}

eth_t
eth_invalid_argument(void)
{
  eth_use_symbol(invalid_argument)
  return invalid_argument;
}

eth_t
eth_failure(void)
{
  eth_use_symbol(failure)
  return failure;
}

