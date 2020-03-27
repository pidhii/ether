#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

eth_ssa_tape*
eth_create_ssa_tape()
{
  eth_ssa_tape *tape = malloc(sizeof(eth_ssa_tape));
  tape->head = tape->point = NULL;
  return tape;
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
    if (where->prev->tag == ETH_INSN_IF)
    {
      if (where->prev->iff.thenbr == where)
        where->prev->iff.thenbr = insn;
      else if (where->prev->iff.elsebr == where)
        where->prev->iff.elsebr = insn;
      else
        where->prev->next = insn;
    }
    else
      where->prev->next = insn;
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

