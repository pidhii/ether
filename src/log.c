#include "ether/ether.h"

#include <stdio.h>
#include <stdarg.h>

enum eth_log_level eth_log_level = ETH_LOG_WARNING;

void
eth_log_aux(bool enable, const char *module, const char *file, const char *func,
    int line, const char *style, FILE *os, const char *fmt, ...)
{
  if (not enable) return;

  fprintf(os, "[%s %s \e[0m] ", style, module);
  /*fprintf(os, "[%s ether \e[0m] ", style);*/

  va_list arg;
  va_start(arg, fmt);
  eth_vfprintf(os, fmt, arg);
  va_end(arg);

  putc('\n', os);
}
