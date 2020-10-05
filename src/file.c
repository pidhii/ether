#include "ether/ether.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>


ETH_MODULE("ether:file")

#define OPEN (1 << 0)
#define PIPE (1 << 1)

eth_type *eth_file_type;
eth_t eth_stdin, eth_stdout, eth_stderr;

typedef struct {
  eth_header header;
  FILE *stream;
  int flag;
} file;

extern inline int
eth_is_open(eth_t f)
{
  return ((file*)f)->flag & OPEN;
}

extern inline int
eth_is_pipe(eth_t f)
{
  return ((file*)f)->flag & PIPE;
}

int
eth_close(eth_t x)
{
  file *f = (void*)x;
  if (eth_is_open(x))
  {
    f->flag ^= OPEN;
    if (eth_is_pipe(x))
      return pclose(f->stream);
    else
      return fclose(f->stream);
  }
  return 0;
}

FILE*
eth_get_file_stream(eth_t x)
{
  return ((file*)x)->stream;
}

static void
destroy_file(eth_type *type, eth_t x)
{
  file *f = (void*)x;
  if (eth_is_open(x))
  {
    if (eth_close(x))
    {
      if (eth_is_pipe(x))
        eth_warning("pclose: %s", strerror(errno));
      else
        eth_warning("fclose: %s", strerror(errno));
    }
  }
  free(f);
}

void
_eth_init_file_type(void)
{
  eth_file_type = eth_create_type("file");
  eth_file_type->destroy = destroy_file;

  static file _stdin, _stdout, _stderr;
  eth_stdin  = ETH(&_stdin);
  eth_stdout = ETH(&_stdout);
  eth_stderr = ETH(&_stderr);
  eth_init_header(eth_stdin, eth_file_type);
  eth_init_header(eth_stdout, eth_file_type);
  eth_init_header(eth_stderr, eth_file_type);
  _stdin.stream = stdin;
  _stdin.flag = OPEN;
  _stdout.stream = stdout;
  _stdout.flag = OPEN;
  _stderr.stream = stderr;
  _stderr.flag = OPEN;
  eth_ref(eth_stdin);
  eth_ref(eth_stdout);
  eth_ref(eth_stderr);
}

eth_t
eth_open(const char *path, const char *mod)
{
  FILE *stream = fopen(path, mod);
  if (stream == NULL)
    return NULL;
  file *f = malloc(sizeof(file));
  eth_init_header(f, eth_file_type);
  f->stream = stream;
  f->flag = OPEN;
  return ETH(f);
}

eth_t
eth_open_fd(int fd, const char *mod)
{
  FILE *stream = fdopen(fd, mod);
  if (stream == NULL)
    return NULL;
  file *f = malloc(sizeof(file));
  eth_init_header(f, eth_file_type);
  f->stream = stream;
  f->flag = OPEN;
  return ETH(f);
}

eth_t
eth_open_pipe(const char *command, const char *mod)
{
  FILE *stream = popen(command, mod);
  if (stream == NULL)
    return NULL;
  file *f = malloc(sizeof(file));
  eth_init_header(f, eth_file_type);
  f->stream = stream;
  f->flag = OPEN | PIPE;
  return ETH(f);
}

