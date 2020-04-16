#include "ether/ether.h"

#include <stdio.h>
#include <stdarg.h>

enum eth_log_level eth_log_level = ETH_LOG_WARNING;

void
eth_log_aux(bool enable, const char *module, const char *file, const char *func,
    int line, const char *style, FILE *os, const char *fmt, ...)
{
  if (not enable) return;

  fflush(stdout);
  fflush(stderr);

#ifdef ETH_DEBUG_MODE
  fprintf(os, "[%s %s \e[0m] ", style, module);
#else
  fprintf(os, "[%s ether \e[0m] ", style);
#endif

  va_list arg;
  va_start(arg, fmt);
  eth_vfprintf(os, fmt, arg);
  va_end(arg);

  putc('\n', os);
}
