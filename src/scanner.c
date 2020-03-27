#include "ether/ether.h"

#include <stdio.h>

extern int
yylex_init(eth_scanner **scanner);

extern int
yylex_destroy(eth_scanner *scanner);

extern void
yyset_in(FILE *in, eth_scanner *scanner);

extern FILE*
yyget_in(eth_scanner *scanner);

eth_scanner*
eth_create_scanner(FILE *stream)
{
  eth_scanner *scan;
  yylex_init(&scan);
  yyset_in(stream, scan);
  return scan;
}

void
eth_destroy_scanner(eth_scanner *scan)
{
  yylex_destroy(scan);
}

FILE*
eth_get_scanner_input(eth_scanner *scan)
{
  return yyget_in(scan);
}

