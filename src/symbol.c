#include "ether/ether.h"
#include "codeine/hash-map.h"
#include "codeine/hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ETH_MODULE("ether:symbol")


eth_type *eth_symbol_type;

typedef struct {
  eth_header header;
  char *str;
  size_t len;
  eth_hash_t hash;
} symbol;

static
cod_hash_map *g_symtab;

static void
destroy_symbol(void *ptr)
{
  symbol *sym = ptr;
  assert(sym->header.rc == 1);
  free(sym);
}

static void
write_symbol(eth_type *type, eth_t x, FILE *stream)
{
  symbol *sym = (void*)x;
  fputs(sym->str, stream);
}

void
_eth_init_symbol_type(void)
{
  g_symtab = cod_hash_map_new();

  eth_symbol_type = eth_create_type("symbol");
  eth_symbol_type->write = write_symbol;
}

void
_eth_cleanup_symbol_type(void)
{
  cod_hash_map_delete(g_symtab, destroy_symbol);
  eth_destroy_type(eth_symbol_type);
}

eth_t
eth_create_symbol(const char *str)
{
  size_t len = strlen(str);
  eth_hash_t hash = cod_halfsiphash(eth_get_siphash_key(), (void*)str, len);

  cod_hash_map_elt *elt;
  if ((elt = cod_hash_map_find(g_symtab, str, hash)))
  {
    symbol *sym = elt->val;
    return ETH(sym);
  }
  else
  {
    symbol *sym = malloc(sizeof(symbol));
    eth_init_header(sym, eth_symbol_type);
    sym->header.rc = 1;
    sym->hash = hash;
    sym->str = strdup(str);
    sym->len = len;
    int ok = cod_hash_map_insert_drain(g_symtab, sym->str, hash, sym, NULL);
    assert(ok);
    return ETH(sym);
  }
}

const char*
eth_get_symbol_cstr(eth_t x)
{
  symbol *sym = (void*)x;
  return sym->str;
}

