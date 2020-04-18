#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>

eth_mtree_ctor*
eth_create_const_ctor(eth_t cval)
{
  eth_mtree_ctor *ctor = malloc(sizeof(eth_mtree_ctor));
  ctor->tag = ETH_MTREE_CONST;
  eth_ref(ctor->cval = cval);
  return ctor;
}

eth_mtree_ctor*
eth_create_unpack_ctor(eth_type *type, int *offs, int *vids, int n)
{
  eth_mtree_ctor *ctor = malloc(sizeof(eth_mtree_ctor));
  ctor->tag = ETH_MTREE_UNPACK;
  ctor->unpack.type = type;
  ctor->unpack.offs = malloc(sizeof(int) * n);
  memcpy(ctor->unpack.offs, offs, sizeof(int) * n);
  ctor->unpack.vids = malloc(sizeof(int) * n);
  memcpy(ctor->unpack.vids, vids, sizeof(int) * n);
  ctor->unpack.n = n;
  return ctor;
}

void
eth_destroy_mtree_ctor(eth_mtree_ctor *ctor)
{
  switch (ctor->tag)
  {
    case ETH_MTREE_CONST:
      eth_unref(ctor->cval);
      break;

    case ETH_MTREE_UNPACK:
      free(ctor->unpack.offs);
      free(ctor->unpack.vids);
      break;
  }

  free(ctor);
}

eth_mtree*
eth_create_fail(void)
{
  eth_mtree *t = malloc(sizeof(eth_mtree));
  t->tag = ETH_MTREE_FAIL;
  return t;
}

eth_mtree*
eth_create_leaf(eth_insn *body)
{
  eth_mtree *t = malloc(sizeof(eth_mtree));
  t->tag = ETH_MTREE_LEAF;
  t->leaf = body;
  return t;
}

eth_mtree*
eth_create_switch(const eth_mtree_case L[], int n)
{
  eth_mtree *t = malloc(sizeof(eth_mtree));
  t->tag = ETH_MTREE_SWITCH;
  t->swtch.n = n;
  t->swtch.L = malloc(sizeof(eth_mtree_case) * n);
  memcpy(t->swtch.L, L, sizeof(eth_mtree_case) * n);
  return t;
}

void
eth_destroy_mtree(eth_mtree *t)
{
  switch (t->tag)
  {
    case ETH_MTREE_FAIL:
      break;

    case ETH_MTREE_LEAF:
      eth_destroy_insn_list(t->leaf);
      break;

    case ETH_MTREE_SWITCH:
      for (int i = 0; i < t->swtch.n; ++i)
      {
        eth_mtree_case *c = t->swtch.L + i;
        if (c->ctor)
          eth_destroy_mtree_ctor(c->ctor);
        eth_destroy_mtree(c->tree);
      }
      free(t->swtch.L);
      break;
  }

  free(t);
}

