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

static inline eth_var*
create_var(eth_var_cfg cfg)
{
  eth_var *var = calloc(1, sizeof(eth_var));
  var->ident = strdup(cfg.ident);
  if ((var->cval = cfg.cval))
    eth_ref(var->cval);
  var->vid = cfg.vid;
  if ((var->attr = cfg.attr))
    eth_ref_attr(var->attr);
  return var;
}

static inline void
destroy_var(eth_var *var)
{
  free(var->ident);
  if (var->cval)
    eth_unref(var->cval);
  if (var->attr)
    eth_unref_attr(var->attr);
  free(var);
}

eth_var_list*
eth_create_var_list(void)
{
  eth_var_list *lst = malloc(sizeof(eth_var_list));
  lst->len = 0;
  lst->head = NULL;
  return lst;
}

void
eth_destroy_var_list(eth_var_list *lst)
{
  eth_var *head = lst->head;
  while (head)
  {
    eth_var *tmp = head->next;
    destroy_var(head);
    head = tmp;
  }
  free(lst);
}

eth_var*
eth_prepend_var(eth_var_list *lst, eth_var_cfg cfg)
{
  eth_var *var = create_var(cfg);
  var->next = lst->head;
  lst->head = var;
  lst->len += 1;
  return var;
}

eth_var*
eth_append_var(eth_var_list *lst, eth_var_cfg cfg)
{
  eth_var *var = create_var(cfg);
  if (lst->head == NULL)
  {
    lst->head = var;
  }
  else
  {
    eth_var *it = lst->head;
    while (it->next)
      it = it->next;
    it->next = var;
  }
  lst->len += 1;
  return var;
}

void
eth_pop_var(eth_var_list *lst, int n)
{
  eth_var *head = lst->head;
  assert(n <= lst->len);
  for (int i = 0; i < n; ++i)
  {
    eth_var *tmp = head->next;
    destroy_var(head);
    head = tmp;
  }
  lst->head = head;
  lst->len -= n;
}

eth_var*
eth_find_var(eth_var *head, const char *ident, int *cnt)
{
  int i = 0;
  while (head && strcmp(head->ident, ident))
  {
    head = head->next;
    i += 1;
  }
  if (cnt) *cnt = i;
  return head;
}
