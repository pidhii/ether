#include "ether/ether.h"

#include "scanner-data.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


extern int
yylex_init(eth_scanner **scanptr);

extern int
yylex_destroy(eth_scanner *scan);

extern void
yyset_in(FILE *in, eth_scanner *scan);

extern FILE*
yyget_in(eth_scanner *scan);

extern void
yyset_extra(eth_scanner_data *data, eth_scanner *scan);

extern eth_scanner_data*
yyget_extra(eth_scanner *scan);

eth_scanner*
eth_create_scanner(FILE *stream)
{
  eth_scanner *scan;
  yylex_init(&scan);
  yyset_in(stream, scan);

  eth_scanner_data *data = malloc(sizeof(eth_scanner_data));
  cod_vec_init(data->primtoks);
  data->commentcnt = 0;
  cod_vec_init(data->fmtbracecnt);
  cod_vec_init(data->quotestack);
  cod_vec_init(data->indentstack);
  data->curindent = 0;
  cod_vec_init(data->indlvlstack);
  cod_vec_init(data->statestack);
  data->curstate = 0 /* INITIAL */;
  yyset_extra(data, scan);

  return scan;
}

void
eth_destroy_scanner(eth_scanner *scan)
{
  eth_scanner_data *data = yyget_extra(scan);
  cod_vec_destroy(data->primtoks);
  cod_vec_destroy(data->fmtbracecnt);
  assert(data->quotestack.len == 0);
  cod_vec_destroy(data->quotestack);
  cod_vec_destroy(data->indentstack);
  cod_vec_destroy(data->indlvlstack);
  cod_vec_destroy(data->statestack);
  free(data);

  yylex_destroy(scan);
}

FILE*
eth_get_scanner_input(eth_scanner *scan)
{
  return yyget_in(scan);
}

eth_scanner_data*
eth_get_scanner_data(eth_scanner *scan)
{
  return yyget_extra(scan);
}
