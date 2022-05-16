#include "ether/sandbox.hpp"

#include <cstdio>
#include <cstring>


eth::sandbox::sandbox()
: m_root {eth_create_root()},
  m_module {eth_create_module("<sandbox>", nullptr)}
{ }

eth::sandbox::sandbox(const std::string &pathroot)
: m_root {eth_create_root()},
  m_module {eth_create_module("<sandbox>", pathroot.c_str())}
{ }

eth::sandbox::~sandbox()
{
  eth_destroy_root(m_root);
  eth_destroy_module(m_module);
}

std::string
eth::sandbox::resolve_path(const std::string &path)
{
  char buf[PATH_MAX];
  if (eth_resolve_path(eth_get_root_env(m_root), path.c_str(), buf))
    return buf;
  return "";
}

void
eth::sandbox::add_module_path(const std::string &path)
{
  if (not eth_add_module_path(eth_get_root_env(m_root), path.c_str()))
    throw runtime_exn {"failed to add module-path"};
}

eth::value
eth::sandbox::eval(const std::string &str)
{
  eth_evaluator evl;
  evl.root = m_root;
  evl.mod = m_module;

  char buf[str.size() + 1];
  strcpy(buf, str.c_str());
  FILE *bufstream = fmemopen(buf, str.size(), "r");
  eth_scanner *scan = eth_create_repl_scanner(m_root, bufstream);
  eth_ast *expr = eth_parse_repl(scan);
  eth_destroy_scanner(scan);
  fclose(bufstream);
  if (not expr)
    throw runtime_exn {"parse error"};

  eth_t ret = eth_eval(&evl, expr);
  if (not ret)
    throw runtime_exn {"WTF, eth_eval() returned NULL"};

  eth_ref(ret);
  eth_drop_ast(expr);
  eth_dec(ret);
  return value {ret};
}

eth::value
eth::sandbox::operator [] (const std::string &var_name) const
{
  eth_def *def = eth_find_def(m_module, var_name.c_str());
  if (def == nullptr)
    throw logic_exn {"no such variable"};
  return value {def->val};
}

void
eth::sandbox::define(const std::string &var_name, const value &val)
{ eth_define(m_module, var_name.c_str(), val.ptr()); }

eth::value
eth::sandbox::source(const std::string &path)
{
  const std::string fullpath = resolve_path(path);
  if (fullpath.empty())
    throw runtime_exn {"no such file"};

  eth_t ret;
  eth_module *script =
    eth_load_module_from_script2(m_root, fullpath.c_str(), &ret, m_module);
  if (not script)
    throw runtime_exn {"load-failure"};

  eth_copy_defs(script, m_module);
  eth_destroy_module(script);
  return value {ret};
}

