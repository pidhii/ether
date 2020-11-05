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

eth_ssa_tape*
eth_create_ssa_tape_at(eth_insn *at)
{
  eth_ssa_tape *tape = malloc(sizeof(eth_ssa_tape));
  tape->head = tape->point = at;
  return tape;
}

eth_ssa_tape*
eth_create_ssa_tape()
{
  return eth_create_ssa_tape_at(NULL);
}

void
eth_destroy_ssa_tape(eth_ssa_tape *tape)
{
  free(tape);
}

void
eth_insert_insn_after(eth_insn *where, eth_insn *insn)
{
  assert(!insn->prev && !insn->next);
  insn->prev = where;
  insn->next = where->next;
  if (where->next)
    where->next->prev = insn;
  where->next = insn;
}

void
eth_insert_insn_before(eth_insn *where, eth_insn *insn)
{
  assert(!insn->prev && !insn->next);

  if (where->flag & ETH_IFLAG_NOBEFORE)
    return eth_insert_insn_after(where, insn);

  insn->next = where;
  insn->prev = where->prev;
  if (where->prev)
  {
    switch (where->prev->tag)
    {
      case ETH_INSN_IF:
        if (where->prev->iff.thenbr == where)
          where->prev->iff.thenbr = insn;
        else if (where->prev->iff.elsebr == where)
          where->prev->iff.elsebr = insn;
        else
          where->prev->next = insn;
        break;

      case ETH_INSN_TRY:
        if (where->prev->try.trybr == where)
          where->prev->try.trybr = insn;
        else if (where->prev->try.catchbr == where)
          where->prev->try.catchbr = insn;
        else
          where->prev->next = insn;
        break;

      default:
        where->prev->next = insn;
    }
  }
  where->prev = insn;
}

void
eth_write_insn(eth_ssa_tape *tape, eth_insn *insn)
{
  assert(!insn->prev && !insn->next);
  if (tape->point)
    eth_insert_insn_after(tape->point, insn);
  else
    tape->head = insn;
  tape->point = insn;
}

