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

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct h1 { eth_header hdr; eth_dword_t data[1]; };
#define UALLOC_NAME h1
#define UALLOC_TYPE struct h1
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"

static
struct cod_ualloc_h1 g_allocator_h1;

void* __attribute__((flatten))
eth_alloc_h1()
{
  return cod_ualloc_h1_alloc(&g_allocator_h1);
}

void __attribute__((flatten))
eth_free_h1(void *ptr)
{
  cod_ualloc_h1_free(&g_allocator_h1, ptr);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct h2 { eth_header hdr; eth_dword_t data[2]; };
#define UALLOC_NAME h2
#define UALLOC_TYPE struct h2
/*#define UALLOC_POOL_SIZE 0x1000*/
#include "codeine/ualloc.h"
ALLOCATOR(2)

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct h3 { eth_header hdr; eth_dword_t data[3]; };
#define UALLOC_NAME h3
#define UALLOC_TYPE struct h3
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"

static
struct cod_ualloc_h3 g_allocator_h3;

void* __attribute__((flatten))
eth_alloc_h3()
{
  return cod_ualloc_h3_alloc(&g_allocator_h3);
}

void __attribute__((flatten))
eth_free_h3(void *ptr)
{
  cod_ualloc_h3_free(&g_allocator_h3, ptr);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct h4 { eth_header hdr; eth_dword_t data[4]; };
#define UALLOC_NAME h4
#define UALLOC_TYPE struct h4
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"

static
struct cod_ualloc_h4 g_allocator_h4;

void* __attribute__((flatten))
eth_alloc_h4()
{
  return cod_ualloc_h4_alloc(&g_allocator_h4);
}

void __attribute__((flatten))
eth_free_h4(void *ptr)
{
  cod_ualloc_h4_free(&g_allocator_h4, ptr);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct h5 { eth_header hdr; eth_dword_t data[5]; };
#define UALLOC_NAME h5
#define UALLOC_TYPE struct h5
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"

static
struct cod_ualloc_h5 g_allocator_h5;

void* __attribute__((flatten))
eth_alloc_h5()
{
  return cod_ualloc_h5_alloc(&g_allocator_h5);
}

void __attribute__((flatten))
eth_free_h5(void *ptr)
{
  cod_ualloc_h5_free(&g_allocator_h5, ptr);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
struct h6 { eth_header hdr; eth_dword_t data[6]; };
#define UALLOC_NAME h6
#define UALLOC_TYPE struct h6
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"

static
struct cod_ualloc_h6 g_allocator_h6;

void* __attribute__((hot, flatten))
eth_alloc_h6()
{
  return cod_ualloc_h6_alloc(&g_allocator_h6);
}

void __attribute__((hot, flatten))
eth_free_h6(void *ptr)
{
  cod_ualloc_h6_free(&g_allocator_h6, ptr);
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
void
_eth_init_alloc(void)
{
  cod_ualloc_h1_init(&g_allocator_h1);
  cod_ualloc_h2_init(&g_allocator_h2);
  cod_ualloc_h3_init(&g_allocator_h3);
  cod_ualloc_h4_init(&g_allocator_h4);
  cod_ualloc_h5_init(&g_allocator_h5);
  cod_ualloc_h6_init(&g_allocator_h6);
}

void
_eth_cleanup_alloc(void)
{
  cod_ualloc_h1_destroy(&g_allocator_h1);
  cod_ualloc_h2_destroy(&g_allocator_h2);
  cod_ualloc_h3_destroy(&g_allocator_h3);
  cod_ualloc_h4_destroy(&g_allocator_h4);
  cod_ualloc_h5_destroy(&g_allocator_h5);
  cod_ualloc_h6_destroy(&g_allocator_h6);
}

