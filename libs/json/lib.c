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
#include <errno.h>
#include <math.h>

#include <json-c/json.h>


ETH_MODULE("ether:json")


static eth_t to_json_method;

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
      eth_t keys[n];
      eth_t vals[n];
      eth_t dict = eth_create_rbtree();
      json_object_object_foreach(json, key, val) {
        eth_t tmp = dict;
        dict = eth_rbtree_minsert(dict, eth_tup2(eth_str(key), to_eth(val)), NULL);
        eth_drop(tmp);
      }
      return dict;
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

static struct json_object*
to_json(eth_t x, FILE *err, bool *haserr)
{
  eth_t to_json_impl;

  if (x == eth_nil)
  {
    return json_object_new_null();
  }
  else if (x->type == eth_boolean_type)
  {
    return json_object_new_boolean(x == eth_true);
  }
  else if (x->type == eth_string_type)
  {
    return json_object_new_string_len(eth_str_cstr(x), eth_str_len(x));
  }
  else if (x->type == eth_number_type)
  {
    long double val = eth_num_val(x);
    long double i, f = modfl(val, &i);
    if (f == 0)
      return json_object_new_int64(i);
    else
    {
      char buf[256];
      sprintf(buf, "%Lf", val);
      return json_object_new_double_s(val, buf);
    }
  }
  else if (x->type == eth_rbtree_type)
  {
    struct json_object *ret = json_object_new_object();
    bool iter(eth_t x, void*)
    {
      eth_t k = eth_tup_get(x, 0);
      eth_t v = eth_tup_get(x, 1);
      if (k->type != eth_string_type)
      {
        eth_fprintf(err, "key is not a string (~w)", k);
        *haserr = true;
        return false;
      }
      else
      {
        json_object_object_add(ret, eth_str_cstr(k), to_json(v, err, haserr));
        return true;
      }
    }
    eth_rbtree_foreach(x, iter, NULL);
    return ret;
  }
  else if (x->type == eth_pair_type)
  {
    struct json_object *ret = json_object_new_array();
    for (eth_t it = x; it->type == eth_pair_type; it = eth_cdr(it))
      json_object_array_add(ret, to_json(eth_car(it), err, haserr));
    return ret;
  }
  else if ((to_json_impl = eth_find_method(x->type->methods, to_json_method)))
  {
    eth_reserve_stack(1);
    eth_sp[0] = x;
    x = eth_apply(to_json_impl, 1);
    if (x->type == eth_exception_type)
    {
      eth_fprintf(err, "exception during serialization (~w)", eth_what(x));
      *haserr = true;
      eth_drop(x);
      return json_object_new_null();
    }
    else
    {
      struct json_object *ret = to_json(x, err, haserr);
      eth_drop(x);
      return ret;
    }
  }
  else
  {
    eth_fprintf(err, "unserializable object (~w)", x);
    *haserr = true;
    return json_object_new_null();
  }
}

static eth_t
parse_string(void)
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

static eth_t
parse_stream(void)
{
  eth_use_symbol(io_error);
  eth_args args = eth_start(1);

  FILE *const stream = eth_get_file_stream(eth_arg2(args, eth_file_type));
  enum json_tokener_error jerr;
  char buf[256];
  struct json_object *json;
  struct json_tokener *jtok = json_tokener_new();
  long fpos;
  size_t nrd;
  do {
    fpos = ftell(stream);
    nrd = fread(buf, sizeof(char), sizeof(buf), stream);
    const int err = errno;
    if (nrd == 0)
    {
      if (feof(stream))
        buf[0] = 0;
      else if (ferror(stream))
      {
        json_tokener_free(jtok);
        eth_throw(args, eth_system_error(err));
      }
    }
    json = json_tokener_parse_ex(jtok, buf, nrd);
  } while ((jerr = json_tokener_get_error(jtok)) == json_tokener_continue);

  if (nrd != 0)
  {
    clearerr(stream);
    fseek(stream, fpos + json_tokener_get_parse_end(jtok), SEEK_SET);
  }
  json_tokener_free(jtok);

  if (jerr == json_tokener_success)
  {
    eth_t ret = to_eth(json);
    json_object_put(json);
    eth_return(args, ret);
  }
  else
  {
    eth_t err = eth_str(json_tokener_error_desc(jerr));
    eth_throw(args, err);
  }
}

static eth_t
dump(void)
{
  eth_args args = eth_start(1);
  eth_t x = eth_arg(args);
  char *errbuf = NULL;
  size_t errbuflen = 0;
  FILE *err = open_memstream(&errbuf, &errbuflen);
  bool haserr = false;
  struct json_object *json = to_json(x, err, &haserr);
  fclose(err);
  if (haserr)
  {
    eth_t what = eth_str(errbuf);
    if (errbuf)
      free(errbuf);
    json_object_put(json);
    eth_throw(args, what);
  }
  else
  {
    if (errbuf)
      free(errbuf);
    size_t reslen;
    const char *resstr = json_object_to_json_string_length(json, 0, &reslen);
    eth_t res = eth_create_string2(resstr, reslen);
    json_object_put(json);
    eth_return(args, res);
  }
}


int
ether_module(eth_module *mod, eth_root *toplvl)
{
  eth_define(mod, "to_json", to_json_method = eth_create_method(1, NULL));
  eth_define(mod, "parse_string", eth_proc(parse_string, 1));
  eth_define(mod, "parse_stream", eth_proc(parse_stream, 1));
  eth_define(mod, "dump", eth_proc(dump, 1));
  return 0;
}

