#include "ether/ether.hpp"


eth_type *eth::user_data_type;

eth::value
eth::user_data(void *data)
{
  detail::_user_data_wrapper *ud = new detail::_user_data_wrapper;
  eth_init_header(ud, user_data_type);
  ud->dtor = detail::_default_dtor;
  return value {ETH(ud)};
}

void*
eth::value::udata() const
{
  if (m_ptr->type != user_data_type)
    throw type_exn {"not a user-data"};
  return reinterpret_cast<detail::_user_data_wrapper*>(m_ptr)->data;
}

void*
eth::value::drain_udata()
{
  if (m_ptr->type != user_data_type)
    throw type_exn {"not a user-data"};
  detail::_user_data_wrapper *ud =
    reinterpret_cast<detail::_user_data_wrapper*>(m_ptr);
  ud->dtor = detail::_default_dtor;
  void *data = ud->data;
  ud->data = nullptr;
  return data;
}


static void
_user_data_destroy(eth_type*, eth_t x)
{
  eth::detail::_user_data_wrapper *ud =
    reinterpret_cast<eth::detail::_user_data_wrapper*>(x);
  ud->dtor(ud->data);
  delete ud;
}

static void
_init_user_data_type()
{
  eth::user_data_type = eth_create_type("user-data");
  eth::user_data_type->destroy = _user_data_destroy;
}

static void
_cleanup_user_data_type()
{ eth_destroy_type(eth::user_data_type); }


static bool s_initialized = false;

void
eth::init(const int *argc)
{
  if (not s_initialized)
  {
    eth_init(argc);
    _init_user_data_type();
  }
  s_initialized = true;
}

void
eth::cleanup()
{
  if (s_initialized)
  {
    eth_cleanup();
    _cleanup_user_data_type();
  }
  s_initialized = false;
}

