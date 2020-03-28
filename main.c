#include "ether/ether.h"

#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

ETH_MODULE("ether:main")

static eth_t
_car(void)
{
  eth_t x = *eth_sp++;
  if (x->type != eth_pair_type)
  {
    eth_drop(x);
    return eth_exn(eth_str("type-error"));
  }
  eth_t ret = eth_car(x);
  eth_ref(ret);
  eth_drop(x);
  eth_dec(ret);
  return ret;
}

static eth_t
_cdr(void)
{
  eth_t x = *eth_sp++;
  if (x->type != eth_pair_type)
  {
    eth_drop(x);
    return eth_exn(eth_str("type-error"));
  }
  eth_t ret = eth_cdr(x);
  eth_ref(ret);
  eth_drop(x);
  eth_dec(ret);
  return ret;
}

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
    return eth_exn(eth_str("type-error"));
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
        return eth_exn(eth_str("format-error"));
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

void __attribute__((noreturn))
help_and_exit(char *argv0)
{
  printf("usage: %s [OPTIONS] <script>\n", argv0);
  puts("");
  puts("OPTIONS:");
  puts("  --help       -h          Show this message and exit.");
  puts("  --bytecode               Dump bytecode.");
  puts("  --log-level     <level>  Set log-level. Available values for <level> are:");
  puts("                           'debug'   - enable all log-messages");
  puts("                           'warning' - show warnings and errors");
  puts("                           'error'   - show only error messages");
  exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
  struct option longopts[] = {
    { "help", false, NULL, 'h' },
    { "bytecode", false, NULL, 0x1FF },
    { "log-level", true, NULL, 0x2FF },
    { 0, 0, 0, 0 }
  };
  bool showbc = false;
  int opt;
  opterr = 0;
  while ((opt = getopt_long(argc, argv, "+h", longopts, NULL)) > 0)
  {
    switch (opt)
    {
      case 'h':
        help_and_exit(argv[0]);

      case 0x1FF:
        showbc = true;
        break;

      case 0x2FF:
        if (strcmp(optarg, "debug") == 0)
          eth_log_level = ETH_LOG_DEBUG;
        else if (strcmp(optarg, "warning") == 0)
          eth_log_level = ETH_LOG_WARNING;
        else if (strcmp(optarg, "error") == 0)
          eth_log_level = ETH_LOG_ERROR;
        else
        {
          eth_error("invalid log-level argument");
          eth_error("see `%s --help` for available arguments", argv[0]);
          exit(EXIT_FAILURE);
        }
        break;

      case '?':
        eth_error("unrecognized option '%s'", argv[optind - 1]);
        eth_error("see `%s --help` for available options", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  FILE *input = stdin;
  if (optind != argc)
  {
    char *path = argv[optind];
    input = fopen(path, "r");
    if (input == NULL)
    {
      eth_error("failed to open file \"%s\", %s", path, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  else
    eth_debug("reading script from standard input");

  eth_debug("init");
  eth_init();

  eth_debug("parse input");
  eth_ast *ast;
  ast = eth_parse(input);
  if (input != stdin)
    fclose(input);

  if (ast)
  {
    eth_module *mod = eth_create_module("");
    eth_define(mod,     "car", eth_create_proc(    _car, 1, NULL, NULL));
    eth_define(mod,     "cdr", eth_create_proc(    _cdr, 1, NULL, NULL));
    eth_define(mod,   "write", eth_create_proc(  _write, 1, NULL, NULL));
    eth_define(mod, "display", eth_create_proc(_display, 1, NULL, NULL));
    eth_define(mod, "newline", eth_create_proc(_newline, 0, NULL, NULL));
    eth_define(mod,  "printf", eth_create_proc( _printf, 1, NULL, NULL));

    eth_env *env = eth_create_env();
    eth_add_module_path(env, ".");

    eth_add_module(env, mod);
    /*eth_load_module_from_script(env, "../module.eth", NULL);*/


    eth_debug("build IR");
    eth_ir *ir = eth_build_ir(ast, env, NULL);
    eth_drop_ast(ast);
    if (ir)
    {
      eth_debug("build SSA");
      eth_ssa *ssa = eth_build_ssa(ir, NULL);
      eth_drop_ir(ir);
      if (ssa)
      {
        if (showbc)
        {
          fprintf(stderr, "---------- SSA bytecode ----------\n");
          eth_dump_ssa(ssa->body, stderr);
          fprintf(stderr, "----------------------------------\n");
        }

        eth_debug("build bytecode");
        eth_bytecode *bc = eth_build_bytecode(ssa);
        eth_destroy_ssa(ssa);
        if (bc)
        {
          eth_debug("run VM");
          eth_t ret = eth_vm(bc);
          if (ret->type == eth_exception_type)
            eth_error("unhandled exception: ~w", ETH_EXCEPTION(ret)->what);
          /*eth_printf("> ~w\n", ret);*/
          eth_drop(ret);
          eth_drop_bytecode(bc);
        }
        else
          eth_error("failed to build bytecode");

      }
      else
        eth_error("failed to build SSA");
    }
    else
      eth_error("failed to build IR");

    eth_destroy_env(env);
  }
  else
    eth_error("failed to build AST");


  eth_debug("cleanup");
  eth_cleanup();

  exit(EXIT_SUCCESS);
}
