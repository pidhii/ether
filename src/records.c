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
cod_hash_map *g_rectab;

static
cod_vec(eth_type*) g_unque_types;

void
_eth_init_record_types(void)
{
  g_rectab = cod_hash_map_new(0);
}

void
_eth_cleanup_record_types(void)
{
  cod_hash_map_delete(g_rectab, (void*)eth_destroy_type);
  for (size_t i = 0; i < g_unque_types.len; ++i)
    eth_destroy_type(g_unque_types.data[i]);
  cod_vec_destroy(g_unque_types);
}

void
eth_destroy_record(eth_type *type, eth_t x)
{
  assert(type->flag & ETH_TFLAG_PLAIN);
  eth_struct *tup = ETH_TUPLE(x);
  for (int i = 0; i < type->nfields; ++i)
    eth_unref(tup->data[i]);
  eth_free(tup, sizeof(eth_struct) + sizeof(eth_t)*type->nfields);
}

static void
write_tuple(eth_type *type, eth_t x, FILE *stream)
{
  int n = type->nfields;
  eth_struct *tup = ETH_TUPLE(x);
  putc('#', stream);
  putc('(', stream);
  for (int i = 0; i < n; ++i)
  {
    if (i > 0) putc(',', stream);
    eth_write(tup->data[i], stream);
  }
  putc(')', stream);
}

static void
write_record(eth_type *type, eth_t x, FILE *stream)
{
  if (eth_is_like_tuple(type))
  {
    write_tuple(type, x, stream);
  }
  else
  {
    int n = type->nfields;
    eth_struct *tup = ETH_TUPLE(x);
    putc('#', stream);
    putc('{', stream);
    for (int i = 0; i < n; ++i)
    {
      if (i > 0) putc(',', stream);
      eth_fprintf(stream, "%s=~w", type->fields[i].name, tup->data[i]);
    }
    putc('}', stream);
  }
}

static bool
struct_equal(eth_type *type, eth_t x, eth_t y)
{
  int n = eth_struct_size(type);
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
  char *keys[n];
  for (size_t i = 0; i < n; ++i)
    keys[i] = (char*)eth_get_symbol_cstr(eth_ordsyms[i+1]);
  eth_type *ret = eth_record_type(keys, n);
  return ret;
}

typedef struct { const char *str; size_t id; } info;
static void
study(char *const fields[], size_t n, info fldinfo[], size_t *namelen)
{
  *namelen = 0;
  for (size_t i = 0; i < n; ++i)
  {
    fldinfo[i].str = fields[i];
    fldinfo[i].id = eth_get_symbol_id(eth_sym(fields[i]));
    *namelen += strlen(fields[i]);
  }
  *namelen += n + 2;

  int cmp(const void *p1, const void *p2)
  {
    const info *i1 = p1;
    const info *i2 = p2;
    return i1->id - i2->id;
  }
  qsort(fldinfo, n, sizeof(info), cmp);
}

static bool
is_tuple(const info fldinfo[], size_t n)
{
  for (size_t i = 0; i < n; ++i)
  {
    if (strcmp(fldinfo[i].str, eth_get_symbol_cstr(eth_ordsyms[i+1])) != 0)
      return false;
  }
  return true;
}

static void
generate_name(const info fldinfo[], size_t n, char *name)
{
  char *p = name;
  *p = 0;
  // ---
  *p++ = '{';
  for (size_t i = 0; i < n; ++i)
  {
    if (i > 0) *p++ = ',';
    int len = strlen(fldinfo[i].str);
    memcpy(p, fldinfo[i].str, len);
    p += len;
  }
  *p++ = '}';
  *p++ = '\0';
}

static void
make_fields(const info fldinfo[], size_t n, eth_field fields[])
{
  for (size_t i = 0; i < n; ++i)
  {
    fields[i].name = (char*)fldinfo[i].str;
    fields[i].offs = offsetof(eth_struct, data[i]);
  }
}

static eth_type*
record_type(char *const fldnames[], size_t n, bool unique)
{
  info arr[n];
  size_t totlen;
  study(fldnames, n, arr, &totlen);

  bool istuple = is_tuple(arr, n);

  char totstr[totlen + 1];
  generate_name(arr, n, totstr);

  eth_field fields[n];
  make_fields(arr, n, fields);

  eth_hash_t hash = cod_halfsiphash(eth_get_siphash_key(), (void*)totstr, totlen);

  if (not unique)
  {
    cod_hash_map_elt *elt;
    if ((elt = cod_hash_map_find(g_rectab, totstr, hash)))
      return elt->val;
  }

  eth_type *type = eth_create_struct_type(totstr, fields, n);
  type->destroy = eth_destroy_record;
  type->flag = istuple ? ETH_TFLAG_TUPLE : ETH_TFLAG_RECORD;
  type->write = write_record;
  type->equal = struct_equal;

  if (unique)
    cod_vec_push(g_unque_types, type);
  else
  {
    int ok = cod_hash_map_insert(g_rectab, totstr, hash, type, NULL);
    assert(ok);
  }

  return type;
}

// TODO: this looks ugly
eth_type*
eth_record_type(char *const fields[], size_t n)
{
  return record_type(fields, n, false);
}

eth_type*
eth_unique_record_type(char *const fields[], size_t n)
{
  return record_type(fields, n, true);
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
      eth_struct *tup = eth_malloc(sizeof(eth_struct) + sizeof(eth_t) * n);
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
  eth_struct *rec = eth_alloc(sizeof(eth_struct) + sizeof(eth_t)*n);
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

