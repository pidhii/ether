#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>


#define ETH_NEW_CPU_STACK_NPAG 1

// FIXME stack overflow not detected
eth_state*
eth_create_initial_state(int cpu_stack_npag, eco_entry_point_t entry)
{
  eth_state *s = eth_malloc(sizeof(eth_state));
  memset(s, 0, sizeof(eth_state));

  // new cpu stack
  eco_allocate_guarded_stack(cpu_stack_npag, &s->cpustack);

  // new arg stack
  s->astack = eth_malloc(ETH_STACK_SIZE);
  eth_t *asb = (eth_t*)((uintptr_t)s->astack + ETH_STACK_SIZE);
  asb = (eth_t*)((uintptr_t)asb & ~(sizeof(void*)-1));
  size_t ass = (uintptr_t)asb - (uintptr_t)s->astack;

  // init cpu context
  eco_init(&s->cpustate, entry, s->cpustack.stack, s->cpustack.stack_size);

  // init vm context
  s->vmstate.sb = asb;
  s->vmstate.sp = asb;
  s->vmstate.ss = ass;
  s->vmstate.cpuse = (uintptr_t)s->cpustack.mem_ptr;

  return s;
}

void
eth_destroy_state(eth_state *state)
{
  eco_cleanup(&state->cpustate);
  eco_destroy_guarded_stack(&state->cpustack);
  free(state->astack);
  free(state);
}


static void
_save_vm_state(eth_vm_state *vms)
{
  vms->sp = eth_sp;
  vms->sb = eth_sb;
  vms->ss = eth_ss;
  vms->thiss = eth_this;
  vms->cpuse = eth_cpu_se;
}

static void
_load_vm_state(const eth_vm_state *vms)
{
  eth_sp = vms->sp;
  eth_sb = vms->sb;
  eth_ss = vms->ss;
  eth_this = vms->thiss;
  eth_cpu_se = vms->cpuse;
}

bool
eth_switch_state(eth_state *from, eth_state *to, void *parcel,
                 eth_state **ret_caller, void **ret_parcel)
{
  _save_vm_state(&from->vmstate);
  _load_vm_state(&to->vmstate);

  if (not eco_switch((eco_t*)from, (eco_t*)to, parcel, (eco_t**)ret_caller,
                     ret_parcel))
  { // didn't switch
    // => recover VM state manually
    _load_vm_state(&from->vmstate);
    return false;
  }
  else
  { // returned from other switch
    // => VM state is already set up
    return true;
  }
}

