#ifndef ETHER_ETHER_HPP
#define ETHER_ETHER_HPP

#include "ether/ether.h"

#include <string>
#include <stdexcept>
#include <tuple>
#include <array>
#include <type_traits>

namespace eth {

class exn: public std::exception { };

class runtime_exn: public std::runtime_error {
  public:
  using std::runtime_error::runtime_error;
  using std::runtime_error::what;
};

class logic_exn: public std::logic_error {
  public:
  using std::logic_error::logic_error;
  using std::logic_error::what;
};

class type_exn: public std::runtime_error {
  public:
  using std::runtime_error::runtime_error;
  using std::runtime_error::what;
};

typedef eth_number_t number_t;

void
init(const int *argc);

void
cleanup();

class value;
namespace detail {
  namespace format_proxy {
    struct write { const eth::value &value; };
    struct display { const eth::value &value; };
  } // namespace eth::detail::foramt_proxy
} // namespace eth::detail

class value {
  public:
  explicit
  value(eth_t ptr): m_ptr {ptr} { eth_ref(ptr); }
  value(const value &other): m_ptr {other.m_ptr} { eth_ref(m_ptr); }
  value(value &&other): m_ptr {other.m_ptr} { other.m_ptr = nullptr; }

  value(number_t x): m_ptr {eth_create_number(x)} { }
  value(const char* x): m_ptr {eth_create_string(x)} {}
  value(const std::string &x): m_ptr {eth_create_string2(x.c_str(),x.size())} {}

  value&
  operator = (const value &other)
  {
    eth_t oldptr = m_ptr;
    eth_ref(m_ptr = other.m_ptr);
    eth_unref(oldptr);
    return *this;
  }

  value&
  operator = (value &&other)
  {
    if (m_ptr != other.m_ptr)
    {
      eth_unref(m_ptr);
      m_ptr = other.m_ptr;
      other.m_ptr = nullptr;
    }
    return *this;
  }

  ~value()
  { if (m_ptr) eth_unref(m_ptr); }

  eth_t
  ptr() const noexcept
  { return m_ptr; }

  detail::format_proxy::write
  w() const noexcept
  { return {*this}; }

  detail::format_proxy::display
  d() const noexcept
  { return {*this}; }

  bool is_tuple() const noexcept { return eth_is_tuple(m_ptr->type); }
  bool is_record() const noexcept { return eth_is_record(m_ptr->type); }
  bool is_string() const noexcept { return eth_is_str(m_ptr); }
  bool is_number() const noexcept { return eth_is_num(m_ptr); }
  bool is_pair() const noexcept { return eth_is_pair(m_ptr); }
  bool is_symbol() const noexcept { return m_ptr->type == eth_symbol_type; }
  bool is_function() const noexcept { return m_ptr->type == eth_function_type; }

  value operator [] (size_t i) const;
  value operator [] (const std::string &field) const;

  template <typename ...Args>
  value
  operator () (Args&& ...args)
  {
    if (eth_unlikely(not is_function()))
      throw type_exn {"not a function"};

    constexpr size_t n_args = sizeof...(Args);
    std::array<eth_t, n_args> p {std::forward<Args>(args)...};
    eth_reserve_stack(n_args);
    for (size_t i = 0; i < n_args; ++i)
      eth_sp[i] = p[i];
    return value {eth_apply(m_ptr, n_args)};
  }

  const char*
  str() const
  {
    if (eth_likely(is_string()))
      return eth_str_cstr(m_ptr);
    else
      throw type_exn {"not a string"};
  };

  number_t
  num() const
  {
    if (eth_likely(is_number()))
      return eth_num_val(m_ptr);
    else
      throw type_exn {"not a number"};
  }

  private:
  operator eth_t () const noexcept
  { return m_ptr; }

  eth_t m_ptr;
}; // class eth::value

class sandbox {
  public:
  sandbox();
  sandbox(const std::string &pathroot);
  sandbox(const sandbox &other) = delete;
  sandbox(sandbox &&other) = delete;

  sandbox& operator = (const sandbox &other) = delete;
  sandbox& operator = (sandbox &&other) = delete;

  ~sandbox();

  std::string
  resolve_path(const std::string &path);

  void
  add_module_path(const std::string &path);

  value
  eval(const std::string &str);

  value
  operator () (const std::string &str)
  { return eval(str); }

  value
  operator [] (const std::string &var_name) const;

  void
  define(const std::string &var_name, const value &val);

  private:
  eth_root *m_root;
  eth_module *m_module;
}; // class eth::sandbox

} // namespace eth

std::ostream&
operator << (std::ostream &os, const eth::detail::format_proxy::write &v);

std::ostream&
operator << (std::ostream &os, const eth::detail::format_proxy::display &v);

inline std::ostream&
operator << (std::ostream &os, const eth::value &v)
{ return os << v.d(); }

#endif
