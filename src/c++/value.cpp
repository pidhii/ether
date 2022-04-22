#include "ether/ether.hpp"

#include <cstring>
#include <cstdio>
#include <ostream>


eth::value
eth::value::operator [] (size_t i) const
{
  if (not is_tuple())
    throw type_exn {"not a tuple"};
  if (i >= eth_tuple_size(m_ptr->type))
    throw runtime_exn {"touple-index index out of bounds"};
  return value {eth_tup_get(m_ptr, i)};
}

eth::value
eth::value::operator [] (const std::string &field) const
{
  if (not is_record())
    throw type_exn {"not a record"};

  const int idx = eth_get_field_by_id(m_ptr->type,
      eth_get_symbol_id(eth_sym(field.c_str())));
  if (idx == m_ptr->type->nfields)
    throw logic_exn {"no such field"};
  return value {eth_tup_get(m_ptr, idx)};
}

std::ostream&
operator << (std::ostream &os, const eth::detail::format_proxy::write &v)
{
  char *ptr = nullptr;
  size_t size = 0;
  FILE *out = open_memstream(&ptr, &size);
  if (out == nullptr)
  {
    int err = errno;
    std::string msg = "operator << (std::ostream&, const eth::value&): ";
    msg += strerror(err);
    throw eth::runtime_exn {msg};
  }

  eth_write(v.value.ptr(), out);

  fflush(out);
  os << ptr;
  fclose(out);
  free(ptr);

  return os;
}

std::ostream&
operator << (std::ostream &os, const eth::detail::format_proxy::display &v)
{
  char *ptr = nullptr;
  size_t size = 0;
  FILE *out = open_memstream(&ptr, &size);
  if (out == nullptr)
  {
    int err = errno;
    std::string msg = "operator << (std::ostream&, const eth::value&): ";
    msg += strerror(err);
    throw eth::runtime_exn {msg};
  }

  eth_display(v.value.ptr(), out);

  fflush(out);
  os << ptr;
  fclose(out);
  free(ptr);

  return os;
}

