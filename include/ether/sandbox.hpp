#ifndef ETHER_SANDBOX_HPP
#define ETHER_SANDBOX_HPP

#include "ether/ether.hpp"


namespace eth {

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

  value
  source(const std::string &path);

  private:
  eth_root *m_root;
  eth_module *m_module;
}; // class eth::sandbox

} // namespace eth

#endif
