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
eth_create_scanner(eth_root *root, FILE *stream)
{
  eth_scanner *scan;
  yylex_init(&scan);
  yyset_in(stream, scan);

  eth_scanner_data *data = malloc(sizeof(eth_scanner_data));
  data->root = root;
  cod_vec_init(data->primtoks);
  data->commentcnt = 0;
  cod_vec_init(data->fmtbracecnt);
  cod_vec_init(data->quotestack);
  cod_vec_init(data->indentstack);
  data->curindent = 0;
  cod_vec_init(data->indlvlstack);
  cod_vec_init(data->statestack);
  data->curstate = 0 /* INITIAL */;
  data->isrepl = false;
  yyset_extra(data, scan);

  return scan;
}

eth_scanner*
eth_create_repl_scanner(eth_root *root, FILE *stream)
{
  eth_scanner *scan = eth_create_scanner(root, stream);
  eth_get_scanner_data(scan)->isrepl = true;
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

bool
eth_is_repl_scanner(eth_scanner *scan)
{
  return eth_get_scanner_data(scan)->isrepl;
}

