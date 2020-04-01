#include "ether/ether.h"

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
  printf("usage: %s [OPTIONS] <script>\n", argv0);
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
  exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
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
  /*opterr = 0;*/
  while ((opt = getopt_long(argc, argv, "+hv", longopts, NULL)) > 0)
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

    eth_env *env = eth_create_env();

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

          struct timespec t1, t2;
          clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
          eth_t ret = eth_vm(bc);
          clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t2);

          if (ret->type == eth_exception_type)
            eth_error("unhandled exception: ~w", ETH_EXCEPTION(ret)->what);

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
  }
  else
    eth_error("failed to build AST");


  eth_debug("cleanup");
  eth_cleanup();

  exit(EXIT_SUCCESS);
}
