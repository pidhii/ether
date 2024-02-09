#include "ether/ether.hpp"

#include <cstring>
#include <cstdio>
#include <ostream>
#include <sstream>

#include <iostream>


eth::value&
eth::value::operator = (const eth::value &other)
{
  eth_t oldptr = m_ptr;
  eth_ref(m_ptr = other.m_ptr);
  eth_unref(oldptr);
  return *this;
}


eth::value&
eth::value::operator = (eth::value &&other)
{
  if (m_ptr != other.m_ptr)
  {
    eth_unref(m_ptr);
    m_ptr = other.m_ptr;
    other.m_ptr = nullptr;
  }
  return *this;
}


const char*
eth::value::str() const
{
  if (eth_likely(is_string()))
    return eth_str_cstr(m_ptr);
  else
    throw type_exn {"not a string"};
};


eth::number_t
eth::value::num() const
{
  if (eth_likely(is_number()))
    return eth_num_val(m_ptr);
  else
    throw type_exn {"not a number"};
}


eth::value
eth::value::car() const
{
  if (eth_likely(is_pair()))
    return value {eth_car(m_ptr)};
  else
    throw type_exn {"not a pair"};
}


eth::value
eth::value::cdr() const
{
  if (eth_likely(is_pair()))
    return value {eth_cdr(m_ptr)};
  else
    throw type_exn {"not a pair"};
}


eth::value
eth::value::operator [] (const eth::value &k) const
{
  if (is_record())
  {
    size_t symid;
    if (k.is_symbol())
      symid = eth_get_symbol_id(k.m_ptr);
    else if (k.is_string())
      symid = eth_get_symbol_id(eth_sym(k.str()));
    else
      throw type_exn {"record must be indexed via symbols or strings"};
    const int idx = eth_get_field_by_id(m_ptr->type, symid);
    if (idx == m_ptr->type->nfields)
    {
      std::ostringstream what;
      what << "no field '" << k.d() << "' in " << d();
      throw logic_exn {what.str()};
    }
    return value {eth_tup_get(m_ptr, idx)};
  }
  else if (is_tuple())
  {
    if (not k.is_number())
      throw type_exn {"tuple must be indexed via numbers"};
    if (k.num() >= eth_struct_size(m_ptr->type))
      throw runtime_exn {"touple index out of bounds"};
    return value {eth_tup_get(m_ptr, size_t(k.num()))};
  }
  else if (is_dict())
  {
    eth_t exn = nullptr;
    if (eth_t ret = eth_rbtree_mfind(m_ptr, k.m_ptr, &exn))
      return value {eth_tup_get(ret, 1)};
    else
    {
      std::ostringstream what;
      what << "failed to index dictionary: " << value(exn).d();
      eth_drop(exn);
      throw runtime_exn {what.str()};
    }
    throw logic_exn {"unimplemented"};
  }
  else if (const eth_t m = eth_find_method(m_ptr->type->methods, eth_get_method))
  {
    eth_reserve_stack(2);
    eth_sp[0] = m_ptr;
    eth_sp[1] = k.m_ptr;
    const eth_t ret = eth_apply(m, 2);
    if (ret->type == eth_exception_type)
    {
      std::ostringstream what;
      what << value(ret).d();
      eth_drop(ret);
      throw runtime_exn {what.str()};
    }
    return value {ret};
  }

  throw type_exn {"can't index object of type " + std::string(m_ptr->type->name)};
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

