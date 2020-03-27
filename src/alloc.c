#include "ether/ether.h"

/*#define ETH_DEBUG_MODE*/

#if defined(ETH_DEBUG_MODE)
# warning Using malloc instead of uniform allocators.
# define ALLOCATOR(n)                                  \
  static                                               \
  struct cod_ualloc_h##n g_allocator_h##n;             \
                                                       \
  void*                                                \
  eth_alloc_h##n()                                     \
  {                                                    \
    return malloc(sizeof(struct h##n));                \
  }                                                    \
                                                       \
  void                                                 \
  eth_free_h##n(void *ptr)                             \
  {                                                    \
    free(ptr);                                         \
  }
#else
# define ALLOCATOR(n)                                  \
  static                                               \
  struct cod_ualloc_h##n g_allocator_h##n;             \
                                                       \
  void* __attribute__((hot, flatten))                  \
  eth_alloc_h##n()                                     \
  {                                                    \
    return cod_ualloc_h##n##_alloc(&g_allocator_h##n); \
  }                                                    \
                                                       \
  void __attribute__((hot, flatten))                   \
  eth_free_h##n(void *ptr)                             \
  {                                                    \
    cod_ualloc_h##n##_free(&g_allocator_h##n, ptr);    \
  }
#endif

struct h2 { eth_header hdr; eth_dword_t data[2]; };
#define UALLOC_NAME h2
#define UALLOC_TYPE struct h2
/*#define UALLOC_POOL_SIZE 0x1000*/
#include "codeine/ualloc.h"
ALLOCATOR(2)

struct h6 { eth_header hdr; eth_dword_t data[6]; };
#define UALLOC_NAME h6
#define UALLOC_TYPE struct h6
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"
ALLOCATOR(6)

void
_eth_init_alloc(void)
{
  cod_ualloc_h2_init(&g_allocator_h2);
  cod_ualloc_h6_init(&g_allocator_h6);
}

void
_eth_cleanup_alloc(void)
{
  cod_ualloc_h2_destroy(&g_allocator_h2);
  cod_ualloc_h6_destroy(&g_allocator_h6);
}

