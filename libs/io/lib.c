#include <ether/ether.h>

#include <errno.h>
#include <stdlib.h>

static eth_t
_read_line_of(void)
{
  eth_t file = *eth_sp++;
  if (file->type != eth_file_type)
  {
    eth_drop(file);
    return eth_exn(eth_type_error());
  }

  char *line = NULL;
  size_t n = 0;
  ssize_t nrd = getline(&line, &n, eth_get_file_stream(file));
  int err = errno;

  if (nrd < 0)
  {
    free(line);
    if (feof(eth_get_file_stream(file)))
    {
      eth_drop(file);
      return eth_exn(eth_sym("End_of_file"));
    }
    else
    {
      eth_drop(file);
      switch (err)
      {
        case EBADF:
        case EINVAL: return eth_exn(eth_invalid_argument());
        default: return eth_system_error(err);
      }
    }
  }
  return eth_create_string_from_ptr2(line, nrd);
}

static eth_t
_read_of(void)
{
  eth_t file = *eth_sp++;
  eth_ref(file);

  eth_t n = *eth_sp++;
  eth_ref(n);

  if (file->type != eth_file_type || not eth_is_num(n))
  {
    eth_unref(file);
    eth_unref(n);
    return eth_exn(eth_type_error());
  }
  if (eth_num_val(n) < 0)
  {
    eth_unref(file);
    eth_unref(n);
    return eth_exn(eth_invalid_argument());
  }

  FILE *stream = eth_get_file_stream(file);
  size_t size = eth_num_val(n);
  char *buf = malloc(size + 1);
  size_t nrd = fread(buf, 1, size, stream);
  if (nrd == 0)
  {
    free(buf);
    if (feof(stream))
    {
      eth_unref(file);
      eth_unref(n);
      return eth_exn(eth_sym("End_of_file"));
    }
    else if (ferror(stream))
    {
      eth_unref(file);
      eth_unref(n);
      return eth_exn(eth_system_error(0));
    }
  }
  eth_unref(file);
  eth_unref(n);
  buf[nrd] = '\0';
  return eth_create_string_from_ptr2(buf, nrd);
}

static eth_t
_read_file(void)
{
  eth_t file = *eth_sp++;
  if (file->type != eth_file_type)
  {
    eth_drop(file);
    return eth_exn(eth_type_error());
  }

  FILE *stream = eth_get_file_stream(file);

  errno = 0;
  long start = ftell(stream);
  if (errno) goto error;
  fseek(stream, 0, SEEK_END);
  if (errno) goto error;
  long end = ftell(stream);
  if (errno) goto error;
  fseek(stream, start, SEEK_SET);
  if (errno) goto error;

  char *buf = malloc(end - start + 1);
  size_t nrd = fread(buf, 1, end - start, stream);
  if (nrd == 0)
  {
    if (feof(stream))
    {
      eth_drop(file);
      return eth_exn(eth_sym("End_of_file"));
    }
    else if (ferror(stream))
    {
      eth_drop(file);
      return eth_system_error(0);
    }
  }
  eth_drop(file);
  buf[nrd] = '\0';
  return eth_create_string_from_ptr2(buf, nrd);

error:;
  int err = errno;
  eth_drop(file);
  switch (err)
  {
    case EINVAL:
    case ESPIPE:
    case EBADF:
      return eth_exn(eth_invalid_argument());
    default:
      return eth_system_error(err);
  }
}

typedef struct {
  eth_t file;
  eth_t fmt;
  int n;
} format_data;

static void
destroy_format_data(format_data *data)
{
  if (data->file)
    eth_unref(data->file);
  eth_unref(data->fmt);
  free(data);
}

static eth_t
_printf_aux(void)
{
  eth_use_symbol(Format_error);
  format_data *data = eth_this->proc.data;
  FILE *stream = eth_get_file_stream(data->file);
  char *fmt = ETH_STRING(data->fmt)->cstr;
  int n = data->n;
  /*return _fmt(fmt, n, stream);*/
  for (int i = 0; i < n; eth_ref(eth_sp[i++]));
  int ok = eth_format(stream, fmt, eth_sp, n);
  for (int i = 0; i < n; eth_unref(eth_sp[i++]));
  eth_pop_stack(n);
  return ok ? eth_nil : eth_exn(Format_error);
}

static eth_t
_printf(void)
{
  eth_args args = eth_start(2);
  eth_t file = eth_arg2(args, eth_file_type);
  eth_t fmt = eth_arg2(args, eth_string_type);

  int n = eth_study_format(eth_str_cstr(fmt));
  if (n < 0)
    eth_throw(args, eth_sym("Format_error"));

  if (n == 0)
  {
    fputs(ETH_STRING(fmt)->cstr, eth_get_file_stream(file));
    eth_return(args, eth_nil);
  }
  else
  {
    format_data *data = malloc(sizeof(format_data));
    data->fmt = fmt;
    data->file = file;
    data->n = n;
    eth_pop_stack(2);
    return eth_create_proc(_printf_aux, n, data, (void*)destroy_format_data);
  }
}

static eth_t
_write_to(void)
{
  eth_args args = eth_start(2);
  eth_t file = eth_arg2(args, eth_file_type);
  eth_t str = eth_arg2(args, eth_string_type);
  FILE *stream = eth_get_file_stream(file);
  clearerr(stream);
  errno = 0;
  size_t nwr = fwrite(eth_str_cstr(str), 1, eth_str_len(str), stream);
  if (ferror(stream))
  {
    switch (errno)
    {
      case EINVAL:
      case EBADF:
        eth_throw(args, eth_invalid_argument());
    }
    eth_throw(args, eth_sym("System_error"));
  }
  eth_end_unref(args);
  return eth_nil;
}

static eth_t
_tell(void)
{
  eth_args args = eth_start(1);
  eth_t file = eth_arg2(args, eth_file_type);
  int pos = ftell(eth_get_file_stream(file));
  int err = errno;
  if (pos < 0)
  {
    switch (err)
    {
      case EINVAL:
      case ESPIPE:
      case EBADF:
        eth_return(args, eth_exn(eth_invalid_argument()));
      default:
        eth_return(args, eth_exn(eth_sym("System_error")));
    }
  }
  eth_return(args, eth_num(pos));
}

static eth_t
_seek(void)
{
  eth_args args = eth_start(3);
  eth_t file = eth_arg2(args, eth_file_type);
  eth_t whence = eth_arg2(args, eth_number_type);
  eth_t offs = eth_arg2(args, eth_number_type);
  int ret = fseek(eth_get_file_stream(file), eth_num_val(whence), eth_num_val(offs));
  int err = errno;
  if (ret)
  {
    switch (err)
    {
      case EINVAL:
      case ESPIPE:
      case EBADF:
        eth_return(args, eth_exn(eth_invalid_argument()));
      default:
        eth_return(args, eth_exn(eth_sym("System_error")));
    }
  }
  eth_return(args, eth_nil);
}

static eth_t
_flush(void)
{
  eth_use_variant(System_error);
  eth_args args = eth_start(1);
  eth_t file = eth_arg2(args, eth_file_type);
  if (fflush(eth_get_file_stream(file)) == EOF)
    eth_throw(args, System_error(eth_str(eth_errno_to_str(errno))));
  else
    eth_return(args, eth_nil);
}

int
ether_module(eth_module *mod, eth_env *toplvl)
{
  eth_define(mod, "read_line_of", eth_proc(_read_line_of, 1));
  eth_define(mod, "read_of", eth_proc(_read_of, 2));
  eth_define(mod, "read_file", eth_proc(_read_file, 1));
  eth_define(mod, "__printf", eth_proc(_printf, 2));
  eth_define(mod, "write_to", eth_proc(_write_to, 2));
  eth_define(mod, "tell", eth_proc(_tell, 1));
  eth_define(mod, "__seek", eth_proc(_seek, 3));
  eth_define(mod, "flush", eth_proc(_flush, 1));

  if (not eth_add_module_script(mod, "./lib.eth", toplvl))
    return -1;

  return 0;
}
