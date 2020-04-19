#include "ether/ether.h"

#include <stdlib.h>

eth_attr*
eth_create_attr(int flag)
{
  eth_attr *attr = malloc(sizeof(eth_attr));
  *attr = (eth_attr) {
    .rc = 0,
    .flag = flag,
  };
  return attr;
}

static void
destroy_attr(eth_attr *attr)
{
  free(attr);
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
