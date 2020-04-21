#include "ether/ether.h"
#include "error-codes.h"

const char*
eth_errno_to_str(int e)
{
  if (e >= g_ncodes or g_codes[e] == NULL)
    return "UNDEFINED";
  else
    return g_codes[e];
}
