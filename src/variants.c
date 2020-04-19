#include "ether/ether.h"
#include "codeine/hash-map.h"
#include "codeine/hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>


ETH_MODULE("ether:variants")


static
cod_hash_map *g_vartab;

void
_eth_init_variant_types(void)
{
  g_vartab = cod_hash_map_new();
}

void
_eth_cleanup_variant_types(void)
{
  cod_hash_map_delete(g_vartab, (void*)eth_destroy_type);
}

static void
destroy_variant(eth_type *type, eth_t x)
{
  eth_variant *var = ETH_VARIANT(x);
  eth_unref(var->val);
  eth_free_h1(var);
}

static void
write_variant(eth_type *type, eth_t x, FILE *file)
{
  char *tag = type->clos;
  eth_fprintf(file, "%s ~w", tag, eth_var_val(x));
}

static bool
variant_equal(eth_type *type, eth_t x, eth_t y)
{
  return eth_equal(eth_var_val(x), eth_var_val(y));
}

eth_type*
eth_variant_type(const char *tag)
{
  assert(isupper(tag[0]));

  size_t len = strlen(tag);
  eth_hash_t hash = cod_halfsiphash(eth_get_siphash_key(), (void*)tag, len);

  cod_hash_map_elt *elt;
  if ((elt = cod_hash_map_find(g_vartab, tag, hash)))
  {
    return elt->val;
  }
  else
  {
    char *_0 = "_0";
    ptrdiff_t offs = offsetof(eth_variant, val);
    eth_type *type = eth_create_struct_type("variant", &_0, &offs, 1);
    type->destroy = destroy_variant;
    type->write = write_variant;
    type->equal = variant_equal;
    type->flag = ETH_TFLAG_VARIANT;
    type->clos = strdup(tag);
    type->dtor = (void*)free;

    int ok = cod_hash_map_insert(g_vartab, tag, hash, type, NULL);
    assert(ok);
    return type;
  }
}

eth_t
eth_create_variant(eth_type *type, eth_t val)
{
  assert(eth_is_variant(type));
  eth_variant *var = eth_alloc_h1();
  eth_init_header(var, type);
  eth_ref(var->val = val);
  return ETH(var);
}

