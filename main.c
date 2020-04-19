#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>

ETH_MODULE("ether:main")

void __attribute__((noreturn))
help_and_exit(char *argv0)
{
  printf("usage: %s [OPTIONS] [<script>]\n", argv0);
  puts("");
  puts("OPTIONS:");
  puts("  --help       -h          Show this message and exit.");
  puts("  --bytecode               Dump bytecode.");
  puts("  --log-level     <level>  Set log-level. Available values for <level> are:");
  puts("                           'debug'   - enable all log-messages;");
  puts("                           'warning' - show warnings and errors;");
  puts("                           'error'   - show only error messages.");
  puts("  --version    -v          Show version and configuration and exit.");
  puts("  --prefix                 Show installation prefix and exit.");
  puts("               -L <dir>    Add direcory to the module path.");
  exit(EXIT_SUCCESS);
}

static eth_t
argv_to_list(int argc, char **argv, int offs)
{
  eth_t acc = eth_nil;
  for (int i = argc - 1; i >= offs; --i)
    acc = eth_cons(eth_str(argv[i]), acc);
  return acc;
}

int
main(int argc, char **argv)
{
  int err = EXIT_SUCCESS;

  struct option longopts[] = {
    { "help", false, NULL, 'h' },
    { "bytecode", false, NULL, 0x1FF },
    { "log-level", true, NULL, 0x2FF },
    { "version", false, NULL, 'v' },
    { "prefix", false, NULL, 0x3FF },
    { 0, 0, 0, 0 }
  };
  bool showbc = false;
  int opt;
  cod_vec(char*) L;
  cod_vec_init(L);
  /*opterr = 0;*/
  while ((opt = getopt_long(argc, argv, "+hv:L:", longopts, NULL)) > 0)
  {
    switch (opt)
    {
      case 'h':
        help_and_exit(argv[0]);

      case 'v':
        printf("version: %s\n", eth_get_version());
        printf("build: %s\n", eth_get_build());
        printf("build flags: %s\n", eth_get_build_flags());
        if (eth_get_prefix())
          printf("prefix: %s\n", eth_get_prefix());
        exit(EXIT_SUCCESS);

      case 'L':
        cod_vec_push(L, optarg);
        break;

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

      case 0x3FF:
        if (eth_get_prefix())
        {
          puts(eth_get_prefix());
          exit(EXIT_SUCCESS);
        }
        else
          exit(EXIT_FAILURE);
        break;

      case '?':
        /*eth_error("unrecognized option '%s'", optopt);*/
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
      cod_vec_destroy(L);
      exit(EXIT_FAILURE);
    }
  }
  else
    eth_debug("reading script from standard input");

  eth_debug("init");
  eth_init(&argc);

  eth_debug("parse input");
  eth_ast *ast;
  ast = eth_parse(input);
  if (input != stdin)
    fclose(input);

  if (ast)
  {
    eth_env *env = eth_create_env();
    cod_vec_iter(L, i, path, eth_add_module_path(env, path));
    // --
    eth_module *extravars = eth_create_module("");
    eth_define(extravars, "command_line", argv_to_list(argc, argv, optind));
    // --
    eth_debug("build IR");
    eth_ir *ir = eth_build_ir(ast, env, extravars);
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
        eth_drop_ssa(ssa);
        if (bc)
        {
          eth_debug("run VM");

          struct timespec t1, t2;
          clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
          eth_t ret = eth_vm(bc);
          clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t2);

          if (eth_is_exn(ret))
          {
            eth_t what = eth_what(ret);
            if (what->type == eth_exit_type)
              err = eth_get_exit_status(what);
            else
            {
              err = EXIT_FAILURE;
              eth_exception *exn = ETH_EXCEPTION(ret);
              eth_error("unhandled exception: ~w", what);
              char buf[PATH_MAX];
              for (int i = exn->tracelen - 1; i >= 0; --i)
              {
                eth_get_location_file(exn->trace[i], buf);
                eth_trace("trace[%d]: %s", exn->tracelen - i - 1, buf);
                if (i == exn->tracelen - 1)
                  eth_print_location_opt(exn->trace[i], stderr, ETH_LOPT_EXTRALINES);
                else
                  eth_print_location_opt(exn->trace[i], stderr, 0);
              }
            }
          }

          eth_drop(ret);
          eth_drop_bytecode(bc);

          double t1sec = t1.tv_sec + t1.tv_nsec * 1e-9;
          double t2sec = t2.tv_sec + t2.tv_nsec * 1e-9;
          double dtsec = t2sec - t1sec;
          eth_debug("evaluation time: %f [cpu sec.]", dtsec);
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
    eth_destroy_module(extravars);
  }
  else
    eth_error("failed to build AST");


  cod_vec_destroy(L);
  eth_debug("cleanup");
  eth_cleanup();

  exit(err);
}
