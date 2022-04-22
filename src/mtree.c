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

#include <stdlib.h>
#include <string.h>

void
eth_init_mtree_case(eth_mtree_case *c, const eth_type *type, const int offs[],
    const int ssavids[], int n, eth_mtree *tree)
{
  c->type = type;
  c->offs = malloc(sizeof(int) * n);
  c->ssavids = malloc(sizeof(int) * n);
  c->n = n;
  c->tree = tree;
  memcpy(c->offs, offs, sizeof(int) * n);
  memcpy(c->ssavids, ssavids, sizeof(int) * n);
}

void
eth_cleanup_mtree_case(eth_mtree_case *c)
{
  free(c->offs);
  free(c->ssavids);
  eth_destroy_mtree(c->tree);
}

void
eth_init_mtree_ccase(eth_mtree_ccase *c, eth_t cval, eth_mtree *tree)
{
  eth_ref(c->cval = cval);
  c->tree = tree;
}

void
eth_cleanup_mtree_ccase(eth_mtree_ccase *c)
{
  eth_unref(c->cval);
  eth_destroy_mtree(c->tree);
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
eth_create_switch(int ssavid, const eth_mtree_case cases[], int ncases,
    eth_mtree *dflt)
{
  eth_mtree *t = malloc(sizeof(eth_mtree));
  t->tag = ETH_MTREE_SWITCH;
  t->swtch.ssavid = ssavid;
  t->swtch.cases = malloc(sizeof(eth_mtree_case) * ncases);
  t->swtch.ncases = ncases;
  memcpy(t->swtch.cases, cases, sizeof(eth_mtree_case) * ncases);
  t->swtch.dflt = dflt;
  return t;
}

eth_mtree*
eth_create_cswitch(int ssavid, const eth_mtree_ccase cases[], int ncases,
    eth_mtree *dflt)
{
  eth_mtree *t = malloc(sizeof(eth_mtree));
  t->tag = ETH_MTREE_CSWITCH;
  t->cswtch.ssavid = ssavid;
  t->cswtch.cases = malloc(sizeof(eth_mtree_ccase) * ncases);
  t->cswtch.ncases = ncases;
  memcpy(t->cswtch.cases, cases, sizeof(eth_mtree_ccase) * ncases);
  t->cswtch.dflt = dflt;
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
      for (int i = 0; i < t->swtch.ncases; ++i)
        eth_cleanup_mtree_case(t->swtch.cases + i);
      free(t->swtch.cases);
      eth_destroy_mtree(t->swtch.dflt);
      break;

    case ETH_MTREE_CSWITCH:
      for (int i = 0; i < t->cswtch.ncases; ++i)
        eth_cleanup_mtree_ccase(t->cswtch.cases + i);
      free(t->cswtch.cases);
      eth_destroy_mtree(t->cswtch.dflt);
      break;
  }

  free(t);
}

