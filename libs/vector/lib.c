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
  eth_t k = *eth_sp++;
  eth_t v = *eth_sp++;
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

int
ether_module(eth_module *mod, eth_env *topenv)
{
  eth_define(mod, "of_list", eth_create_proc(of_list, 1, NULL, NULL));
  eth_define(mod, "len", eth_create_proc(len, 1, NULL, NULL));
  eth_define(mod, "push", eth_create_proc(push, 2, NULL, NULL));
  eth_define(mod, "insert", eth_create_proc(insert, 3, NULL, NULL));
  eth_define(mod, "front", eth_create_proc(front, 1, NULL, NULL));
  eth_define(mod, "back", eth_create_proc(back, 1, NULL, NULL));
  eth_define(mod, "get", eth_create_proc(get, 2, NULL, NULL));
  return 0;
}
