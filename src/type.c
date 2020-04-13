#include "ether/ether.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

ETH_MODULE("type")

static void
default_destroy(eth_type *type, eth_t x)
{
  eth_warning("destructor for type '%s' not specified", type->name);
}

static void
default_display(eth_type *type, eth_t x, FILE *stream)
{
  type->write(type, x, stream);
}

static eth_type*
create_type(const char *name, int nfields)
{
  eth_type *type = malloc(sizeof(eth_type) + sizeof(size_t) * (nfields + 1));
  type->name = strdup(name);
  type->destroy = default_destroy;
  type->write = eth_default_write;
  type->display = default_display;
  type->equal = NULL;
  type->nfields = 0;
  type->fields = NULL;
  type->clos = NULL;
  type->dtor = NULL;
  type->flag = 0;
  return type;
}

eth_type*
eth_create_type(const char *name)
{
  return create_type(name, 0);
}

eth_type*
eth_create_struct_type(const char *name, char *const fields[],
    ptrdiff_t const offs[], int n)
{
  eth_type *type = create_type(name, n);
  type->fields = malloc(sizeof(eth_field) * n);
  type->nfields = n;
  for (int i = 0; i < n; ++i)
  {
    type->fields[i].name = strdup(fields[i]);
    type->fields[i].offs = offs[i];
    type->fieldids[i] = eth_get_symbol_id(eth_sym(fields[i]));
  }
  return type;
}

void
eth_destroy_type(eth_type *type)
{
  if (type->dtor)
    type->dtor(type->clos);

  if (type->fields)
  {
    for (int i = 0; i < type->nfields; ++i)
      free(type->fields[i].name);
    free(type->fields);
  }

  free(type->name);
  free(type);
}

eth_field* __attribute__((pure))
eth_get_field(eth_type *type, const char *field)
{
  for (int i = 0; i < type->nfields; ++i)
  {
    if (strcmp(field, type->fields[i].name) == 0)
      return type->fields + i;
  }
  return NULL;
}

void
eth_default_write(eth_type *type, eth_t x, FILE *out)
{
  fprintf(out, "<%s %p>", type->name, x);
}

void
eth_write(eth_t x, FILE *out)
{
  x->type->write(x->type, x, out);
}

void
eth_display(eth_t x, FILE *out)
{
  x->type->display(x->type, x, out);
}

bool
eth_equal(eth_t x, eth_t y)
{
  if (x == y)
    return true;
  else if (x->type != y->type)
    return false;
  else if (x->type->equal)
    return x->type->equal(x->type, x, y);
  else
    return false;
}


