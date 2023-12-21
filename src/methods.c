#include "ether/ether.h"

#include "codeine/hash-map.h"

#include <string.h>

ETH_MODULE("ether:methods")

#ifndef ETH_METHOD_STAB_SIZE
# define ETH_METHOD_STAB_SIZE 30
#endif


eth_type *eth_method_type;
eth_t eth_apply_method;
eth_t eth_enum_ctor_method;
eth_t eth_get_method;
eth_t eth_set_method;
eth_t eth_write_method;
eth_t eth_display_method;
eth_t eth_len_method;
eth_t eth_cmp_method;


static void
_destroy_method(eth_type *type, eth_t x)
{
  eth_method *method = (eth_method*)x;
  if (method->spec.default_impl)
    eth_unref(method->spec.default_impl);
  eth_free(method, sizeof(eth_method));
}

static eth_t
_apply(void)
{
  eth_use_symbol(unimplemented_method);
  eth_args args = eth_start(2);
  eth_t method = eth_arg(args);
  eth_t object = eth_arg(args);
  eth_t apply = eth_find_method(object->type->methods, method);
  if (apply == NULL)
    eth_throw(args, unimplemented_method);

  eth_reserve_stack(1);
  eth_sp[0] = object;
  eth_t ret = eth_apply(apply, 1);
  eth_return(args, ret);
}

void
_eth_init_methods(void)
{
  eth_method_type = eth_create_type("method");
  eth_method_type->destroy = _destroy_method;

  eth_ref(eth_apply_method = eth_create_method(1, NULL));
  eth_ref(eth_enum_ctor_method = eth_create_method(0, NULL));
  eth_ref(eth_get_method = eth_create_method(2, NULL));
  eth_ref(eth_set_method = eth_create_method(3, NULL));
  eth_ref(eth_write_method = eth_create_method(2, NULL));
  eth_ref(eth_display_method = eth_create_method(2, NULL));
  eth_ref(eth_len_method = eth_create_method(1, NULL));
  eth_ref(eth_cmp_method = eth_create_method(2, NULL));

  eth_add_method(eth_method_type->methods, eth_apply_method,
      eth_proc(_apply, 2, eth_apply_method));
}

void
_eth_cleanup_methods(void)
{
  eth_unref(eth_apply_method);
  eth_destroy_type(eth_method_type);
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                            eth_method
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
eth_t
eth_create_method(int arity, eth_t default_impl /* or NULL */)
{
  eth_method *method = eth_alloc(sizeof(eth_method));
  method->spec.arity = arity;
  if ((method->spec.default_impl = default_impl))
    eth_ref(default_impl);
  eth_init_header(&method->hdr, eth_method_type);
  return ETH(method);
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                            eth_methods
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
typedef struct {
  eth_t method;
  eth_t impl;
} method;

struct eth_methods {
  size_t size;
  union {
    method stab[ETH_METHOD_STAB_SIZE+1];
    cod_hash_map *ltab;
  };
};

static size_t
find_in_stab(method *stab, size_t size, eth_t m)
{
  stab[size].method = m;
  for (size_t i = 0; true; ++i)
  {
    if (stab[i].method == m)
      return i;
  }
}

static void
sort_stab(method *stab, size_t size)
{
  int cmp(const void *p1, const void *p2)
  {
    const method *m1 = p1;
    const method *m2 = p2;
    return m1->method - m2->method;
  }
  qsort(stab, size, sizeof(method), cmp);
}

eth_methods*
eth_create_methods()
{
  eth_methods *ms = eth_malloc(sizeof(eth_methods));
  ms->size = 0;
  return ms;
}

static void
_free_method(void *ptr)
{
  method *m = ptr;
  eth_unref(m->method);
  eth_unref(m->impl);
  free(m);
}

void
eth_destroy_methods(eth_methods *ms)
{
  if (ms->size >= ETH_METHOD_STAB_SIZE)
    cod_hash_map_delete(ms->ltab, _free_method);
  else
  {
    for (size_t i = 0; i < ms->size; ++i)
    {
      eth_unref(ms->stab[i].method);
      eth_unref(ms->stab[i].impl);
    }
  }
  free(ms);
}

bool
eth_add_method(eth_methods *ms, eth_t _method, eth_t impl)
{
  if (ms->size + 1 < ETH_METHOD_STAB_SIZE)
  { // using stab
    if (find_in_stab(ms->stab, ms->size, _method) < ms->size)
      return false;
    method *m = ms->stab + ms->size++;
    m->method = _method;
    eth_ref(m->method = _method);
    eth_ref(m->impl = impl);
    sort_stab(ms->stab, ms->size);
  }
  else
  {
    if (ms->size + 1 == ETH_METHOD_STAB_SIZE)
    { // stab is full => switch to ltab
      if (find_in_stab(ms->stab, ms->size, _method) < ms->size)
        // will not insert anyway if it is a duplicate
        return false;

      cod_hash_map *ltab = cod_hash_map_new(COD_HASH_MAP_INTKEYS);
      // copy stab to ltab
      for (int i = 0; i < ETH_METHOD_STAB_SIZE; ++i)
      {
        method *m = eth_malloc(sizeof(method));
        *m = ms->stab[i];
        cod_hash_map_insert(ltab, (void*)_method, (uintptr_t)_method, m, NULL);
      }
      ms->ltab = ltab;
    }

    if (cod_hash_map_find(ms->ltab, (void*)_method, (uintptr_t)_method))
      return false;

    method *m = eth_malloc(sizeof(method));
    eth_ref(m->method = _method);
    eth_ref(m->impl = impl);
    cod_hash_map_insert(ms->ltab, (void*)_method, (uintptr_t)_method, m, NULL);
  }

  return true;
}

eth_t
eth_find_method(eth_methods *ms, eth_t _method)
{
  if (ms->size <= ETH_METHOD_STAB_SIZE)
  {
    const size_t idx = find_in_stab(ms->stab, ms->size, _method);
    if (eth_unlikely(idx >= ms->size))
      return NULL;
    else
      return ms->stab[idx].impl;
  }
  else
  {
    cod_hash_map_elt *elt =
      cod_hash_map_find(ms->ltab, (void*)_method, (uintptr_t)_method);
    if (elt == NULL)
      return NULL;
    method *m = elt->val;
    return m->impl;
  }
}

