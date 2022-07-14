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
#include "codeine/vec.h"

#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#include <readline/readline.h>
#include <readline/history.h>

ETH_MODULE("ether:main")

static void __attribute__((noreturn))
help_and_exit(char *argv0);

static void
print_GPL(void);

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
eth_root *repl_root;

static eth_t
resolve_ident(const eth_module *root, const char *ident);

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

static char*
extract_ident_module_path(const char *ident, char *modpath, char *varname)
{
  char *p;
  if ((p = strrchr(ident, '.')))
  {
    int modpathlen = p - ident;
    strncpy(modpath, ident, modpathlen);
    modpath[modpathlen] = 0;
    strcpy(varname, p + 1);
    return modpath;
  }
  else
    return NULL;
}

static void
less(const char *text)
{
  int size = strlen("LESS=-FX /bin/less -R << 'ETHER_HELP'\n")
           + strlen(text)
           + strlen("\nETHER_HELP");
  char buf[size+1];
  sprintf(buf, "LESS=-FX /bin/less -R << 'ETHER_HELP'\n%s\nETHER_HELP", text);
  system(buf);
}

static void
asciidoc(const char *text)
{
  int size = strlen("cat << 'ETHER_HELP' | asciidoc -o - - | w3m -T text/html\n")
           + strlen(text)
           + strlen("\nETHER_HELP");
  char buf[size+1];
  sprintf(buf, "cat << 'ETHER_HELP' | asciidoc -o - - | w3m -T text/html\n%s\nETHER_HELP", text);
  system(buf);
}


static void
asciidoc_less(const char *text)
{
  int size = strlen("cat << 'ETHER_HELP' | asciidoc -o - - | w3m -T text/html -dump -cols 80 | LESS=-FX /bin/less\n")
           + strlen(text)
           + strlen("\nETHER_HELP");
  char buf[size+1];
  sprintf(buf, "cat << 'ETHER_HELP' | asciidoc -o - - | w3m -T text/html -dump -cols 80 | LESS=-FX /bin/less\n%s\nETHER_HELP", text);
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
    { "module-path", false, NULL, 0x5FF },
    { "batch-mode", false, NULL, 'b' },
    { 0, 0, 0, 0 }
  };
  int opt;
  cod_vec(char*) L;
  cod_vec_init(L);
  int tracelimh = 3;
  int tracelimt = 1;
  bool batchmode = false;
  /*opterr = 0;*/
  while ((opt = getopt_long(argc, argv, "+hvL:b", longopts, NULL)) > 0)
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

      case 'b':
        batchmode = true;
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

      case 0x5FF:
        if (eth_get_module_path())
        {
          puts(eth_get_module_path());
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

  char *filepath = NULL;
  FILE *input = stdin;
  if (optind != argc)
  {
    filepath = argv[optind];
    input = fopen(filepath, "r");
    if (input == NULL)
    {
      eth_error("failed to open file \"%s\", %s", filepath, strerror(errno));
      cod_vec_destroy(L);
      exit(EXIT_FAILURE);
    }
  }

  eth_debug("[main] init");
  eth_init(&argc);

  eth_root *root = eth_create_root();
  cod_vec_iter(L, i, path, eth_add_module_path(eth_get_root_env(root), path));
  // --
  const char *modpath;
  if (filepath)
  {
    static char filedir[PATH_MAX];
    strcpy(filedir, filepath);
    modpath = dirname(filedir);
  }
  else
    modpath = ".";
  eth_module *extravars = eth_create_module("<main>", modpath);
  eth_define(extravars, "command_line", argv_to_list(argc, argv, optind));
  eth_define(extravars, "__main", eth_true);

  if (input == stdin and not batchmode) // REPL
  {
    eth_debug("[main] REPL");

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
    repl_root = root;

    eth_evaluator evl;
    evl.root = root;
    evl.mod = mod;

    // load previous history
    using_history();
    if (system("test -f ~/.ether_history") == 0)
      read_history(histpath);
    else
      system("touch ~/.ether_history");

    printf("ether %s\n", eth_get_version());
    print_GPL();
    printf("\n");
    repl_help(stdout);
    printf("\n");

    char stdmodpath[PATH_MAX];
    if (eth_resolve_path(eth_get_root_env(repl_root), "std.eth", stdmodpath))
    {
      eth_debug("loading 'std' (from \"%s\")", stdmodpath);
      eth_module *stdmod =
        eth_load_module_from_script(repl_root, stdmodpath, NULL);
      if (stdmod)
      {
        eth_copy_defs(stdmod, repl_defs);
        eth_destroy_module(stdmod);
        eth_debug("successfully loaded 'std'");
      }
      else
        eth_warning("failed to load std-library");
    }
    else
      eth_warning("can't find std-library");

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
      else if (strncmp(line, ".help ", 6) == 0 || strncmp(line, ".h ", 3) == 0)
      {
        char *ident = (char*)triml(strchr(line, ' '));
        trimr(ident);
        const char *orig = ident;

        char modpath[PATH_MAX];
        char varname[PATH_MAX];
        const eth_def *vardef = NULL;
        if (extract_ident_module_path(ident, modpath, varname))
        {
          eth_printf("module path: \"%s\"\n", modpath);
          const eth_module *mod =
            eth_require_module(repl_root, eth_get_root_env(repl_root), modpath);
          if (not mod)
          {
            eth_warning("failed to load module");
            free(line);
            continue;
          }
          vardef = eth_find_def(mod, varname);
        }
        else
          vardef = eth_find_def(repl_defs, ident);

        if (vardef)
        {
          if (vardef->attr->help)
          {
            // XXX: mutating help here
            // XXX: oh do I?
            if (strncmp(line, ".help ", 6) == 0)
              asciidoc(vardef->attr->help);
            else
              asciidoc_less(vardef->attr->help);
          }
          else if (vardef->attr->loc)
          {
            int opt = ETH_LOPT_NOLINENO | ETH_LOPT_NOCOLOR;
            eth_print_location_opt(vardef->attr->loc, stdout, opt);
          }
          else
            eth_warning("no help available");
        }
        else
          eth_warning("no such variable");

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
        eth_scanner *scan = eth_create_repl_scanner(root, bufstream);
        eth_ast *expr = eth_parse_repl(scan);
        eth_destroy_scanner(scan);
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
          eth_t ret = eth_eval(&evl, expr);
          if (ret)
          {
            if (eth_is_exn(ret))
              eth_printf("\e[38;5;1;1mexception:\e[0m (~w)\n", eth_what(ret));
            else if (ret != eth_nil)
              eth_printf("~w\n", ret);
            eth_drop(ret);
          }
          eth_drop_ast(expr);

          prompt = "> ";
        }
        else
        {
          // continue reading
          cod_vec_push(buf, '\n');
          prompt = "@ ";
        }
      }
    }
  }
  else // Evaluate script
  {
    eth_debug("[main] parse input");
    eth_ast *ast;
    ast = eth_parse(root, input);
    fclose(input);
    if (ast)
    {
      eth_debug("[main] build IR");
      eth_ir *ir = eth_build_ir(ast, root, extravars);
      eth_drop_ast(ast);
      if (ir)
      {
        eth_debug("[main] build SSA");
        eth_ssa *ssa = eth_build_ssa(ir, NULL);
        eth_drop_ir(ir);
        if (ssa)
        {
          eth_debug("[main] build bytecode");
          eth_bytecode *bc = eth_build_bytecode(ssa);
          eth_drop_ssa(ssa);
          if (bc)
          {
            eth_debug("[main] run VM");

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
            eth_debug("[main] evaluation time: %f [cpu sec.]", dtsec);
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

  eth_destroy_root(root);
  eth_destroy_module(extravars);

  cod_vec_destroy(L);
  eth_debug("[main] cleanup");
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
  puts("  --module-path               Print module-path in use and exit.");
  puts("  --batch-mode   -b           Execute code from standard input without entering REPL mode.");
  exit(EXIT_SUCCESS);
}

static void
print_GPL(void)
{
  printf("Copyright (C) 2020  Ivan Pidhurskyi\n");
  puts("");
  printf("This program is free software: you can redistribute it and/or modify\n");
  printf("it under the terms of the GNU General Public License as published by\n");
  printf("the Free Software Foundation, either version 3 of the License, or\n");
  printf("(at your option) any later version.\n");
  puts("");
  printf("This program is distributed in the hope that it will be useful,\n");
  printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
  printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
  printf("GNU General Public License for more details.\n");
  puts("");
  printf("You should have received a copy of the GNU General Public License\n");
  printf("along with this program.  If not, see <https://www.gnu.org/licenses/>.\n");
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

static eth_t
resolve_ident(const eth_module *root, const char *ident)
{
  char *p;
  if ((p = strchr(ident, '.')))
  {
    int prefixlen = p - ident;
    char prefix[prefixlen + 1];
    memcpy(prefix, ident, prefixlen);
    prefix[prefixlen] = '\0';
    ident += prefixlen + 1;
    eth_def *basedef = eth_find_def(root, prefix);
    if (basedef == NULL)
      return NULL;

    eth_t base = basedef->val;
    if (not eth_is_record(base->type))
      return NULL;

    while ((p = strchr(ident, '.')))
    {
      int prefixlen = p - ident;
      char prefix[prefixlen + 1];
      memcpy(prefix, ident, prefixlen);
      prefix[prefixlen] = '\0';
      ident += prefixlen + 1;
      eth_field* fld = eth_get_field(base->type, prefix);
      if (fld == NULL)
        return NULL;
      base = *(eth_t*)((char*)base + fld->offs);
    }

    return base;
  }

  return NULL;
}

static char*
completion_generator(const char *text, int state)
{
  static cod_vec(char*) matches;
  static size_t idx;
  static char prefix[PATH_MAX] = "";

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
    if (strchr(text, '.'))
    {
      eth_t base = resolve_ident(repl_defs, text);
      if (base == NULL)
        return NULL;

      const char *ident = strrchr(text, '.') + 1;

      int identlen = strlen(ident);
      int prefixlen = strlen(text) - strlen(ident);
      memcpy(prefix, text, prefixlen);
      prefix[prefixlen] = '\0';

      bool checkpriv = strncmp(ident, "__", 2) == 0;
      for (int i = 0; i < base->type->nfields; ++i)
      {
        if (strncmp(base->type->fields[i].name, ident, identlen) == 0)
        {
          if (strncmp(base->type->fields[i].name, "__", 2) == 0 and not checkpriv)
            continue;
          char *match = malloc(prefixlen + strlen(base->type->fields[i].name) + 1);
          sprintf(match, "%s%s", prefix, base->type->fields[i].name);
          cod_vec_push(matches, match);
        }
      }
    }
    else
    {
      const char *ident = text;
      int identlen = strlen(ident);

      int ndefs = eth_get_ndefs(repl_defs);
      eth_def defs[ndefs];
      eth_get_defs(repl_defs, defs);
      // wether we are matching some builtin value
      bool checkpriv = strncmp(ident, "__", 2) == 0;
      for (int i = 0; i < ndefs; ++i)
      {
        if (strncmp(defs[i].ident, ident, identlen) == 0)
        {
          if (strncmp(defs[i].ident, "__", 2) == 0 and not checkpriv)
            continue;
          char *ident = malloc(strlen(prefix) + strlen(defs[i].ident) + 2);
          sprintf(ident, "%s", defs[i].ident);
          cod_vec_push(matches, ident);
        }
      }

      // also check builtins
      {
        const eth_module *mod = eth_get_builtins(repl_root);
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

