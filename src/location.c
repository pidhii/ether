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
#include <errno.h>
#include <libgen.h>
#include <limits.h>


ETH_MODULE("ether:location")


eth_location*
eth_create_location(int fl, int fc, int ll, int lc, const char *path)
{
  eth_location *loc = malloc(sizeof(eth_location));
  loc->rc = 0;
  loc->fl = fl;
  loc->fc = fc;
  loc->ll = ll;
  loc->lc = lc;
  loc->filepath = strdup(path);
  return loc;
}

static void
destroy_location(eth_location *loc)
{
  free(loc->filepath);
  free(loc);
}

void
eth_ref_location(eth_location *loc)
{
  loc->rc += 1;
}

void
eth_unref_location(eth_location *loc)
{
  if (--loc->rc == 0)
    destroy_location(loc);
}

void
eth_drop_location(eth_location *loc)
{
  if (loc->rc == 0)
    destroy_location(loc);
}

char*
eth_get_location_file(eth_location *loc, char *out)
{
  char buf[PATH_MAX];
  strcpy(buf, loc->filepath);
  char *file = basename(buf);

  if (loc->fl == loc->ll)
    sprintf(out, " %s %d:%d-%d", file, loc->fl, loc->fc, loc->lc);
  else
    sprintf(out, " %s %d:%d-%d:%d", file, loc->fl, loc->fc, loc->ll, loc->lc);

  return out;
}

int
eth_print_location_opt(eth_location *loc, FILE *stream, int opt)
{
  if (loc == NULL)
    return -1;

  FILE *fs = fopen(loc->filepath, "r");
  if (fs == NULL)
    return -1;

  if (opt & ETH_LOPT_FILE)
  {
    //char buf[PATH_MAX];
    //eth_get_location_file(loc, buf);
    //fprintf(stream, "%s:\n", buf);
    fprintf(stream, "%s:\n", loc->filepath);

    if (opt & ETH_LOPT_NEWLINES)
      putc('\n', stream);
  }

  int start = loc->fl;
  int end = loc->ll;
  if (opt & ETH_LOPT_EXTRALINES)
  {
    if (start > 1)
      start -= 1;
    end += 1;
  }

  int line = 1;
  int col = 1;
  int hl = false;
  do
  {
    errno = 0;
    int c = fgetc(fs);
    if (errno)
    {
      eth_error("print location: %s\n", strerror(errno));
      fclose(fs);
      return -1;
    }
    if (c == EOF)
    {
      fclose(fs);
      fputs("\e[0m", stream);
      goto end;
    }

    if (line >= start && line <= end)
    {
      if (not (opt & ETH_LOPT_NOCOLOR))
      {
        if (line == loc->fl && col == loc->fc)
        {
          hl = true;
          fputs("\e[38;5;9;1m", stream);
        }
      }

      if (col == 1)
      {
        if (hl)
          fputs("\e[0m", stream);

        if (not (opt & ETH_LOPT_NOLINENO))
          fprintf(stream, " %6d | ", line);
        else
          fprintf(stream, "| ");

        if (hl)
          fputs("\e[38;5;9;1m", stream);
      }

      putc(c, stream);

      if (not (opt & ETH_LOPT_NOCOLOR))
      {
        if (line == loc->ll && col == loc->lc - 1)
        {
          fputs("\e[0m", stream);
          hl = false;
        }
      }
    }

    if (c == '\n')
    {
      line += 1;
      col = 1;
    }
    else
    {
      col += 1;
    }

  } while (line <= end);

  fclose(fs);

end:
  if (opt & ETH_LOPT_NEWLINES)
    putc('\n', stream);
  return 0;
}

int
eth_print_location(eth_location *loc, FILE *stream)
{
  int opt = ETH_LOPT_FILE | ETH_LOPT_NEWLINES | ETH_LOPT_EXTRALINES;
  return eth_print_location_opt(loc, stream, opt);
}
