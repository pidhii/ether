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
#include <ether/ether.h>

#include <errno.h>
#include <stdlib.h>


ETH_MODULE("ether:io");


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
      return eth_exn(eth_sym("end_of_file"));
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
  eth_drop(file);
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
      return eth_exn(eth_sym("end_of_file"));
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

#define DEF_READ_NUM(NAME, TYPE)                       \
  static eth_t                                         \
  _read_##NAME##_of(void)                              \
  {                                                    \
    eth_use_symbol(End_of_file);                       \
    eth_use_symbol(Failure);                           \
                                                       \
    eth_args args = eth_start(1);                      \
    eth_t file = eth_arg2(args, eth_file_type);        \
    FILE *stream = eth_get_file_stream(file);          \
    TYPE buf;                                          \
    size_t nrd = fread(&buf, sizeof(TYPE), 1, stream); \
    if (nrd == 0)                                      \
    {                                                  \
      if (feof(stream))                                \
        eth_throw(args, End_of_file);                  \
      else if (ferror(stream))                         \
        eth_throw(args, eth_system_error(0));          \
    }                                                  \
    else if (nrd != 1)                                 \
    {                                                  \
      eth_throw(args, Failure);                        \
    }                                                  \
    eth_return(args, eth_num(buf));                    \
  }

#define DEF_WRITE_NUM(NAME, TYPE)                     \
  static eth_t                                        \
  _write_##NAME##_to(void)                            \
  {                                                   \
    eth_args args = eth_start(2);                     \
    eth_t file = eth_arg2(args, eth_file_type);       \
    eth_t num = eth_arg2(args, eth_number_type);      \
    FILE *stream = eth_get_file_stream(file);         \
    TYPE buf = eth_num_val(num);                      \
    clearerr(stream);                                 \
    errno = 0;                                        \
    size_t nwr = fwrite(&buf, sizeof buf, 1, stream); \
    if (ferror(stream))                               \
    {                                                 \
      switch (errno)                                  \
      {                                               \
        case EINVAL:                                  \
        case EBADF:                                   \
          eth_throw(args, eth_invalid_argument());    \
      }                                               \
      eth_throw(args, eth_system_error(0));           \
    }                                                 \
    eth_return(args, eth_nil);                        \
  }

#define DEF_READ_WRITE_NUM(NAME, TYPE) \
  DEF_READ_NUM(NAME, TYPE) \
  DEF_WRITE_NUM(NAME, TYPE)

DEF_READ_WRITE_NUM(int8, int8_t);
DEF_READ_WRITE_NUM(int16, int16_t);
DEF_READ_WRITE_NUM(int32, int32_t);
DEF_READ_WRITE_NUM(int64, int64_t);
DEF_READ_WRITE_NUM(uint8, uint8_t);
DEF_READ_WRITE_NUM(uint16, uint16_t);
DEF_READ_WRITE_NUM(uint32, uint32_t);
DEF_READ_WRITE_NUM(uint64, uint64_t);
DEF_READ_WRITE_NUM(float, float);
DEF_READ_WRITE_NUM(double, double);


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
      return eth_exn(eth_sym("end_of_file"));
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
    eth_throw(args, eth_sym("format_error"));

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
    eth_throw(args, eth_system_error(0));
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
        eth_return(args, eth_exn(eth_sym("system_error")));
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
        eth_return(args, eth_exn(eth_sym("system_error")));
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
ether_module(eth_module *mod, eth_root *toplvl)
{
  eth_define(mod, "read_line_of", eth_proc(_read_line_of, 1));
  eth_define(mod, "read_of", eth_proc(_read_of, 2));
  eth_define(mod, "read_i8_of", eth_proc(_read_int8_of, 1));
  eth_define(mod, "read_i16_of", eth_proc(_read_int16_of, 1));
  eth_define(mod, "read_i32_of", eth_proc(_read_int32_of, 1));
  eth_define(mod, "read_i64_of", eth_proc(_read_int64_of, 1));
  eth_define(mod, "read_u8_of", eth_proc(_read_uint8_of, 1));
  eth_define(mod, "read_u16_of", eth_proc(_read_uint16_of, 1));
  eth_define(mod, "read_u32_of", eth_proc(_read_uint32_of, 1));
  eth_define(mod, "read_u64_of", eth_proc(_read_uint64_of, 1));
  eth_define(mod, "read_f32_of", eth_proc(_read_float_of, 1));
  eth_define(mod, "read_f64_of", eth_proc(_read_double_of, 1));
  eth_define(mod, "write_i8_to", eth_proc(_write_int8_to, 2));
  eth_define(mod, "write_i26_to", eth_proc(_write_int16_to, 2));
  eth_define(mod, "write_i32_to", eth_proc(_write_int32_to, 2));
  eth_define(mod, "write_i64_to", eth_proc(_write_int64_to, 2));
  eth_define(mod, "write_u8_to", eth_proc(_write_uint8_to, 2));
  eth_define(mod, "write_u26_to", eth_proc(_write_uint16_to, 2));
  eth_define(mod, "write_u32_to", eth_proc(_write_uint32_to, 2));
  eth_define(mod, "write_u64_to", eth_proc(_write_uint64_to, 2));
  eth_define(mod, "write_f32_to", eth_proc(_write_float_to, 2));
  eth_define(mod, "write_f64_to", eth_proc(_write_double_to, 2));
  eth_define(mod, "read_file", eth_proc(_read_file, 1));
  eth_define(mod, "__printf", eth_proc(_printf, 2));
  eth_define(mod, "write_to", eth_proc(_write_to, 2));
  eth_define(mod, "tell", eth_proc(_tell, 1));
  eth_define(mod, "__seek", eth_proc(_seek, 3));
  eth_define(mod, "flush", eth_proc(_flush, 1));

  eth_module *aux = eth_load_module_from_script2(toplvl, "./lib.eth", NULL, mod);
  if (not aux)
    return -1;
  eth_copy_defs(aux, mod);
  eth_destroy_module(aux);
  return 0;
}
