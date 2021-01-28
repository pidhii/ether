#include "ether/ether.h"

#include "codeine/hash-map.h"

#include <string.h>

ETH_MODULE("ether:methods")

#ifndef ETH_METHOD_STAB_SIZE
# define ETH_METHOD_STAB_SIZE 30
#endif

typedef struct {
  size_t id;
  eth_method_cb cb;
} method;

struct eth_methods {
  int n;
  union {
    method stab[ETH_METHOD_STAB_SIZE+1];
    cod_hash_map *ltab;
  };
};

static int
find_in_stab(method *stab, int n, size_t id)
{
  stab[n].id = id;
  for (int i = 0; true; ++i)
  {
    if (stab[i].id == id)
      return i;
  }
}

static void
sort_stab(method *stab, int n)
{
  int cmp(const void *p1, const void *p2)
  {
    const method *m1 = p1;
    const method *m2 = p2;
    return m1->id - m2->id;
  }
  qsort(stab, n, sizeof(method), cmp);
}

eth_methods*
eth_create_methods()
{
  eth_methods *ms = malloc(sizeof(eth_methods));
  ms->n = 0;
  return ms;
}

void
eth_destroy_methods(eth_methods *ms)
{
  if (ms->n >= ETH_METHOD_STAB_SIZE)
    cod_hash_map_delete(ms->ltab, NULL);
  free(ms);
}

bool
eth_add_method(eth_methods *ms, eth_t sym, eth_method_cb cb)
{
  size_t id = eth_get_symbol_id(sym);
  if (ms->n + 1 < ETH_METHOD_STAB_SIZE)
  { // using stab
    if (find_in_stab(ms->stab, ms->n, id) < ms->n)
    {
      eth_warning("method '%s' already present, won't update");
      return false;
    }
    method *m = ms->stab + ms->n++;
    m->id = id;
    m->cb = cb;
    sort_stab(ms->stab, ms->n);
  }
  else
  {
    if (ms->n + 1 == ETH_METHOD_STAB_SIZE)
    { // stab is full => switch to ltab

      if (find_in_stab(ms->stab, ms->n, id) < ms->n)
      { // will not insert anyway if it is a duplicate
        eth_warning("method '~w' already present, won't update", sym);
        return false;
      }

      cod_hash_map *ltab = cod_hash_map_new(COD_HASH_MAP_INTKEYS);
      // copy stab to ltab
      for (int i = 0; i < ETH_METHOD_STAB_SIZE; ++i)
        cod_hash_map_insert(ltab, (void*)sym, id, ms->stab[i].cb, NULL);
      ms->ltab = ltab;
    }

    if (cod_hash_map_find(ms->ltab, (void*)sym, id))
    {
      eth_warning("method '~w` already present, won't update", sym);
      return false;
    }
    cod_hash_map_insert(ms->ltab, (void*)sym, id, cb, NULL);
  }

  return true;
}

eth_method_cb
eth_get_method(eth_methods *ms, eth_t sym)
{
  size_t id = eth_get_symbol_id(sym);
  if (ms->n >= ETH_METHOD_STAB_SIZE)
  {
    int idx = find_in_stab(ms->stab, ms->n, id);
    if (eth_unlikely(idx >= ms->n))
      return NULL;
    else
      return ms->stab[idx].cb;
  }
  else
  {
    cod_hash_map_elt *elt = cod_hash_map_find(ms->ltab, (void*)sym, id);
    return elt ? elt->val : NULL;
  }
}

