#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>

eth_attr*
eth_create_attr(int flag)
{
  eth_attr *attr = malloc(sizeof(eth_attr));
  *attr = (eth_attr) {
    .rc = 0,
    .flag = flag,
    .help = NULL,
    .loc = NULL,
  };
  return attr;
}

static void
destroy_attr(eth_attr *attr)
{
  if (attr->help)
    free(attr->help);
  if (attr->loc)
    eth_unref_location(attr->loc);
  free(attr);
}

void
eth_set_help(eth_attr *attr, const char *help)
{
  if (attr->help)
    free(attr->help);
  attr->help = strdup(help);
}

void
eth_set_location(eth_attr *attr, eth_location *loc)
{
  eth_ref_location(loc);
  if (attr->loc)
    eth_unref_location(attr->loc);
  attr->loc = loc;
}

void
eth_ref_attr(eth_attr *attr)
{
  attr->rc += 1;
}

void
eth_unref_attr(eth_attr *attr)
{
  if (--attr->rc == 0)
    destroy_attr(attr);
}

void
eth_drop_attr(eth_attr *attr)
{
  if (attr->rc == 0)
    destroy_attr(attr);
}
