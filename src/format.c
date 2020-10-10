#include "ether/ether.h"

#include <assert.h>
#include <string.h>

int
eth_study_format(const char *fmt)
{
  int n = 0;
  const char *p = fmt;
  while (true)
  {
    if ((p = strchr(p, '~')))
    {
      if (p[1] == 'w' || p[1] == 'd')
      {
        n += 1;
        p += 2;
      }
      else if (p[1] == '~')
      {
        p += 2;
      }
      else
      {
        return -1;
      }
      continue;
    }
    return n;
  }
}

bool
eth_format(FILE *out, const char *fmt, eth_t args[], int n)
{
  int ipar = 0;
  for (const char *p = fmt; *p; ++p)
  {
    switch (*p)
    {
      case '~':
        switch (p[1])
        {
          case 'w':
            eth_write(args[ipar], out);
            eth_drop(args[ipar]);
            ipar += 1;
            p += 1;
            break;

          case 'd':
            eth_display(args[ipar], out);
            eth_drop(args[ipar]);
            ipar += 1;
            p += 1;
            break;

          case '~':
            putc('~', out);
            p += 1;
            break;

          default:
            assert(!"wtf");
        }
        break;

      default:
        putc(*p, out);
    }
  }

  return true;
}

