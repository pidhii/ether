#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>


ETH_MODULE("ether:builtins")


static eth_t
_strcat(void)
{
  eth_args args = eth_start(2);
  eth_t x = eth_arg2(args, eth_string_type);
  eth_t y = eth_arg2(args, eth_string_type);

  int xlen = eth_str_len(x);
  int ylen = eth_str_len(y);
  const char *xstr = eth_str_cstr(x);
  const char *ystr = eth_str_cstr(y);

  char *str = malloc(xlen + ylen + 1);
  memcpy(str, xstr, xlen);
  memcpy(str + xlen, ystr, ylen + 1);

  eth_end_unref(args);
  return eth_create_string_from_ptr2(str, xlen + ylen);
}

static eth_t
_pair_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_pair_type);
  eth_drop(x);
  return ret;
}

static eth_t
_symbol_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_symbol_type);
  eth_drop(x);
  return ret;
}

static eth_t
_number_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_number_type);
  eth_drop(x);
  return ret;
}

static eth_t
_string_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_string_type);
  eth_drop(x);
  return ret;
}

static eth_t
_boolean_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_boolean_type);
  eth_drop(x);
  return ret;
}

static eth_t
_function_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(x->type == eth_function_type);
  eth_drop(x);
  return ret;
}

static eth_t
_tuple_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(eth_is_tuple(x->type));
  eth_drop(x);
  return ret;
}

static eth_t
_record_p(void)
{
  eth_t x = *eth_sp++;
  eth_t ret = eth_boolean(eth_is_record(x->type));
  eth_drop(x);
  return ret;
}

static eth_t
_tonumber(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }

  char *nptr = eth_str_cstr(x);
  char *endptr;
  eth_number_t ret = eth_strtonum(nptr, &endptr);

  if (endptr == nptr)
  {
    eth_drop(x);
    return eth_exn(eth_sym("Failure"));
  }

  while (*endptr++)
  {
    if (not isspace(*endptr))
    {
      eth_drop(x);
      return eth_exn(eth_sym("Failure"));
    }
  }

  eth_drop(x);
  return eth_num(ret);
}

static eth_t
_tosymbol(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(x->type != eth_string_type))
  {
    eth_drop(x);
    return eth_exn(eth_sym("Type_error"));
  }

  eth_t ret = eth_sym(eth_str_cstr(x));
  eth_drop(x);
  return ret;
}

static eth_t
_list(void)
{
  eth_t x = *eth_sp++;
  if (x == eth_nil || x->type == eth_pair_type)
  {
    return x;
  }
  else if (eth_is_tuple(x->type))
  {
    int n = eth_tuple_size(x->type);
    eth_t acc = eth_nil;
    for (int i = n - 1; i >= 0; --i)
      acc = eth_cons(eth_tup_get(x, i), acc);
    eth_drop(x);
    return acc;
  }
  else if (eth_is_record(x->type))
  {
    int n = eth_record_size(x->type);
    eth_t acc = eth_nil;
    for (int i = n - 1; i >= 0; --i)
    {
      eth_t key = eth_str(x->type->fields[i].name);
      eth_t val = eth_tup_get(x, i);
      acc = eth_cons(eth_tup2(key, val), acc);
    }
    eth_drop(x);
    return acc;
  }
  else
  {
    eth_drop(x);
    return eth_exn(eth_sym("Failure"));
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  lists
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_length(void)
{
  eth_t l = *eth_sp++;
  bool isproper;
  size_t len = eth_length(l, &isproper);
  eth_drop(l);
  if (isproper)
    return eth_num(len);
  else
    return eth_exn(eth_sym("Improper_list"));
}

static eth_t
_revappend(void)
{
  eth_t l = *eth_sp++;
  eth_ref(l);

  eth_t acc = *eth_sp++;

  eth_t it = l;
  for (; it->type == eth_pair_type; it = eth_cdr(it))
    acc = eth_cons(eth_car(it), acc);

  if (it != eth_nil)
  {
    eth_drop(acc);
    eth_unref(l);
    return eth_exn(eth_sym("Improper_list"));
  }
  eth_ref(acc);
  eth_unref(l);
  eth_dec(acc);
  return acc;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 basic I/O
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_write(void)
{
  eth_t x = *eth_sp++;
  eth_write(x, stdout);
  eth_drop(x);
  return eth_nil;
}

static eth_t
_display(void)
{
  eth_t x = *eth_sp++;
  eth_display(x, stdout);
  eth_drop(x);
  return eth_nil;
}

static eth_t
_newline(void)
{
  putchar('\n');
  return eth_nil;
}

static eth_t
_print(void)
{
  eth_t x = *eth_sp++;

  if (eth_is_tuple(x->type))
  {
    for (int i = 0; i < eth_tuple_size(x->type); ++i)
    {
      if (i > 0) putc('\t', stdout);
      eth_display(eth_tup_get(x, i), stdout);
    }
  }
  else
    eth_display(x, stdout);
  putc('\n', stdout);
  eth_drop(x);
  return eth_nil;
}

// TODO: print entered string when reading from terminal
static eth_t
_input(void)
{
  eth_t prompt = *eth_sp++;

  bool istty = isatty(STDIN_FILENO);

  // Try detect EOF before printing promt:
  if (not istty)
  {
    char c = getc(stdin);
    if (c == EOF)
    {
      eth_drop(prompt);
      return eth_exn(eth_sym("End_of_file"));
    } 
    ungetc(c, stdin);
  }

  eth_display(prompt, stdout);
  eth_drop(prompt);

  char *line = NULL;
  size_t n = 0;
  ssize_t nrd = getline(&line, &n, stdin);
  if (nrd < 0)
  {
    int err = errno;
    free(line);
    if (feof(stdin))
      return eth_exn(eth_sym("End_of_file"));
    else
    {
      switch (err)
      {
        case EINVAL: return eth_exn(eth_sym("IO_error"));
        case ENOMEM: return eth_exn(eth_sym("Out_of_memory"));
        default: abort();
      }
    }
  }

  // Show entered line when input is fed from pipe:
  if (not istty)
    fputs(line, stdout);

  // Remove trailing newline-symbol:
  int len = nrd;
  if (line[len - 1] == '\n')
  {
    len = nrd - 1;
    line[len] = 0;
  }

  return eth_create_string_from_ptr2(line, len);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  printf
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  eth_t fmt;
  int n;
} format_data;

static void
destroy_format_data(format_data *data)
{
  eth_unref(data->fmt);
  free(data);
}

static eth_t
_printf_aux(void)
{
  format_data *data = eth_this->proc.data;

  char *fmt = ETH_STRING(data->fmt)->cstr;
  int n = data->n;

  for (int i = 0; i < n; ++i)
    eth_ref(eth_sp[i]);

  int ipar = 0;
  for (char *p = fmt; *p; ++p)
  {
    switch (*p)
    {
      case '~':
        switch (p[1])
        {
          case 'w':
            eth_write(eth_sp[ipar], stdout);
            eth_unref(eth_sp[ipar]);
            ipar += 1;
            p += 1;
            break;

          case 'd':
            eth_display(eth_sp[ipar], stdout);
            eth_unref(eth_sp[ipar]);
            ipar += 1;
            p += 1;
            break;

          case '~':
            putc('~', stdout);
            p += 1;
            break;

          default:
            assert(!"wtf");
        }
        break;

      default:
        putc(*p, stdout);
    }
  }

  eth_pop_stack(n);
  return eth_nil;
}

static eth_t
_printf(void)
{
  eth_t fmt = *eth_sp++;
  if (fmt->type != eth_string_type)
  {
    eth_drop(fmt);
    return eth_exn(eth_sym("Type_error"));
  }

  int n = 0;
  char *p = ETH_STRING(fmt)->cstr;
  while (true)
  {
    if ((p = strchr(p, '~')))
    {
      if (p[1] == 'w' || p[1] == 'd')
      {
        n += 1;
        p += 2;
      }
      else if (p[1] == '~')
      {
        p += 2;
      }
      else
      {
        eth_drop(fmt);
        return eth_exn(eth_sym("Format_error"));
      }
      continue;
    }
    break;
  }

  if (n == 0)
  {
    fputs(ETH_STRING(fmt)->cstr, stdout);
    eth_drop(fmt);
    return eth_nil;
  }
  else
  {
    format_data *data = malloc(sizeof(format_data));
    eth_ref(data->fmt = fmt);
    data->n = n;
    return eth_create_proc(_printf_aux, n, data, (void*)destroy_format_data);
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                exceptions
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_raise(void)
{
  return eth_exn(*eth_sp++);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                 module
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static
eth_env *g_env;

static
eth_module *g_mod;

/*
 * NOTE: IR-builder will use builtins ...while loading builtins.
 */
void
_eth_init_builtins(void)
{
  g_env = eth_create_empty_env();
  g_mod = eth_create_module("Builtins");

  eth_debug("loading builtins");

  eth_define(g_mod,        "++", eth_create_proc(    _strcat, 2, NULL, NULL));
  // ---
  eth_define(g_mod,     "pair?", eth_create_proc(    _pair_p, 1, NULL, NULL));
  eth_define(g_mod,   "symbol?", eth_create_proc(  _symbol_p, 1, NULL, NULL));
  eth_define(g_mod,   "number?", eth_create_proc(  _number_p, 1, NULL, NULL));
  eth_define(g_mod,   "string?", eth_create_proc(  _string_p, 1, NULL, NULL));
  eth_define(g_mod,  "boolean?", eth_create_proc( _boolean_p, 1, NULL, NULL));
  eth_define(g_mod, "function?", eth_create_proc(_function_p, 1, NULL, NULL));
  eth_define(g_mod,    "tuple?", eth_create_proc(   _tuple_p, 1, NULL, NULL));
  eth_define(g_mod,   "record?", eth_create_proc(  _record_p, 1, NULL, NULL));
  eth_define(g_mod,  "tonumber", eth_create_proc(  _tonumber, 1, NULL, NULL));
  eth_define(g_mod,  "tosymbol", eth_create_proc(  _tosymbol, 1, NULL, NULL));
  eth_define(g_mod,      "list", eth_create_proc(      _list, 1, NULL, NULL));
  // ---
  eth_define(g_mod,    "length", eth_create_proc(    _length, 1, NULL, NULL));
  eth_define(g_mod, "revappend", eth_create_proc( _revappend, 2, NULL, NULL));
  // ---
  eth_define(g_mod,     "write", eth_create_proc(     _write, 1, NULL, NULL));
  eth_define(g_mod,   "display", eth_create_proc(   _display, 1, NULL, NULL));
  eth_define(g_mod,   "newline", eth_create_proc(   _newline, 0, NULL, NULL));
  eth_define(g_mod,     "print", eth_create_proc(     _print, 1, NULL, NULL));
  eth_define(g_mod,     "input", eth_create_proc(     _input, 1, NULL, NULL));
  // ---
  eth_define(g_mod,    "printf", eth_create_proc(    _printf, 1, NULL, NULL));
  // ---
  eth_define(g_mod,     "raise", eth_create_proc(     _raise, 1, NULL, NULL));

  eth_debug("loading \"src/builtins.eth\"");
  if (not eth_load_module_from_script(g_env, g_mod, "src/builtins.eth"))
  {
    eth_error("failed to load builtins");
    abort();
  }
  eth_debug("builtins were succesfully loaded");
}

void
_eth_cleanup_builtins(void)
{
  eth_destroy_env(g_env);
}

const eth_module*
eth_builtins(void)
{
  return g_mod;
}

eth_t
eth_get_builtin(const char *name)
{
  eth_def *def = eth_find_def(g_mod, name);
  if (def)
    return def->val;
  else
    return NULL;
}

