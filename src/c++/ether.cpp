#include "ether/ether.hpp"

static bool s_initialized = false;

void
eth::init(const int *argc)
{
  if (not s_initialized)
    eth_init(argc);
  s_initialized = true;
}

void
eth::cleanup()
{ eth_cleanup(); }

