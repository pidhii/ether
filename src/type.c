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

eth_type*
eth_create_type(const char *name)
{
  eth_type *type = malloc(sizeof(eth_type));
  type->name = strdup(name);
  type->destroy = default_destroy;
  type->write = eth_default_write;
  type->display = default_display;
  type->nfields = 0;
  type->fields = NULL;
  type->clos = NULL;
  type->dtor = NULL;
  return type;
}

eth_type*
eth_create_struct_type(const char *name, char *const fields[],
    ptrdiff_t const offs[], int n)
{
  eth_type *type = eth_create_type(name);
  type->fields = malloc(sizeof(eth_field) * n);
  type->nfields = n;
  for (int i = 0; i < n; ++i)
  {
    type->fields[i].name = strdup(fields[i]);
    type->fields[i].offs = offs[i];
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

