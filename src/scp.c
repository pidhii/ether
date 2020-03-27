#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

ETH_MODULE("scp")

eth_scp*
eth_create_scp(eth_function **clos, int nclos, eth_t *wrefs, int nwref)
{
  eth_scp *scp = malloc(sizeof(eth_scp));
  scp->clos = malloc(sizeof(eth_function*) * nclos);
  scp->wrefs = malloc(sizeof(eth_t*) * (nwref + nclos));
  scp->nclos = nclos;
  scp->nwref = nwref + nclos;
  scp->rc = nwref;
  memcpy(scp->clos, clos, sizeof(eth_function*) * nclos);

  // sort arrays
  int cmp(const void *p1, const void *p2) { return p2 - p1; }
  qsort(clos , nclos, sizeof(void*), cmp);
  qsort(wrefs, nwref, sizeof(void*), cmp);

  // merge removing duplicates
  eth_t *p1 = (eth_t*)clos,
        *p2 = wrefs;
  for (size_t i = 0; i < scp->nwref; ++i)
  {
    if (p1 - (eth_t*)clos == nclos)
      scp->wrefs[i] = *p2++;
    else if (p2 - wrefs == nwref)
      scp->wrefs[i] = *p1++;
    else if (*p1 < *p2)
      scp->wrefs[i] = *p1++;
    else if (*p1 > *p2)
      scp->wrefs[i] = *p2++;
    else /* (*p1 == *p2) */
    {
      scp->wrefs[i] = *p1++;
      *p2++;
      scp->nwref -= 1;
    }
  }

  // set up magic
  for (size_t i = 0; i < scp->nwref; ++i)
  {
    eth_magic *magic = eth_require_magic(scp->wrefs[i]);
    eth_add_scope(magic, scp);
  }

  // fix RC for closures
  for (int i = 0; i < nclos; ++i)
    clos[i]->header.rc = 1;

  return scp;
}

void
eth_destroy_scp(eth_scp *scp)
{
  free(scp->clos);
  free(scp->wrefs);
  free(scp);
}

void
eth_drop_out(eth_scp *scp)
{
  size_t nclos = scp->nclos;
  eth_function **restrict clos = scp->clos;
  size_t nwref = scp->nwref;
  eth_t *restrict wrefs = scp->wrefs;

  if (--scp->rc != 0)
    return;

  // validate scope-RC:
  size_t rc = 0;
  for (size_t i = 0; i < nwref; ++i)
    rc += (wrefs[i]->rc != 0);
  if ((scp->rc = rc) != 0)
    return;

  // Drop scope:
  // 1. deactivate closures
  // 1. drop weak refs
  // 2. free closures
  for (size_t i = 0; i < nclos; ++i)
    eth_deactivate_clos(clos[i]);
  for (size_t i = 0; i < nclos; ++i)
    clos[i]->header.rc = -1; // to keep closures alive while dorpping wrefs
  // --
  for (size_t i = 0; i < nwref; ++i)
  {
    if (eth_remove_scope(eth_get_magic(wrefs[i]->magic), scp) == 0)
    {
      if (wrefs[i]->rc == 0)
      {
        /*eth_unref_magic(eth_get_magic(wrefs[i]->magic));*/
        /*wrefs[i]->magic = ETH_MAGIC_NONE;*/
        eth_drop(wrefs[i]);
      }
    }
  }
  // --
  for (size_t i = 0; i < nclos; ++i)
  {
    /*eth_unref_magic(eth_get_magic(clos[i]->header.magic));*/
    /*clos[i]->header.magic = ETH_MAGIC_NONE;*/
    eth_delete(ETH(clos[i]));
  }

  eth_destroy_scp(scp);
}
