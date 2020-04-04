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

int
eth_print_location(eth_location *loc, FILE *stream)
{
  if (loc == NULL)
    return -1;

  FILE *fs = fopen(loc->filepath, "r");
  if (fs == NULL)
    return -1;

  putc('\n', stream);
  char buf[PATH_MAX];
  strcpy(buf, loc->filepath);
  char *file = basename(buf);
  if (loc->fl == loc->ll)
    fprintf(stream, " %s %d:%d-%d:\n\n", file, loc->fl, loc->fc, loc->lc);
  else
    fprintf(stream, " %s %d:%d-%d:%d:\n\n", file, loc->fl, loc->fc, loc->ll, loc->lc);

  int line = 1;
  int col = 1;
  int hl = false;
  while (true)
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
      /*eth_error("print location: unexpected end of file\n");*/
      /*eth_error = 1;*/
      fclose(fs);
      fputs("\e[0m", stream);
      goto end;
    }

    if (line >= loc->fl && line <= loc->ll)
    {
      if (line == loc->fl && col == loc->fc)
      {
        hl = true;
        fputs("\e[38;5;9;1m", stream);
      }

      if (col == 1)
      {
        fputs("\e[0m", stream);
        fprintf(stream, " %6d | ", line);
        if (hl)
          fputs("\e[38;5;9;1m", stream);
      }

      putc(c, stream);

      if (line == loc->ll && col == loc->lc - 1)
        fputs("\e[0m", stream);
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

    if (line > loc->ll)
      break;
  }

  fclose(fs);

end:
  putc('\n', stream);
  return 0;
}
