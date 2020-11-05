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
