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
#include <ether/ether.h>
#include <codeine/vec.h>

#include <stdio.h>
#include <assert.h>

#include <json-c/json.h>


ETH_MODULE("ether:json")


static eth_t
to_eth(struct json_object *json)
{
  switch (json_object_get_type(json))
  {
    case json_type_null:
      return eth_nil;

    case json_type_boolean:
      return eth_boolean(json_object_get_boolean(json));

    case json_type_double:
      return eth_num(json_object_get_double(json));

    case json_type_int:
      return eth_num(json_object_get_int(json));

    case json_type_string:
      return eth_str(json_object_get_string(json));

    case json_type_object:
    {
      int n = json_object_object_length(json);
      char *keys[n];
      eth_t vals[n];
      int i = 0;
      json_object_object_foreach(json, key, val) {
        keys[i] = key;
        vals[i] = to_eth(val);
        i++;
      }
      return eth_record(keys, vals, n);
    }

    case json_type_array:
    {
      int n = json_object_array_length(json);
      eth_t buf[n];
      for (int i = 0; i < n; ++i)
        buf[i] = to_eth(json_object_array_get_idx(json, i));
      eth_t acc = eth_nil;
      for (int i = n - 1; i >= 0; --i)
        acc = eth_cons(buf[i], acc);
      return acc;
    }
  }

  eth_error("i dunno");
  abort();
}

static eth_t
parse(void)
{
  eth_args args = eth_start(1);
  eth_t str = eth_arg2(args, eth_string_type);
  struct json_object *json = json_tokener_parse(eth_str_cstr(str));
  if (json == NULL)
    eth_throw(args, eth_failure());
  eth_t ret = to_eth(json);
  json_object_put(json);
  eth_return(args, ret);
}


int
ether_module(eth_module *mod, eth_root *toplvl)
{
  eth_define(mod, "parse", eth_proc(parse, 1));
  return 0;
}

