#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

eth_type* eth_exception_type;

static void
destroy_exception(eth_type *type, eth_t x)
{
  eth_unref(ETH_EXCEPTION(x)->what);
  free(x);
}

static void
write_exception(eth_type *type, eth_t x, FILE *out)
{
  eth_fprintf(out, "exception { ~w }", ETH_EXCEPTION(x)->what);
}

static void
display_exception(eth_type *type, eth_t x, FILE *out)
{
  eth_fprintf(out, "exception { ~d }", ETH_EXCEPTION(x)->what);
}

void
_eth_init_exception_type(void)
{
  eth_exception_type = eth_create_type("exception");
  eth_exception_type->destroy = destroy_exception;
  eth_exception_type->write = write_exception;
  eth_exception_type->display = display_exception;
}

eth_t
eth_create_exception(eth_t what)
{
  eth_exception *exn = malloc(sizeof(eth_exception));
  eth_init_header(exn, eth_exception_type);
  eth_ref(exn->what = what);
  return ETH(exn);
}
