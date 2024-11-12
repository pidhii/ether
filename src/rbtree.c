#include "ether/ether.h"

#include <codeine/vec.h>

ETH_MODULE("ether:rbtree")

eth_type *eth_rbtree_type;

typedef enum { EMPTY, RED, BLACK } color_t;

typedef struct rbtree {
  eth_header hdr;
  color_t color;
  eth_t value;
  union {
    struct {
      struct rbtree *left, *right;
    };
    struct rbtree *branch[2];
  };
} rbtree;

static inline bool
test_color(rbtree *a, color_t color)
{
  return a ? a->color == color : false;
}

static rbtree *
make_node(color_t color, eth_t x, rbtree *a, rbtree *b)
{
  rbtree *t = eth_alloc(sizeof(rbtree));
  t->color = color;
  if ((t->left = a))
    eth_ref(ETH(a));
  if ((t->right = b))
    eth_ref(ETH(b));
  eth_ref(t->value = x);
  eth_init_header(&t->hdr, eth_rbtree_type);
  return t;
}

static rbtree *
balance(color_t color, eth_t x, rbtree *a, rbtree *b)
{
  if (color == BLACK)
  {
    if (test_color(a, RED))
    {
      if (test_color(a->left, RED))
      {
        rbtree *ret = make_node(
            RED, a->value,
            make_node(BLACK, a->left->value, a->left->left, a->left->right),
            make_node(BLACK, x, a->right, b));
        eth_drop(ETH(a));
        return ret;
      }
      else if (test_color(a->right, RED))
      {
        rbtree *ret =
            make_node(RED, a->right->value,
                      make_node(BLACK, a->value, a->left, a->right->left),
                      make_node(BLACK, x, a->right->right, b));
        eth_drop(ETH(a));
        return ret;
      }
    }
    else if (test_color(b, RED))
    {
      if (test_color(b->left, RED))
      {
        rbtree *ret = make_node(
            RED, b->left->value, make_node(BLACK, x, a, b->left->left),
            make_node(BLACK, b->value, b->left->right, b->right));
        eth_drop(ETH(b));
        return ret;
      }
      else if (test_color(b->right, RED))
      {
        rbtree *ret = make_node(
            RED, b->value, make_node(BLACK, x, a, b->left),
            make_node(BLACK, b->right->value, b->right->left, b->right->right));
        eth_drop(ETH(b));
        return ret;
      }
    }
  }
  return make_node(color, x, a, b);
}

static rbtree *
insert_aux(rbtree *t, eth_t x, int (*compare)(eth_t, eth_t, eth_t *),
           eth_t *exn)
{
  if (t and t->color != EMPTY)
  {
    const int test = compare(x, t->value, exn);
    if (*exn)
      return NULL;
    else if (test > 0)
    {
      rbtree *newright = insert_aux(t->right, x, compare, exn);
      if (*exn)
        return NULL;
      else
        return balance(t->color, t->value, t->left, newright);
    }
    else if (test < 0)
    {
      rbtree *newleft = insert_aux(t->left, x, compare, exn);
      if (*exn)
        return NULL;
      else
        return balance(t->color, t->value, newleft, t->right);
    }
    else
      return make_node(t->color, x, t->left, t->right);
  }
  else
    return make_node(RED, x, NULL, NULL);
}

static rbtree *
insert(rbtree *t, eth_t x, int (*compare)(eth_t, eth_t, eth_t *), eth_t *exn)
{
  rbtree *ret = insert_aux(t, x, compare, exn);
  if (ret)
    ret->color = BLACK;
  return ret;
}

static eth_t
find(rbtree *t, eth_t x, int (*compare)(eth_t, eth_t, eth_t *), eth_t *exn)
{
  if (t and t->color != EMPTY)
  {
    const int test = compare(x, t->value, exn);
    if (*exn)
      return NULL;
    else if (test < 0)
      return find(t->left, x, compare, exn);
    else if (test > 0)
      return find(t->right, x, compare, exn);
    else /* test == 0 */
      return t->value;
  }
  else
    return NULL;
}

static void foreach (rbtree *t, bool (*f)(eth_t x, void *data), void *data,
                     int dir)
{
  if (t and t->color != EMPTY)
  {
    if (f(t->value, data))
    {
      foreach (t->branch[dir % 2], f, data, dir)
        ;
      foreach (t->branch[(dir + 1) % 2], f, data, dir)
        ;
    }
  }
}

// TODO: optimize (dont search for cmp implementation every single time)
static int
compare(eth_t x, eth_t y, eth_t *exn)
{
  eth_use_symbol(cmp_result_error);
  eth_use_symbol(cmp_unimplemented_error);

  if (x->type == y->type)
  {
    eth_t cmp = eth_find_method(x->type->methods, eth_cmp_method);
    if (cmp)
    {
      eth_reserve_stack(2);
      eth_ref(eth_sp[0] = x);
      eth_ref(eth_sp[1] = y);
      eth_t res = eth_apply(cmp, 2);
      eth_dec(x);
      eth_dec(y);
      if (res->type == eth_exception_type)
      {
        *exn = eth_what(res);
        eth_ref(x);
        eth_ref(y);
        eth_ref(*exn);
        eth_drop(res);
        eth_dec(*exn);
        eth_dec(x);
        eth_dec(y);
        return 0;
      }
      else if (res->type != eth_number_type)
      {
        eth_ref(x);
        eth_ref(y);
        eth_drop(res);
        eth_dec(x);
        eth_dec(y);
        *exn = cmp_result_error;
        return 0;
      }
      else
      {
        const int cmpres = (int)eth_num_val(res);
        eth_ref(x);
        eth_ref(y);
        eth_drop(res);
        eth_dec(x);
        eth_dec(y);
        return cmpres;
      }
    }
    else
    {
      *exn = cmp_unimplemented_error;
      return 0;
    }
  }
  else
    return y->type - x->type;
}

static int
tup_compare(eth_t x, eth_t y, eth_t *exn)
{
  return compare(eth_tup_get(x, 0), eth_tup_get(y, 0), exn);
}

struct eth_rbtree_iterator {
  cod_vec(rbtree *) stack;
  int idx;
};

eth_rbtree_iterator *
eth_rbtree_begin(eth_t t)
{
  eth_rbtree_iterator *it = eth_alloc(sizeof(eth_rbtree_iterator));
  cod_vec_init(it->stack);
  if (((rbtree *)t)->color != EMPTY)
    cod_vec_push(it->stack, (rbtree *)t);
  return it;
}

eth_t
eth_rbtree_next(eth_rbtree_iterator *it)
{
  if (it->stack.len == 0)
    return NULL;

  rbtree *node = it->stack.data[it->stack.len - 1];
  eth_t ret = node->value;

  if (node->left)
  {
    cod_vec_push(it->stack, node->left);
    return ret;
  }
  else if (node->right)
  {
    cod_vec_push(it->stack, node->right);
    return ret;
  }
  else
  {
    rbtree *tail = cod_vec_pop(it->stack);
    rbtree *head = it->stack.data[it->stack.len - 1];
    while (it->stack.len > 0)
    {
      if (tail == head->right)
      {
        tail = head;
        head = cod_vec_pop(it->stack);
      }
      else if (head->right)
      {
        cod_vec_push(it->stack, head->right);
        return ret;
      }
      else
        cod_vec_pop(it->stack);
    }
    return NULL;
  }
}

void
eth_rbtree_stop(eth_rbtree_iterator *it)
{
  cod_vec_destroy(it->stack);
  eth_free(it, sizeof(eth_rbtree_iterator));
}

eth_t
eth_rbtree_mfind(eth_t t, eth_t k, eth_t *exn)
{
  eth_t ktup = eth_tup2(k, eth_nil);
  eth_t myexn = NULL;
  eth_t res = find((rbtree *)t, ktup, tup_compare, &myexn);
  eth_ref(k);
  eth_drop(ktup);
  eth_dec(k);
  if (res)
    return res;
  else
  {
    if (exn)
      *exn = myexn;
    return NULL;
  }
}

eth_t
eth_rbtree_sfind(eth_t t, eth_t k, eth_t *exn)
{
  eth_t myexn = NULL;
  eth_t res = find((rbtree *)t, k, compare, &myexn);
  if (res)
    return res;
  else
  {
    if (exn)
      *exn = myexn;
    return NULL;
  }
}

eth_t
eth_rbtree_minsert(eth_t t, eth_t kvtup, eth_t *exn)
{
  eth_t myexn = NULL;
  rbtree *res = insert((rbtree *)t, kvtup, tup_compare, &myexn);
  if (res)
    return ETH(res);
  else
  {
    if (exn)
      *exn = myexn;
    return NULL;
  }
}

eth_t
eth_rbtree_sinsert(eth_t t, eth_t x, eth_t *exn)
{
  eth_t myexn = NULL;
  rbtree *res = insert((rbtree *)t, x, compare, &myexn);
  if (res)
    return ETH(res);
  else
  {
    if (exn)
      *exn = myexn;
    return NULL;
  }
}

eth_t
eth_create_rbtree()
{
  return ETH(make_node(EMPTY, eth_nil, NULL, NULL));
}

void
eth_rbtree_foreach(eth_t t, bool (*f)(eth_t x, void *data), void *data)
{
  foreach ((rbtree *)t, f, data, 0)
    ;
}

void
eth_rbtree_rev_foreach(eth_t t, bool (*f)(eth_t x, void *data), void *data)
{
  foreach ((rbtree *)t, f, data, 1)
    ;
}

static void
destroy(eth_type *__attribute((unused)) type, eth_t x)
{
  rbtree *t = (rbtree *)x;
  if (t->left)
    eth_unref(ETH(t->left));
  if (t->right)
    eth_unref(ETH(t->right));
  eth_unref(t->value);
  eth_free(t, sizeof(rbtree));
}

static eth_t
get_impl(void)
{
  eth_use_symbol(not_found);
  eth_args args = eth_start(2);
  eth_t t = eth_arg2(args, eth_rbtree_type);
  eth_t k = eth_arg(args);
  eth_t exn = NULL;
  eth_t kv = eth_rbtree_mfind(t, k, &exn);
  if (kv)
    eth_return(args, eth_tup_get(kv, 1));
  else if (exn)
    eth_throw(args, eth_exn(exn));
  else
    eth_throw(args, eth_exn(not_found));
}

static eth_t
set_impl(void)
{
  eth_args args = eth_start(3);
  eth_t t = eth_arg2(args, eth_rbtree_type);
  eth_t k = eth_arg(args);
  eth_t v = eth_arg(args);
  eth_t kv = eth_tup2(k, v);
  eth_t exn = NULL;
  eth_t newt = eth_rbtree_minsert(t, kv, &exn);
  if (newt)
    eth_return(args, newt);
  else
  {
    eth_drop(kv);
    eth_throw(args, eth_exn(exn));
  }
}

static void
write(eth_type *__attribute((unused)) type, eth_t x, FILE *os)
{
  fputc('{', os);
  int isnotfirst = 0;
  bool iter(eth_t x, void *)
  {
    if (isnotfirst++)
      fputs(", ", os);
    eth_fprintf(os, "~w: ~w", eth_tup_get(x, 0), eth_tup_get(x, 1));
    return true;
  }
  eth_rbtree_foreach(x, iter, NULL);
  fputc('}', os);
}

ETH_TYPE_CONSTRUCTOR(init_rbtree_type)
{
  eth_rbtree_type = eth_create_type("rbtree");
  eth_rbtree_type->destroy = destroy;
  eth_rbtree_type->write = write;
  eth_add_method(eth_rbtree_type->methods, eth_get_method,
                 eth_proc(get_impl, 2));
  eth_add_method(eth_rbtree_type->methods, eth_set_method,
                 eth_proc(set_impl, 3));
}
