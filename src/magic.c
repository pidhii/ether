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
#include "codeine/vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

ETH_MODULE("magic");

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                magic API
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct eth_magic {
  size_t rc;
  cod_vec(eth_scp*) scps;
  eth_magic *next;
  eth_word_t id;
};

static inline void
init_magic(eth_magic *magic)
{
  cod_vec_init(magic->scps);
  magic->rc = 0;
}

static inline void
deinit_magic(eth_magic *magic)
{
  cod_vec_destroy(magic->scps);
}

eth_word_t
eth_get_magic_id(eth_magic *magic)
{
  return magic->id;
}

eth_scp* const*
eth_get_scopes(eth_magic *magic, int *n)
{
  if (n) *n = magic->scps.len;
  return magic->scps.data;
}

int
eth_add_scope(eth_magic *magic, eth_scp *scp)
{
  cod_vec_push(magic->scps, scp);
  return magic->scps.len;
}

int
eth_remove_scope(eth_magic *magic, eth_scp *scp)
{
  int k = -1;
  cod_vec_iter(magic->scps, i, x, if (x == scp) { k = i; break; });
  assert(k >= 0);
  cod_vec_erase(magic->scps, k);
  return magic->scps.len;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                        uniform allocator for magic
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#define POOL_SIZE_LOG2 10
#define POOL_SIZE (1 << POOL_SIZE_LOG2)

typedef struct {
  eth_magic arr[POOL_SIZE];
} magic_pool;

static magic_pool*
create_pool(size_t id0)
{
  magic_pool *pool = malloc(sizeof(magic_pool));
  for (size_t i = 0; i < POOL_SIZE; ++i)
    pool->arr[i].id = id0 + i;
  return pool;
}

static
size_t g_curidx;

static
cod_vec(magic_pool*) g_pools;

static
eth_magic *g_free_mgc;

void
_eth_init_magic(void)
{
  cod_vec_init(g_pools);
  cod_vec_push(g_pools, create_pool(0));
  g_curidx = 0;
  g_free_mgc = NULL;
}

void
_eth_cleanup_magic(void)
{
  cod_vec_iter(g_pools, i, x, free(x));
  cod_vec_destroy(g_pools);
}

eth_magic*
eth_create_magic(void)
{
  if (g_free_mgc)
  {
    eth_magic *magic = g_free_mgc;
    g_free_mgc = g_free_mgc->next;
    init_magic(magic);
    return magic;
  }
  else
  {
    if (eth_unlikely(g_curidx == POOL_SIZE))
    {
      g_curidx = 0;
      magic_pool *pool = create_pool(g_pools.len * POOL_SIZE);
      cod_vec_push(g_pools, pool);
    }
    magic_pool *pool = cod_vec_last(g_pools);
    eth_magic *magic = pool->arr + g_curidx++;
    init_magic(magic);
    return magic;
  }
}

static void
destroy_magic(eth_magic *magic)
{
  deinit_magic(magic);
  magic->next = g_free_mgc;
  g_free_mgc = magic;
}

void
eth_ref_magic(eth_magic *magic)
{
  magic->rc += 1;
}

void
eth_unref_magic(eth_magic *magic)
{
  if (--magic->rc == 0)
    destroy_magic(magic);
}

void
eth_drop_magic(eth_magic *magic)
{
  if (magic->rc == 0)
    destroy_magic(magic);
}

eth_magic*
eth_get_magic(eth_word_t magicid)
{
  eth_word_t cellidx = magicid & (POOL_SIZE - 1);
  eth_word_t poolidx = magicid >> POOL_SIZE_LOG2;
  magic_pool *pool = g_pools.data[poolidx];
  return pool->arr + cellidx;
}

