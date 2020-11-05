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
#include <string.h>
#include <stdarg.h>
#include <assert.h>

void
eth_vfprintf(FILE *out, const char *fmt, va_list arg)
{
  if (*fmt == 0) return;

  char *p = strchr(fmt, '~');
  if (p && (p[1] == 'w' || p[1] == 'd'))
  {
    if (p != fmt)
    {
      int n = p - fmt;
      char buf[n + 1];
      memcpy(buf, fmt, n);
      buf[n] = 0;
      vfprintf(out, buf, arg);
    }

    eth_t x = va_arg(arg, eth_t);
    switch (p[1])
    {
      case 'w':
        eth_write(x, out);
        break;

      case 'd':
        eth_display(x, out);
        break;
    }

    eth_vfprintf(out, p + 2, arg);
  }
  else
    vfprintf(out, fmt, arg);
}

void
eth_vprintf(const char *fmt, va_list arg)
{
  eth_vfprintf(stdout, fmt, arg);
}

void
eth_fprintf(FILE *out, const char *fmt, ...)
{
  va_list arg;
  va_start(arg, fmt);
  eth_vfprintf(out, fmt, arg);
  va_end(arg);
}

void
eth_printf(const char *fmt, ...)
{
  va_list arg;
  va_start(arg, fmt);
  eth_vprintf(fmt, arg);
  va_end(arg);
}

