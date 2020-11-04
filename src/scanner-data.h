#ifndef SCANNER_DATA
#define SCANNER_DATA

#include <codeine/vec.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


enum { QUOTES_DEFAULT, QUOTES_STRING };
typedef struct { int tag; char *str; } quote;

static inline void
destroy_quote(quote q)
{
  if (q.tag == QUOTES_STRING)
    free(q.str);
}

typedef struct indent_level {
  int nspaces;
  bool issilent;
} indent_level;

struct eth_scanner_data {
  cod_vec(int) primtoks;
  int commentcnt;
  cod_vec(int) fmtbracecnt;
  cod_vec(quote) quotestack;
  cod_vec(indent_level) indentstack;
  int curindent;
  cod_vec(int) indlvlstack;
  cod_vec(int) statestack;
  int curstate;
};

static inline void
_push_quote(struct eth_scanner_data *data, int tag, const char *maybestr)
{
  quote q = { .tag = tag, .str = maybestr ? strdup(maybestr) : NULL };
  cod_vec_push(data->quotestack, q);
}

static inline void
_pop_quote(struct eth_scanner_data *data)
{
  destroy_quote(cod_vec_pop(data->quotestack));
}

#endif
