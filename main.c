#include "ether/ether.h"
#include "codeine/vec.h"

#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <readline/readline.h>
#include <readline/history.h>

ETH_MODULE("ether:main")

static void __attribute__((noreturn))
help_and_exit(char *argv0);

static void
print_version(FILE *out);

static eth_t
argv_to_list(int argc, char **argv, int offs);

static void
print_trace(eth_location *const trace[], int start, int n, int hi);

/*
 * This is where all definitions from within the REPL are going to.
 */
static
eth_module *repl_defs;

static
eth_env *repl_env;

static const eth_module*
resolve_ident(const char** ident);

static char**
completer(const char *text, int start, int end);

static void
repl_help(FILE *out);

static
bool repl_complete_empty = false;

static const char*
triml(const char *str)
{
  while (*str and isspace(*str))
    ++str;
  return str;
}

static char*
trimr(char *str)
{
  int i = strlen(str) - 1;
  for (; i >= 0 and isspace(str[i]); --i);
  str[i+1] = '\0';
  return str;
}

static void
less(const char *text)
{
  int size = strlen("LESS=-FX /bin/less -R <<ETHER_HELP\n")
           + strlen(text)
           + strlen("\nETHER_HELP");
  char buf[size+1];
  sprintf(buf, "LESS=-FX /bin/less -R <<ETHER_HELP\n%s\nETHER_HELP", text);
  system(buf);
}

int
main(int argc, char **argv)
{
  int err = EXIT_SUCCESS;

  struct option longopts[] = {
    { "help", false, NULL, 'h' },
    { "log-level", true, NULL, 0x2FF },
    { "version", false, NULL, 'v' },
    { "prefix", false, NULL, 0x3FF },
    { "trace-limit", true, NULL, 0x4FF },
    { 0, 0, 0, 0 }
  };
  int opt;
  cod_vec(char*) L;
  cod_vec_init(L);
  int tracelimh = 3;
  int tracelimt = 1;
  /*opterr = 0;*/
  while ((opt = getopt_long(argc, argv, "+hv:L:", longopts, NULL)) > 0)
  {
    switch (opt)
    {
      case 'h':
        help_and_exit(argv[0]);

      case 'v':
        print_version(stdout);
        exit(EXIT_SUCCESS);

      case 'L':
        cod_vec_push(L, optarg);
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

      case 0x4FF:
      {
        int h, t;
        if (sscanf(optarg, "%d,%d", &h, &t) == 2)
        {
          tracelimh = h;
          tracelimt = t;
        }
        else
        {
          tracelimh = atoi(optarg);
          tracelimt = 1;
        }
        break;
      }

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

  eth_debug("init");
  eth_init(&argc);

  eth_env *env = eth_create_env();
  cod_vec_iter(L, i, path, eth_add_module_path(env, path));
  // --
  eth_module *extravars = eth_create_module("<main>");
  eth_define(extravars, "command_line", argv_to_list(argc, argv, optind));
  eth_define(extravars, "__main", eth_true);

  if (input == stdin) // REPL
  {
    eth_debug("REPL");

    eth_module *mod = extravars;
    const char *prompt = "> ";
    char histpath[PATH_MAX];
    char *line;
    cod_vec(char) buf;

    sprintf(histpath, "%s/.ether_history", getenv("HOME"));
    cod_vec_init(buf);

    // set up autocomplete
    rl_attempted_completion_function = completer;
    repl_defs = mod;
    repl_env = env;

    // load previous history
    using_history();
    if (system("test -f ~/.ether_history") == 0)
      read_history(histpath);
    else
      system("touch ~/.ether_history");

    printf("Ether REPL\n");
    print_version(stdout);
    printf("\n");
    repl_help(stdout);
    printf("\n");

    while (true)
    {
      // read a line from the input
      if (not (line = readline(prompt)))
      {
        eth_printf("End of input reached.\n");
        cod_vec_destroy(buf);
        break;
      }

      // check if line is empty
      if (line[0] == 0)
      {
        free(line);
        continue;
      }
      // reset buffer
      else if (strcmp(line, ".") == 0)
      {
        buf.len = 0;
        free(line);
        prompt = "> ";
        continue;
      }
      // show help
      else if (strcmp(line, ".help") == 0)
      {
        repl_help(stdout);
        free(line);
        continue;
      }
      // show help for function
      else if (strncmp(line, ".help ", 6) == 0)
      {
        char *ident = (char*)triml(line + 5);
        trimr(ident);
        const char *orig = ident;
        const eth_module *mod = resolve_ident((const char**)&ident);
        bool try_help(const eth_module *mod)
        {
          eth_def *def = eth_find_def(mod, ident);
          if (def)
          {
            if (def->attr->help)
              // XXX: mutating help it here
              less(def->attr->help);
            else if (def->attr->loc)
              eth_print_location_opt(def->attr->loc, stdout,
                  ETH_LOPT_NOLINENO | ETH_LOPT_NOCOLOR);
            else
              eth_warning("no help available");
            return true;
          }
          else
            return false;
        }
        if (mod)
        {
          if (not (try_help(mod) or (ident==orig and try_help(eth_builtins()))))
            eth_warning("no such identifier");
        }
        free(line);
        continue;
      }
      // enable empty input completion
      else if (strcmp(line, ".complete-empty") == 0)
      {
        repl_complete_empty = true;
        free(line);
        continue;
      }
      // disable empty input completion
      else if (strcmp(line, ".no-complete-empty") == 0)
      {
        repl_complete_empty = false;
        free(line);
        continue;
      }
      // evaluate input
      else
      {
        // append line to input-buffer
        for (int i = 0; line[i]; ++i)
          cod_vec_push(buf, line[i]);
        free(line);

        // parse input-buffer
        FILE *bufstream = fmemopen(buf.data, buf.len, "r");
        eth_ast *expr = eth_parse_repl(bufstream);
        fclose(bufstream);

        if (expr)
        {
          // append history
          cod_vec_push(buf, 0);
          add_history(buf.data);

          if (append_history(1, histpath))
            eth_warning("failed to append history (%s)", strerror(errno));

          // reset buffer
          buf.len = 0;

          // evaluate expression
          eth_t ret = eth_eval(env, mod, expr);
          if (ret and ret != eth_nil)
          {
            eth_printf("~w\n", ret);
            eth_drop(ret);
          }
          eth_drop_ast(expr);

          prompt = "> ";
        }
        else
        {
          // continue reading
          cod_vec_push(buf, ' ');
          prompt = "@ ";
        }
      }
    }
  }
  else // Evaluate script
  {
    eth_debug("parse input");
    eth_ast *ast;
    ast = eth_parse(input);
    fclose(input);
    if (ast)
    {
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

                int tracelen = exn->tracelen;
                if (tracelimh + tracelimt >= tracelen)
                {
                  print_trace(exn->trace, tracelen - 1, tracelen, tracelen - 1);
                }
                else
                {
                  print_trace(exn->trace, tracelen - 1, tracelimh, tracelen - 1);
                  putc('\n', stderr);
                  eth_trace(" ... ");
                  print_trace(exn->trace, tracelimt - 1, tracelimt, -1);
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
          {
            eth_error("failed to build bytecode");
            err = EXIT_FAILURE;
          }

        }
        else
        {
          eth_error("failed to build SSA");
          err = EXIT_FAILURE;
        }
      }
      else
      {
        eth_error("failed to build IR");
        err = EXIT_FAILURE;
      }
    }
    else
    {
      eth_error("failed to build AST");
      err = EXIT_FAILURE;
    }
  }

  eth_destroy_env(env);
  eth_destroy_module(extravars);

  cod_vec_destroy(L);
  eth_debug("cleanup");
  eth_cleanup();

  exit(err);
}


static void __attribute__((noreturn))
help_and_exit(char *argv0)
{
  printf("usage: %s [OPTIONS]\n", argv0);
  puts("  Run interpreter in interactive mode (REPL).");
  puts("");
  printf("usage: %s [OPTIONS] <script> [argv ...]\n", argv0);
  puts("  Evaluate a script.");
  puts("");
  puts("OPTIONS:");
  puts("  --help         -h           Show this message and exit.");
  puts("  --log-level        <level>  Set log-level. Available values for <level> are:");
  puts("                                'debug'   - enable all log-messages;");
  puts("                                'warning' - show warnings and errors;");
  puts("                                'error'   - show only error messages.");
  puts("  --version      -v           Show version and configuration and exit.");
  puts("  --prefix                    Show installation prefix and exit.");
  puts("                 -L  <dir>    Add direcory to the module path.");
  puts("  --trace-limit      <value>  Specify depth of the trace to display. Formats for <value> are:");
  puts("                                <head>,<tail> - specify both head- and tail-length;");
  puts("                                <head>        - specify only head-length while tail-length is set to 1.");
  exit(EXIT_SUCCESS);
}

static void
print_version(FILE *out)
{
  fprintf(out, "version: %s\n", eth_get_version());
  fprintf(out, "build: %s\n", eth_get_build());
  fprintf(out, "build flags: %s\n", eth_get_build_flags());
  if (eth_get_prefix())
    fprintf(out, "prefix: %s\n", eth_get_prefix());
}

static eth_t
argv_to_list(int argc, char **argv, int offs)
{
  eth_t acc = eth_nil;
  for (int i = argc - 1; i >= offs; --i)
    acc = eth_cons(eth_str(argv[i]), acc);
  return acc;
}

static void
print_trace(eth_location *const trace[], int start, int n, int hi)
{
  char buf[PATH_MAX];
  for (int i = start; i >= start - n + 1; --i)
  {
    eth_get_location_file(trace[i], buf);
    putc('\n', stderr);
    eth_trace("trace[%d]: %s", i, buf);
    if (i == hi)
      eth_print_location_opt(trace[i], stderr, ETH_LOPT_EXTRALINES);
    else
      eth_print_location_opt(trace[i], stderr, 0);
  }
}

static const eth_module*
resolve_ident(const char** ident)
{
  const eth_module *mod = repl_defs;
  char *p;
  if ((p = strrchr(*ident, '.')))
  {
    int modnamelen = p - *ident;
    char modname[modnamelen + 1];
    memcpy(modname, *ident, modnamelen);
    modname[modnamelen] = '\0';
    mod = eth_require_module(repl_env, repl_env, modname);
    if (mod == NULL)
    {
      eth_warning("no module '%s'", modname);
      return NULL;
    }
    *ident = p + 1;
  }
  return mod;
}

static char*
completion_generator(const char *text, int state)
{
  static cod_vec(char*) matches;
  static size_t idx;
  static char prefix[PATH_MAX];

  // - - - - - - - - - -
  // Ignore empty input
  // - - - - - - - - - -
  if (text[0] == '\0' && not repl_complete_empty)
    return NULL;

  // - - - - - - - - - -
  // Initialize matches
  // - - - - - - - - - -
  if (state == 0)
  {
    // init vector
    if (matches.data)
    {
      cod_vec_iter(matches, i, x, free(x));
      matches.len = 0;
    }
    else
      cod_vec_init(matches);

    // resolve module
    const char *ident = text;
    const eth_module *mod = resolve_ident(&ident);
    if (mod == NULL)
      return NULL;
    strncpy(prefix, text, ident - text);

    // - - - - - - -
    // Get matches
    // - - - - - - -
    int identlen = strlen(ident);
    // check selected module
    int ndefs = eth_get_ndefs(mod);
    eth_def defs[ndefs];
    eth_get_defs(mod, defs);
    // wether we are matching some builtin value
    bool checkpriv = strncmp(ident, "__", 2) == 0;
    for (int i = 0; i < ndefs; ++i)
    {
      if (strncmp(defs[i].ident, ident, identlen) == 0)
      {
        if (strncmp(defs[i].ident, "__", 2) == 0 and not checkpriv)
          continue;
        char *ident = malloc(strlen(prefix) + strlen(defs[i].ident) + 2);
        sprintf(ident, "%s%s", prefix, defs[i].ident);
        cod_vec_push(matches, ident);
      }
    }
    // also check builtins (if no module prefix specified)
    if (strcmp(prefix, "") == 0)
    {
      mod = eth_builtins();
      int ndefs = eth_get_ndefs(mod);
      eth_def defs[ndefs];
      eth_get_defs(mod, defs);
      for (int i = 0; i < ndefs; ++i)
      {
        if (strncmp(defs[i].ident, ident, identlen) == 0)
        {
          if (strncmp(defs[i].ident, "__", 2) == 0 and not checkpriv)
            continue;
          cod_vec_push(matches, strdup(defs[i].ident));
        }
      }
    }

    // reset index
    idx = 0;
  }

  if (idx >= matches.len)
    return NULL;
  else
    return strdup(matches.data[idx++]);
}

static char**
completer(const char *text, int start, int end)
{
  rl_attempted_completion_over = 1;
  return rl_completion_matches(text, completion_generator);
}

static void
repl_help(FILE *out)
{
  fprintf(out, "Enter <C-d> (EOF) to exit\n");
  fprintf(out, "Commands:\n");
  fprintf(out, "  '.'                   to reset input buffer (cancel current expression)\n");
  fprintf(out, "  '.help'               show help and available commands\n");
  fprintf(out, "  '.help <ident>'       show help for given identifier\n");
  fprintf(out, "  '.complete-empty'     display all available identifiers when completing empty word\n");
  fprintf(out, "  '.no-complete-empty'  disable effect of the previous command\n");
}
