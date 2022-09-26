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

ETH_MODULE("vector")

static eth_t
of_list(void)
{
  eth_use_symbol(Improper_list)
  eth_t l = *eth_sp++;
  eth_t vec = eth_create_vector();
  eth_t it;
  for (it = l; eth_is_pair(it); it = eth_cdr(it))
    eth_push_mut(vec, eth_car(it));
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(vec);
    return eth_exn(Improper_list);
  }
  eth_drop(l);
  return vec;
}

static eth_t
len(void)
{
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_vec(x)))
  {
    eth_drop(x);
    return eth_exn(eth_type_error());
  }
  int len = eth_vec_len(x);
  eth_drop(x);
  return eth_num(len);
}

static eth_t
push(void)
{
  eth_t v = *eth_sp++;
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_vec(v)))
  {
    eth_drop_2(v, x);
    return eth_exn(eth_type_error());
  }
  if (v->rc == 0 and v != x)
  {
    eth_push_mut(v, x);
    return v;
  }
  else
    return eth_push_pers(v, x);
}

static eth_t
insert(void)
{
  eth_use_symbol(Range_error);
  eth_t v = *eth_sp++;
  eth_t k = *eth_sp++;
  eth_t x = *eth_sp++;
  if (eth_unlikely(not eth_is_vec(v) or not eth_is_num(k)))
  {
    eth_drop_3(v, k, x);
    return eth_exn(eth_type_error());
  }
  int len = eth_vec_len(v);
  int kval = eth_num_val(k);
  if (eth_unlikely(kval < 0 or kval >= len))
  {
    eth_drop_3(v, k, v);
    return eth_exn(Range_error);
  }
  if (v->rc == 0 and v != x)
  {
    eth_insert_mut(v, kval, x);
    eth_drop(k);
    return v;
  }
  else
  {
    eth_t ret = eth_insert_pers(v, kval, x);
    eth_drop(k);
    return ret;
  }
}

static eth_t
front(void)
{
  eth_use_symbol(Range_error)
  eth_t v = *eth_sp++;
  if (eth_unlikely(not eth_is_vec(v)))
  {
    eth_drop(v);
    return eth_exn(eth_type_error());
  }
  if (eth_vec_len(v) == 0)
  {
    eth_drop(v);
    return eth_exn(Range_error);
  }
  eth_t ret = eth_front(v);
  eth_ref(ret);
  eth_drop(v);
  eth_dec(ret);
  return ret;
}

static eth_t
back(void)
{
  eth_use_symbol(Range_error)
  eth_t v = *eth_sp++;
  if (eth_unlikely(not eth_is_vec(v)))
  {
    eth_drop(v);
    return eth_exn(eth_type_error());
  }
  if (eth_vec_len(v) == 0)
  {
    eth_drop(v);
    return eth_exn(Range_error);
  }
  eth_t ret = eth_back(v);
  eth_ref(ret);
  eth_drop(v);
  eth_dec(ret);
  return ret;
}

static eth_t
get(void)
{
  eth_use_symbol(Range_error)
  eth_t v = *eth_sp++;
  eth_t k = *eth_sp++;
  if (eth_unlikely(not (eth_is_vec(v) and eth_is_num(k))))
  {
    eth_drop_2(v, k);
    return eth_exn(eth_type_error());
  }
  int len = eth_vec_len(v);
  int kval = eth_num_val(k);
  if (eth_unlikely(kval < 0 or kval >= len))
  {
    eth_drop_2(v, k);
    return eth_exn(Range_error);
  }
  eth_t ret = eth_vec_get(v, kval);
  eth_ref(ret);
  eth_drop_2(v, k);
  eth_dec(ret);
  return ret;
}

static eth_t
iter(void)
{
  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t v = eth_arg2(args, eth_vector_type);

  eth_vector_iterator iter;
  eth_vector_begin(v, &iter, 0);
  while (not iter.isend)
  {
    for (eth_t *p = iter.slice.begin; p != iter.slice.end; ++p)
    {
      eth_reserve_stack(1);
      eth_sp[0] = *p;
      eth_t r = eth_apply(f, 1);
      if (eth_unlikely(eth_is_exn(r)))
        eth_rethrow(args, v);
      eth_drop(r);
    }

    eth_vector_next(&iter);
  }

  eth_return(args, eth_nil);
}

static eth_t
iteri(void)
{
  eth_args args = eth_start(2);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t v = eth_arg2(args, eth_vector_type);

  eth_vector_iterator iter;
  eth_vector_begin(v, &iter, 0);
  int i = 0;
  while (not iter.isend)
  {
    for (eth_t *p = iter.slice.begin; p != iter.slice.end; ++p)
    {
      eth_reserve_stack(2);
      eth_sp[0] = eth_num(i);
      eth_sp[1] = *p;
      eth_t r = eth_apply(f, 2);
      if (eth_unlikely(eth_is_exn(r)))
        eth_rethrow(args, v);
      eth_drop(r);

      i++;
    }

    eth_vector_next(&iter);
  }

  eth_return(args, eth_nil);
}


int
ether_module(eth_module *mod, eth_root *topenv)
{
  eth_define(mod, "of_list", eth_create_proc(of_list, 1, NULL, NULL));
  eth_define(mod, "len", eth_create_proc(len, 1, NULL, NULL));
  eth_define(mod, "push", eth_create_proc(push, 2, NULL, NULL));
  eth_define(mod, "insert", eth_create_proc(insert, 3, NULL, NULL));
  eth_define(mod, "front", eth_create_proc(front, 1, NULL, NULL));
  eth_define(mod, "back", eth_create_proc(back, 1, NULL, NULL));
  eth_define(mod, "get", eth_create_proc(get, 2, NULL, NULL));
  eth_define(mod, "iter", eth_create_proc(iter, 2, NULL, NULL));
  eth_define(mod, "iteri", eth_create_proc(iteri, 2, NULL, NULL));

  eth_module *aux = eth_load_module_from_script2(topenv, "./lib.eth", NULL, mod);
  if (not aux)
    return -1;
  eth_copy_defs(aux, mod);
  eth_destroy_module(aux);
  return 0;
}
