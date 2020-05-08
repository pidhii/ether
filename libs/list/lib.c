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
    return eth_exn(eth_sym("Invalid_argument"));
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
    return eth_exn(eth_sym("Invalid_argument"));
  }
  eth_ref(acc);
  eth_unref(l);
  eth_dec(acc);
  return acc;
}

static eth_t
_rev_map(void)
{
  eth_use_symbol(Improper_list)

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
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_mapi(void)
{
  eth_use_symbol(Improper_list)

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
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_zip(void)
{
  eth_use_symbol(Improper_list)

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
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_zipi(void)
{
  eth_use_symbol(Improper_list)

  eth_args args = eth_start(3);
  const eth_t f = eth_arg2(args, eth_function_type);
  const eth_t xs = eth_arg(args);
  const eth_t ys = eth_arg(args);

  eth_t acc = eth_nil;
  eth_t it1, it2;
  eth_number_t i = 0;
  for (it1 = xs, it2 = ys; eth_is_pair(it1) and eth_is_pair(it2);
      it1 = eth_cdr(it1), it2 = eth_cdr(it2), ++i)
  {
    eth_reserve_stack(3);
    eth_sp[0] = eth_num(i);
    eth_sp[1] = eth_car(it1);
    eth_sp[2] = eth_car(it2);
    const eth_t v = eth_apply(f, 3);
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
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

static eth_t
_rev_filter_map(void)
{
  eth_use_symbol(Filter_out)
  eth_use_symbol(Improper_list)

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
    if (eth_is_exn(v) && eth_what(v) == Filter_out)
      eth_drop(v);
    else
      acc = eth_cons(v, acc);
  }
  if (eth_unlikely(it != eth_nil))
  {
    eth_drop(acc);
    eth_throw(args, Improper_list);
  }

  eth_return(args, acc);
}

int
ether_module(eth_module *mod, eth_env *topenv)
{
  eth_define(mod, "len", eth_create_proc(_length, 1, NULL, NULL));
  eth_define(mod, "rev_append", eth_create_proc(_rev_append, 2, NULL, NULL));
  eth_define(mod, "rev_map", eth_create_proc(_rev_map, 2, NULL, NULL));
  eth_define(mod, "rev_mapi", eth_create_proc(_rev_mapi, 2, NULL, NULL));
  eth_define(mod, "rev_zip", eth_create_proc(_rev_zip, 3, NULL, NULL));
  eth_define(mod, "rev_zipi", eth_create_proc(_rev_zipi, 3, NULL, NULL));
  eth_define(mod, "rev_filter_map", eth_create_proc(_rev_filter_map, 2, NULL, NULL));

  if (not eth_load_module_from_script2(topenv, NULL, mod, "lib.eth", NULL, mod))
    return -1;
  return 0;
}
