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

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//                                  lists
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_length(void)
{
  eth_t l = *eth_sp++;
  bool isproper;
  size_t len = eth_length(l, &isproper);
  eth_drop(l);
  if (isproper)
    return eth_num(len);
  else
    return eth_exn(eth_invalid_argument());
}

static eth_t
_rev_append(void)
{
  eth_t l = *eth_sp++;
  eth_ref(l);

  eth_t acc = *eth_sp++;

  eth_t it = l;
  for (; it->type == eth_pair_type; it = eth_cdr(it))
    acc = eth_cons(eth_car(it), acc);

  if (it != eth_nil)
  {
    eth_drop(acc);
    eth_unref(l);
    return eth_exn(eth_invalid_argument());
  }
  eth_ref(acc);
  eth_unref(l);
  eth_dec(acc);
  return acc;
}

static eth_t
_rev_map(void)
{
  eth_use_symbol(improper_list)

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t l = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it;
  for (it = l; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_reserve_stack(1);
    eth_sp[0] = eth_car(it);
    const eth_t v = eth_apply(f, 1);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_mapi(void)
{
  eth_use_symbol(improper_list)

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t l = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it;
  eth_number_t i = 0;
  for (it = l; eth_is_pair(it); it = eth_cdr(it), ++i)
  {
    eth_reserve_stack(2);
    eth_sp[0] = eth_num(i);
    eth_sp[1] = eth_car(it);
    const eth_t v = eth_apply(f, 2);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_map2(void)
{
  eth_use_symbol(improper_list)

  eth_args args = eth_start(3);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t xs = eth_arg(args);
  const eth_t ys = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it1, it2;
  for (it1 = xs, it2 = ys; eth_is_pair(it1) and eth_is_pair(it2);
      it1 = eth_cdr(it1), it2 = eth_cdr(it2))
  {
    eth_reserve_stack(2);
    eth_sp[0] = eth_car(it1);
    eth_sp[1] = eth_car(it2);
    const eth_t v = eth_apply(f, 2);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    acc = eth_cons(v, acc);
  }
  if (eth_unlikely(not eth_is_pair(it1) and it1 != eth_nil or
                   not eth_is_pair(it2) and it2 != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_zip(void)
{
  eth_use_symbol(improper_list);

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t ltup = eth_arg(args);

  if (eth_unlikely(not eth_is_tuple(ltup->type)))
    eth_throw(args, eth_type_error());

  const int n = eth_tuple_size(ltup->type);
  eth_t it[n];
  for (int i = 0; i < n; ++i)
  {
    it[i] = eth_tup_get(ltup, i);
    if (eth_unlikely(not eth_is_pair(it[i])))
    {
      if (it[i] == eth_nil)
        eth_return(args, eth_nil);
      else
        eth_throw(args, eth_invalid_argument());
    }
  }

  eth_t acc = eth_nil;
  while (true)
  {
    // push arguments
    eth_reserve_stack(n);
    for (register int i = 0; i < n; ++i)
      eth_sp[i] = eth_car(it[i]);
    // apply
    const eth_t v = eth_apply(f, n);
    if (eth_unlikely(eth_is_exn(v)))
    {
      eth_drop(acc);
      eth_rethrow(args, v);
    }
    // prepend to return-value
    acc = eth_cons(v, acc);
    // incerement iterators
    for (int i = 0; i < n; ++i)
    {
      it[i] = eth_cdr(it[i]);
      if (eth_unlikely(not eth_is_pair(it[i])))
      {
        if (eth_unlikely(it[i] != eth_nil))
        {
          eth_drop(acc);
          eth_throw(args, improper_list);
        }
        goto end_loop;
      }
    }
  }
  end_loop:
  eth_return(args, acc);
}

static eth_t
_fold_zip(void)
{
  eth_use_symbol(improper_list);

  eth_args args = eth_start(3);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t z = eth_arg(args);
  const eth_t ltup = eth_arg(args);

  if (eth_unlikely(not eth_is_tuple(ltup->type)))
    eth_throw(args, eth_type_error());

  const int n = eth_tuple_size(ltup->type);
  eth_t it[n];
  for (int i = 0; i < n; ++i)
  {
    it[i] = eth_tup_get(ltup, i);
    if (eth_unlikely(not eth_is_pair(it[i])))
    {
      if (it[i] == eth_nil)
        eth_return(args, eth_nil);
      else
        eth_throw(args, eth_invalid_argument());
    }
  }

  eth_t acc = z;
  while (true)
  {
    // push arguments
    eth_reserve_stack(n + 1);
    eth_sp[0] = acc;
    for (register int i = 0; i < n; ++i)
      eth_sp[i+1] = eth_car(it[i]);
    // apply
    acc = eth_apply(f, n + 1);
    if (eth_unlikely(eth_is_exn(acc)))
      eth_rethrow(args, acc);
    // incerement iterators
    for (int i = 0; i < n; ++i)
    {
      it[i] = eth_cdr(it[i]);
      if (eth_unlikely(not eth_is_pair(it[i])))
      {
        if (eth_unlikely(it[i] != eth_nil))
        {
          eth_drop(acc);
          eth_throw(args, improper_list);
        }
        goto end_loop;
      }
    }
  }
  end_loop:
  eth_return(args, acc);
}

static eth_t
_rev_filter_map(void)
{
  eth_use_symbol(filter_out)
  eth_use_symbol(improper_list)

  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t l = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it;
  for (it = l; eth_is_pair(it); it = eth_cdr(it))
  {
    eth_reserve_stack(1);
    eth_sp[0] = eth_car(it);
    const eth_t v = eth_apply(f, 1);
    if (eth_is_exn(v))
    {
      if (eth_what(v) == filter_out)
        eth_drop(v);
      else
      {
        eth_drop(acc);
        eth_rethrow(args, v);
      }
    }
    else
      acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, improper_list);
  }

  eth_return(args, acc);
}

int
ether_module(eth_module *mod, eth_root *topenv)
{
  eth_define(mod, "__len", eth_create_proc(_length, 1, NULL, NULL));
  eth_define(mod, "__rev_append", eth_create_proc(_rev_append, 2, NULL, NULL));
  eth_define(mod, "__rev_map", eth_create_proc(_rev_map, 2, NULL, NULL));
  eth_define(mod, "__rev_mapi", eth_create_proc(_rev_mapi, 2, NULL, NULL));
  eth_define(mod, "__rev_map2", eth_create_proc(_rev_map2, 3, NULL, NULL));
  eth_define(mod, "__rev_zip", eth_create_proc(_rev_zip, 2, NULL, NULL));
  eth_define(mod, "__fold_zip", eth_create_proc(_fold_zip, 3, NULL, NULL));
  eth_define(mod, "__rev_filter_map", eth_create_proc(_rev_filter_map, 2, NULL, NULL));

  eth_module *ethmod = eth_load_module_from_script2(topenv, "lib.eth", NULL, mod);
  if (not ethmod)
    return -1;
  eth_copy_defs(ethmod, mod);
  eth_destroy_module(ethmod);

  return 0;
}
