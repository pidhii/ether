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


ETH_MODULE("ether:record")


static
cod_hash_map *g_tuptab;

static
cod_hash_map *g_rectab;


void
_eth_init_record_types(void)
{
  g_tuptab = cod_hash_map_new(0);
  g_rectab = cod_hash_map_new(0);
}

void
_eth_cleanup_record_types(void)
{
  cod_hash_map_delete(g_tuptab, (void*)eth_destroy_type);
  cod_hash_map_delete(g_rectab, (void*)eth_destroy_type);
}

void
eth_destroy_record_h1(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  eth_unref(tup->data[0]);
  eth_free_h1(tup);
}

void
eth_destroy_record_h2(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  eth_unref(tup->data[0]);
  eth_unref(tup->data[1]);
  eth_free_h2(tup);
}

void
eth_destroy_record_h3(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  eth_unref(tup->data[0]);
  eth_unref(tup->data[1]);
  eth_unref(tup->data[2]);
  eth_free_h3(tup);
}

void
eth_destroy_record_h4(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  eth_unref(tup->data[0]);
  eth_unref(tup->data[1]);
  eth_unref(tup->data[2]);
  eth_unref(tup->data[3]);
  eth_free_h4(tup);
}

void
eth_destroy_record_h5(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  eth_unref(tup->data[0]);
  eth_unref(tup->data[1]);
  eth_unref(tup->data[2]);
  eth_unref(tup->data[3]);
  eth_unref(tup->data[4]);
  eth_free_h5(tup);
}

void
eth_destroy_record_h6(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  eth_unref(tup->data[0]);
  eth_unref(tup->data[1]);
  eth_unref(tup->data[2]);
  eth_unref(tup->data[3]);
  eth_unref(tup->data[4]);
  eth_unref(tup->data[5]);
  eth_free_h6(tup);
}

void
eth_destroy_record_hn(eth_type *type, eth_t x)
{
  if (eth_unlikely(type->nfields <= 6))
  {
    eth_error("invalid free for record %s (%d fields)", type->name, type->nfields);
  }
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_tuple *tup = ETH_TUPLE(x);
  for (int i = 0; i < type->nfields; ++i)
    eth_unref(tup->data[i]);
  free(tup);
}

static void
write_tuple(eth_type *type, eth_t x, FILE *stream)
{
  int n = type->nfields;
  eth_tuple *tup = ETH_TUPLE(x);
  putc('(', stream);
  for (int i = 0; i < n; ++i)
  {
    if (i > 0) putc(',', stream);
    eth_write(tup->data[i], stream);
  }
  putc(')', stream);
}

static bool
record_equal(eth_type *type, eth_t x, eth_t y)
{
  int n = eth_record_size(type);
  while (n--)
  {
    if (not eth_equal(eth_tup_get(x, n), eth_tup_get(y, n)))
      return false;
  }
  return true;
}

eth_type*
eth_tuple_type(size_t n)
{
  assert(n > 1);

  char key[22];
  sprintf(key, "%ju", n);

  cod_hash_map_elt *elt;
  if ((elt = cod_hash_map_find(g_tuptab, key, n)))
  {
    return elt->val;
  }
  else
  {
    char name[n+3];
    name[0] = '(';
    for (size_t i = 0; i < n; ++i)
      name[i+1] = ',';
    name[n+1] = ')';
    name[n+2] = 0;

    char *fields[n];
    ptrdiff_t offs[n];
    for (size_t i = 0; i < n; ++i)
    {
      fields[i] = malloc(22);
      sprintf(fields[i], "_%ju", i+1);
      offs[i] = offsetof(eth_tuple, data[i]);
    }
    eth_type *type = eth_create_struct_type(name, fields, offs, n);
    for (size_t i = 0; i < n; free(fields[i++]));

    switch (n)
    {
      case 2:  type->destroy = eth_destroy_record_h2; break;
      case 3:  type->destroy = eth_destroy_record_h3; break;
      case 4:  type->destroy = eth_destroy_record_h4; break;
      case 5:  type->destroy = eth_destroy_record_h5; break;
      case 6:  type->destroy = eth_destroy_record_h6; break;
      default: type->destroy = eth_destroy_record_hn; break;
    }
    type->write = write_tuple;
    type->equal = record_equal;
    type->flag = ETH_TFLAG_TUPLE;

    int ok = cod_hash_map_insert(g_tuptab, key, n, type, NULL);
    assert(ok);
    return type;
  }
}


static void
write_record(eth_type *type, eth_t x, FILE *stream)
{
  int n = type->nfields;
  eth_tuple *tup = ETH_TUPLE(x);
  putc('{', stream);
  for (int i = 0; i < n; ++i)
  {
    if (i > 0) putc(',', stream);
    eth_fprintf(stream, "%s=~w", type->fields[i].name, tup->data[i]);
  }
  putc('}', stream);
}

// TODO: this looks ugly
eth_type*
eth_record_type(char *const fields[], size_t n)
{
  assert(n > 0);

  typedef struct { const char *str; size_t id; } info;
  info arr[n];
  size_t totlen = 0;
  for (size_t i = 0; i < n; ++i)
  {
    arr[i].str = fields[i];
    arr[i].id = eth_get_symbol_id(eth_sym(fields[i]));
    totlen += strlen(fields[i]);
  }
  totlen += n + 1;

  int cmp(const void *p1, const void *p2)
  {
    const info *i1 = p1;
    const info *i2 = p2;
    return i1->id - i2->id;
  }
  qsort(arr, n, sizeof(info), cmp);

  char *sortedfields[n];
  ptrdiff_t offsets[n];
  char totstr[totlen + 2];
  totstr[0] = 0;
  char *p = totstr;
  // ---
  *p++ = '{';
  for (size_t i = 0; i < n; ++i)
  {
    sortedfields[i] = (char*)arr[i].str;
    offsets[i] = offsetof(eth_tuple, data[i]);

    if (i > 0) *p++ = ',';
    int len = strlen(arr[i].str);
    memcpy(p, arr[i].str, len);
    p += len;
  }
  *p++ = '}';
  *p++ = '\0';
  // ---
  eth_hash_t hash = cod_halfsiphash(eth_get_siphash_key(), (void*)totstr, totlen);

  cod_hash_map_elt *elt;
  if ((elt = cod_hash_map_find(g_rectab, totstr, hash)))
  {
    return elt->val;
  }
  else
  {
    eth_debug("new record-type: %s", totstr);
    eth_type *type = eth_create_struct_type(totstr, sortedfields, offsets, n);
    switch (n)
    {
      case 1:  type->destroy = eth_destroy_record_h1; break;
      case 2:  type->destroy = eth_destroy_record_h2; break;
      case 3:  type->destroy = eth_destroy_record_h3; break;
      case 4:  type->destroy = eth_destroy_record_h4; break;
      case 5:  type->destroy = eth_destroy_record_h5; break;
      case 6:  type->destroy = eth_destroy_record_h6; break;
      default: type->destroy = eth_destroy_record_hn; break;
    }
    type->flag = ETH_TFLAG_RECORD;
    type->write = write_record;
    type->equal = record_equal;

    int ok = cod_hash_map_insert(g_rectab, totstr, hash, type, NULL);
    assert(ok);
    return type;
  }
}


eth_t
eth_create_tuple_n(eth_type *type, eth_t const data[])
{
  assert(eth_is_tuple(type));
  int n = type->nfields;
  assert(n > 1);
  switch (n)
  {
    case 2: return eth_tup2(data[0], data[1]); break;
    case 3: return eth_tup3(data[0], data[1], data[2]); break;
    case 4: return eth_tup4(data[0], data[1], data[2], data[3]); break;
    case 5: return eth_tup5(data[0], data[1], data[2], data[3], data[4]); break;
    case 6: return eth_tup6(data[0], data[1], data[2], data[3], data[4], data[5]); break;
    default:
    {
      eth_tuple *tup = malloc(sizeof(eth_tuple) + sizeof(eth_t) * n);
      eth_init_header(tup, type);
      for (int i = 0; i < n; ++i)
        eth_ref(tup->data[i] = data[i]);
      return ETH(tup);
    }
  }
}

eth_t
eth_create_record(eth_type *type, eth_t const data[])
{
  assert(eth_is_plain(type));
  int n = type->nfields;
  assert(n > 0);
  eth_tuple *rec;
  switch (n)
  {
    case 1:  rec = eth_alloc_h1(); break;
    case 2:  rec = eth_alloc_h2(); break;
    case 3:  rec = eth_alloc_h3(); break;
    case 4:  rec = eth_alloc_h4(); break;
    case 5:  rec = eth_alloc_h5(); break;
    case 6:  rec = eth_alloc_h6(); break;
    default: rec = malloc(sizeof(eth_tuple) + sizeof(eth_t) * n);
  }
  eth_init_header(rec, type);
  for (int i = 0; i < n; ++i)
    eth_ref(rec->data[i] = data[i]);
  return ETH(rec);
}

// TODO: cache produced records
eth_t
eth_record(char *const keys[], eth_t const vals[], int n)
{
  eth_t keysyms[n];
  for (int i = 0; i < n; ++i)
    keysyms[i] = eth_sym(keys[i]);

  // Sort fields by IDs:
  // - sort
  int idxs[n];
  for (int i = 0; i < n; idxs[i] = i, ++i);
  int cmp(const void *p1, const void *p2)
  {
    int i1 = *(int*)p1, i2 = *(int*)p2;
    return eth_get_symbol_id(keysyms[i1]) - eth_get_symbol_id(keysyms[i2]);
  }
  qsort(idxs, n, sizeof(int), cmp);
  // - reorder keys
  char *ordkeys[n];
  eth_t ordvals[n];
  for (int i = 0; i < n; ++i)
  {
    ordkeys[i] = keys[idxs[i]];
    ordvals[i] = vals[idxs[i]];
  }

  eth_type *recordtype = eth_record_type(ordkeys, n);
  return eth_create_record(recordtype, ordvals);
}

