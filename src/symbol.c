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
  /*assert(sym->header.rc == 1);*/
  if (eth_unlikely(sym->header.rc != 1))
  {
    eth_warning("RC for symbol '%s' /= 1", sym->str);
  }
  free(sym);
}

static void
write_symbol(eth_type *type, eth_t x, FILE *stream)
{
  symbol *sym = (void*)x;
  fprintf(stream, "`%s", sym->str);
}

void
_eth_init_symbol_type(void)
{
  g_symtab = cod_hash_map_new(0);

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

eth_hash_t
eth_get_symbol_hash(eth_t x)
{
  symbol *sym = (void*)x;
  return sym->hash;
}

