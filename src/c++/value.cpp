#include "ether/ether.hpp"

#include <cstring>
#include <cstdio>
#include <ostream>
#include <sstream>


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
eth::value::operator [] (const char *field) const
{
  if (not is_record())
    throw type_exn {"not a record"};

  const int idx = eth_get_field_by_id(m_ptr->type,
      eth_get_symbol_id(eth_sym(field)));
  if (idx == m_ptr->type->nfields)
  {
    std::ostringstream what;
    what << "no such field: '" << field << "' in {";
    for (int i = 0; i < m_ptr->type->nfields; ++i)
    {
      if (i > 0)
        what << ", ";
      what << m_ptr->type->fields[i].name;
    }
    what << "}";
    throw logic_exn {what.str()};
  }
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

