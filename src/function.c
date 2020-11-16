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
#include <assert.h>

ETH_MODULE("ether:function")

eth_type* eth_function_type;


eth_source*
eth_create_source(eth_ast *ast, eth_ir *ir, eth_ssa *ssa)
{
  eth_source *src = malloc(sizeof(eth_source));
  src->rc = 0;
  eth_ref_ast(src->ast = ast);
  eth_ref_ir(src->ir = ir);
  eth_ref_ssa(src->ssa = ssa);
  return src;
}

static inline void
destroy_source(eth_source *src)
{
  eth_unref_ast(src->ast);
  eth_unref_ir(src->ir);
  eth_unref_ssa(src->ssa);
  free(src);
}

void
eth_ref_source(eth_source *src)
{
  src->rc += 1;
}

void
eth_unref_source(eth_source *src)
{
  if (--src->rc == 0)
    destroy_source(src);
}

void
eth_drop_source(eth_source *src)
{
  if (src->rc == 0)
    destroy_source(src);
}


// TODO: may cause stack overflow during UNREF of captures
static void
function_destroy(eth_type *type, eth_t x)
{
  eth_function *func = ETH_FUNCTION(x);

  if (func->islam)
  {
    if (func->clos.scp)
    { /* Deffer deletion to be hendled by the scope. */
      eth_drop_out(func->clos.scp);
      return;
    }

    for (int i = 0; i < func->clos.ncap; ++i)
      eth_unref(func->clos.cap[i]);
    free(func->clos.cap);
    eth_unref_source(func->clos.src);
    eth_unref_bytecode(func->clos.bc);
  }
  else
  {
    if (func->proc.dtor)
      func->proc.dtor(func->proc.data);
  }
  eth_free_h6(func);
}

void
eth_deactivate_clos(eth_function *func)
{
  // destroy all fields
  for (int i = 0; i < func->clos.ncap; ++i)
  {
    if (func->clos.cap[i]->rc > 0)
      eth_unref(func->clos.cap[i]);
  }
  free(func->clos.cap);
  eth_unref_source(func->clos.src);
  eth_unref_bytecode(func->clos.bc);

  // replace with dummy proc
  func->islam = false;
  func->proc.handle = NULL;
  func->proc.data = NULL;
  func->proc.dtor = NULL;
}

void
_eth_init_function_type(void)
{
  eth_function_type = eth_create_type("function");
  eth_function_type->destroy = function_destroy;
}

static inline eth_function* __attribute__((malloc))
create_function(void)
{
  eth_function *func = eth_alloc_h6();
  eth_init_header(func, eth_function_type);
  return func;
}

eth_t
eth_create_proc(eth_t (*f)(void), int n, void *data, void (*dtor)(void*), ...)
{
  eth_function *func = create_function();
  func->islam = false;
  func->arity = n;
  func->proc.handle = f;
  func->proc.data = data;
  func->proc.dtor = dtor;
  return ETH(func);
}

eth_t
eth_create_clos(eth_source *src, eth_bytecode *bc, eth_t *cap, int ncap, int arity)
{
  eth_function *func = create_function();
  func->islam = true;
  func->arity = arity;
  func->clos.src = src;
  func->clos.bc = bc;
  func->clos.cap = cap;
  func->clos.ncap = ncap;
  func->clos.scp = NULL;
  eth_ref_source(src);
  eth_ref_bytecode(bc);
  return ETH(func);
}

static eth_t
dummy_proc(void)
{
  eth_error("evaluation of uninitalized function");
  abort();
}

eth_t
eth_create_dummy_func(int arity)
{
  return eth_create_proc(dummy_proc, arity, NULL, NULL);
}

void
eth_finalize_clos(eth_function *func, eth_source *src, eth_bytecode *bc,
    eth_t *cap, int ncap, int arity)
{
  assert(arity == func->arity);
  func->islam = true;
  func->arity = arity;
  func->clos.src = src;
  func->clos.bc = bc;
  func->clos.cap = cap;
  func->clos.ncap = ncap;
  func->clos.scp = NULL;
  eth_ref_source(src);
  eth_ref_bytecode(bc);
}


typedef struct {
  eth_t f;
  int n;
  eth_t p[];
} curry_data;

static void
destroy_curried(curry_data *data)
{
  eth_unref(data->f);
  for (int i = 0; i < data->n; ++i)
    eth_unref(data->p[i]);
  size_t datasz = sizeof(curry_data) + sizeof(eth_t) * data->n;
  if (datasz <= ETH_H6_SIZE)
    eth_free_h6(data);
  else
    free(data);
}

static eth_t
curried(void)
{
  curry_data *data = eth_this->proc.data;
  eth_reserve_stack(data->n);
  if (ETH(eth_this)->rc == 0)
  {
    // remove reference from curried arguments
    for (int i = 0; i < data->n; ++i)
    {
      eth_dec(data->p[i]);
      eth_sp[i] = data->p[i];
    }
    data->n = 0; // don't touch them in destructor
  }
  else
  {
    memcpy(eth_sp, data->p, sizeof(eth_t) * data->n);
  }
  return eth_apply(data->f, eth_nargs + data->n);
}

eth_t
_eth_partial_apply(eth_function *fn, int narg)
{
  int arity = fn->arity;

  if (arity < narg)
  {
    for (int i = arity; i < narg; eth_ref(eth_sp[i++]));
    eth_t tmp_f = _eth_raw_apply(ETH(fn), arity);
    narg -= arity;

    if (eth_unlikely(tmp_f->type != eth_function_type))
    {
      eth_ref(tmp_f);
      while (narg--)
        eth_unref(*eth_sp++);
      eth_dec(tmp_f);

      if (tmp_f->type == eth_exception_type)
        return tmp_f;
      else
      {
        eth_drop(tmp_f);
        return eth_exn(eth_sym("Apply_error"));
      }
    }

    for (int i = 0; i < narg; eth_dec(eth_sp[i++]));
    eth_ref(tmp_f);
    eth_t ret = eth_apply(tmp_f, narg);
    eth_ref(ret);
    eth_unref(tmp_f);
    eth_dec(ret);
    return ret;
  }
  else
  {
    size_t datasz = sizeof(curry_data) + sizeof(eth_t) * narg;
    curry_data *data = datasz <= ETH_H6_SIZE ? eth_alloc_h6() : malloc(datasz);
    eth_ref(data->f = ETH(fn));
    data->n = narg;
    for (int i = 0; i < narg; ++i)
      eth_ref(data->p[i] = eth_sp[i]);
    eth_pop_stack(narg);
    return eth_create_proc(curried, arity - narg, data, (void*)destroy_curried);

  }
}

