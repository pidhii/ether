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

#include <stdio.h>
#include <stdarg.h>

enum eth_log_level eth_log_level = ETH_LOG_WARNING;

static
int g_indent = 0;

void
eth_indent_log(void)
{
  g_indent += 2;
}

void
eth_dedent_log(void)
{
  g_indent -= 2;
}

void
eth_log_aux(bool enable, const char *module, const char *file, const char *func,
    int line, const char *style, FILE *os, const char *fmt, ...)
{
  if (not enable) return;

  fflush(stdout);
  fflush(stderr);

#ifdef ETH_DEBUG_MODE
  fprintf(os, "[%s %s \e[0m] at %s:%d (in %s()): ", style, module, file, line, func);
#else
  fprintf(os, "[%s ether \e[0m] ", style);
#endif

  for (int i = 0; i < g_indent; ++i)
  {
    if (i % 2 == 0)
      fputs("¦", os);
    else
      fputc(' ', os);
  }

  va_list arg;
  va_start(arg, fmt);
  eth_vfprintf(os, fmt, arg);
  if (va_arg(arg, int))
    putc('\n', os);
  va_end(arg);

}
