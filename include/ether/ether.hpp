#ifndef ETHER_ETHER_HPP
#define ETHER_ETHER_HPP

#include "ether/ether.h"

#include <string>
#include <stdexcept>
#include <tuple>
#include <array>
#include <type_traits>
#include <vector>
#include <functional>


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
init(void *argv);

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

  value(): value(eth_nil) { }
  value(const char* x): value(eth_create_string(x)) {}
  value(const std::string &x): value(eth_create_string2(x.c_str(),x.size())) {}
  value(number_t x) : value(eth_create_number(x)) { }

  template <typename T>
  value(typename std::enable_if<std::is_arithmetic<T>::value, T>::type x)
  : value(eth_create_number(x))
  { }

  static value
  boolean(bool x) { return value {eth_boolean(x)}; }

  value&
  operator = (const value &other);

  value&
  operator = (value &&other);

  ~value() { if (m_ptr) eth_unref(m_ptr); }

  eth_t
  ptr() const noexcept
  { return m_ptr; }

  eth_t
  drain_ptr() noexcept { eth_t ret = m_ptr; m_ptr = nullptr; return ret; }

  detail::format_proxy::write
  w() const noexcept { return {*this}; }

  detail::format_proxy::display
  d() const noexcept { return {*this}; }

  bool is_tuple() const noexcept { return eth_is_tuple(m_ptr->type); }
  bool is_record() const noexcept { return eth_is_record(m_ptr->type); }
  bool is_string() const noexcept { return m_ptr->type == eth_string_type; }
  bool is_number() const noexcept { return m_ptr->type == eth_number_type; }
  bool is_pair() const noexcept { return m_ptr->type == eth_pair_type; }
  bool is_symbol() const noexcept { return m_ptr->type == eth_symbol_type; }
  bool is_function() const noexcept { return m_ptr->type == eth_function_type; }
  bool is_boolean() const noexcept { return m_ptr->type == eth_boolean_type; }
  bool is_dict() const noexcept { return m_ptr->type == eth_rbtree_type; }

  bool is_nil() const noexcept { return m_ptr == eth_nil; }
  bool is_true() const noexcept { return m_ptr == eth_true; }
  bool is_false() const noexcept { return m_ptr == eth_false; }

  explicit
  operator bool () const noexcept { return m_ptr != eth_false; }
  operator number_t () const { return num(); }
  operator std::string () const { return {str()}; }

  value operator [] (const value &k) const;
  value operator [] (int i) const { return (*this)[value {number_t(i)}]; }
  value operator [] (const char *s) const { return (*this)[value {s}]; }

  template <typename ...Args> value
  operator () (Args&& ...args) const;

  const char*
  str() const;

  number_t
  num() const;

  value
  car() const;

  value
  cdr() const;

  void*
  udata() const;

  void*
  drain_udata();

  private:
  operator eth_t () const noexcept { return m_ptr; }

  eth_t m_ptr;
}; // class eth::value

template <typename ...Args> value
value::operator () (Args&& ...args) const
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

inline value
cons(const value &car, const value &cdr)
{ return value {eth_cons(car.ptr(), cdr.ptr())}; }

inline value
list(const value &x)
{
  if (eth_t ret = eth_list(x.ptr()))
    return value {ret};
  else
    throw type_exn {std::string("can't list ") + x.ptr()->type->name};
}

template <typename Container>
value
rev_list(const Container &c)
{
  eth_t acc = eth_nil;
  for (auto it = c.rbegin(); it != c.rend(); ++it)
    acc = eth_cons(it->ptr(), acc);
  return value {acc};
}

template <typename Iterator>
value
rev_list(Iterator begin, Iterator end)
{
  eth_t acc = eth_nil;
  for (auto it = begin; it != end; ++it)
    acc = eth_cons(it->ptr(), acc);
  return value {acc};
}

inline value
nil()
{ return value {eth_nil}; }

inline value
boolean(bool x)
{ return value {eth_boolean(x)}; }

inline value
tuple(const value &a, const value &b)
{ return value {eth_tup2(a.ptr(), b.ptr())}; }

inline value
tuple(const value &a, const value &b, const value &c)
{ return value {eth_tup3(a.ptr(), b.ptr(), c.ptr())}; }

inline value
tuple(const value &a, const value &b, const value &c, const value &d)
{ return value {eth_tup4(a.ptr(), b.ptr(), c.ptr(), d.ptr())}; }

static value
record(std::initializer_list<std::pair<std::string, eth::value>> pairs)
{
  std::vector<const char*> keys;
  std::vector<eth_t> vals;
  for (const auto &pair : pairs)
  {
    keys.push_back(pair.first.c_str());
    vals.emplace_back(pair.second.ptr());
  }
  return value {eth_record(
      const_cast<char* const*>(keys.data()), vals.data(), keys.size()
  )};
}

namespace detail {
  template <size_t Arity, typename Func, typename ...Args>
  typename std::enable_if<sizeof...(Args) == Arity, value>::type
  _apply_function(Func& fn, Args&& ...args)
  { return fn(args...); }

  template <size_t Arity, typename Func, typename ...Args>
  typename std::enable_if<sizeof...(Args) != Arity, value>::type
  _apply_function(Func& fn, Args&& ...args)
  { return _apply_function<Arity>(fn, args..., value(*eth_sp++)); }

  struct _function {
    virtual ~_function() = default;
    virtual value apply() = 0;
  }; // struct eth::detail::_function

  template <size_t Arity, typename Func>
  struct _function_impl: public _function {
    _function_impl(const Func &fn): fn {fn} { }
    value apply() override { return _apply_function<Arity>(fn); }
    Func fn;
  }; // class eth::detail::_function_impl

  static void
  _function_dtor(void *fn)
  { delete static_cast<_function*>(fn); }

  static eth_t
  _function_handle(void)
  {
    try {
      return static_cast<_function*>(eth_this->proc.data)->apply().drain_ptr();
    } catch (const std::exception &exn) {
      return eth_exn(eth_str(exn.what()));
    };
  }
} // namespace eth::detail

template <size_t Arity, typename Func> value
function(Func fn)
{
  using namespace detail;
  const eth_t proc = eth_create_proc(
    _function_handle,
    Arity,
    static_cast<_function*>(new _function_impl<Arity, Func> {fn}),
    _function_dtor
  );
  return value {proc};
}

extern eth_type *user_data_type;

namespace detail {
  struct _user_data_wrapper {
    eth_header header;
    void *data;
    std::function<void(void*)> dtor;
  };

  static void
  _default_dtor(void*)
  { }
} // namespace eth::detail

value
user_data(void *data);

template <typename Dtor> value
user_data(void *data, Dtor dtor)
{
  value x = user_data(data);
  reinterpret_cast<detail::_user_data_wrapper*>(x.ptr())->dtor = dtor;
  return x;
}

} // namespace eth

std::ostream&
operator << (std::ostream &os, const eth::detail::format_proxy::write &v);

std::ostream&
operator << (std::ostream &os, const eth::detail::format_proxy::display &v);

inline std::ostream&
operator << (std::ostream &os, const eth::value &v)
{ return os << v.d(); }

inline eth::value
operator "" _eth(unsigned long long int x)
{ return eth::value {eth_create_number(x)}; }

inline eth::value
operator "" _eth(long double x)
{ return eth::value {eth_create_number(x)}; }

inline eth::value
operator "" _eth(const char *str, size_t len)
{ return eth::value {eth_create_string2(str, len)}; }

#endif
