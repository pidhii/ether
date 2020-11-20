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

#include <gtk/gtk.h>

#include <assert.h>


ETH_MODULE("Gtk+ 3");

#define STRINGIFY(x) #x


#define GET_OBJECT(x) (((struct { eth_header _; GObject *obj; } *)(x))->obj)

////////////////////////////////////////////////////////////////////////////////
//                              GObject
////////////////////////////////////////////////////////////////////////////////
typedef struct {
  eth_header header;
  GObject *gobj;
} gobject;
#define GOBJECT(x) ((gobject*)(x))
#define get_gobj(x) (GOBJECT(x)->gobj)

static
eth_type *gobject_type;

static void
destroy_gobject(eth_type *type, eth_t x)
{
  g_object_unref(get_gobj(x));
  free(x);
}

static void
init_gobject(void)
{
  gobject_type = eth_create_type("GObject");
  gobject_type->destroy = destroy_gobject;
}

static eth_t
create_gobject(void *gobj)
{
  assert(gobj);
  gobject *obj = malloc(sizeof(gobject));
  eth_init_header(obj, gobject_type);
  obj->gobj = gobj;
  return ETH(obj);
}

////////////////////////////////////////////////////////////////////////////////
//                              Closure
////////////////////////////////////////////////////////////////////////////////
static void
callback_none(eth_t f)
{
  eth_t ret = eth_apply(f, 0);
  if (eth_is_exn(ret))
  {
    eth_warning("unhandled exception in GtkCallback handle (~w)", ret);
    char buf[PATH_MAX];
    for (int i = ETH_EXCEPTION(ret)->tracelen - 1; i >= 0; --i)
    {
      eth_get_location_file(ETH_EXCEPTION(ret)->trace[i], buf);
      putc('\n', stderr);
      eth_trace("trace[%d]: %s", i, buf);
      if (i == ETH_EXCEPTION(ret)->tracelen - 1)
        eth_print_location_opt(ETH_EXCEPTION(ret)->trace[i], stderr, ETH_LOPT_EXTRALINES);
      else
        eth_print_location_opt(ETH_EXCEPTION(ret)->trace[i], stderr, 0);
    }
  }
  eth_drop(ret);
}

static void
destroy_callback(gpointer data, GClosure *clos)
{
  eth_unref(ETH(data));
}

static GClosure*
create_closure_none(eth_t f)
{
  GClosure *clos = g_cclosure_new_swap(G_CALLBACK(callback_none), f, destroy_callback);
  eth_ref(f);
  return clos;
}


////////////////////////////////////////////////////////////////////////////////
//                            GObject API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_g_signal_connect_closure(void)
{
  eth_use_symbol(Ivalid_callback)

  eth_args args = eth_start(4);
  eth_t wgt = eth_arg2(args, gobject_type);
  eth_t sig = eth_arg2(args, eth_string_type);
  eth_t fun = eth_arg2(args, eth_function_type);
  bool after = eth_arg(args) != eth_false;

  if (ETH_FUNCTION(fun)->arity != 0)
    eth_throw(args, Ivalid_callback);

  GClosure *clos = create_closure_none(fun);
  gulong id = g_signal_connect_closure(GET_OBJECT(wgt), eth_str_cstr(sig), clos, after);
  if (id <= 0)
  {
    g_closure_sink(clos);
    eth_throw(args, eth_failure());
  }

  eth_return(args, eth_num(id));
}

////////////////////////////////////////////////////////////////////////////////
//                          GApplication API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_g_application_run(void)
{
  eth_args arg = eth_start(1);
  eth_t app = eth_arg2(arg, gobject_type);
  char *argv[] = { NULL };
  int status = g_application_run(G_APPLICATION(get_gobj(app)), 0, argv);
  eth_return(arg, eth_num(status));
}

////////////////////////////////////////////////////////////////////////////////
//                               Misc
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_is_container(void)
{
  eth_args arg = eth_start(1);
  eth_t x = eth_arg2(arg, gobject_type);
  eth_t ret = eth_boolean(GTK_IS_CONTAINER(get_gobj(x)));
  eth_return(arg, ret);
}

////////////////////////////////////////////////////////////////////////////////
//                            GtkWidget API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_widget_destroy(void)
{
  eth_args arg = eth_start(1);
  eth_t wgt = eth_arg2(arg, gobject_type);
  gtk_widget_destroy(GTK_WIDGET(get_gobj(wgt)));
  eth_return(arg, eth_nil);
}

static eth_t
_gtk_widget_show_all(void)
{
  eth_args arg = eth_start(1);
  eth_t wgt = eth_arg2(arg, gobject_type);
  gtk_widget_show_all(GTK_WIDGET(get_gobj(wgt)));
  eth_return(arg, eth_nil);
}
static eth_t
_gtk_widget_get_style_context(void)
{
  eth_args arg = eth_start(1);
  eth_t wgt = eth_arg2(arg, gobject_type);
  GtkStyleContext *sty = gtk_widget_get_style_context(GTK_WIDGET(get_gobj(wgt)));
  g_object_ref(sty);
  eth_return(arg, create_gobject(sty));
}

////////////////////////////////////////////////////////////////////////////////
//                          GtkApplication API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_application_new(void)
{
  eth_args arg = eth_start(2);
  eth_t name = eth_arg2(arg, eth_string_type);
  eth_t flag = eth_arg2(arg, eth_number_type);
  GtkApplication *app = gtk_application_new(eth_str_cstr(name), eth_num_val(flag));
  assert(app);
  eth_return(arg, create_gobject(G_OBJECT(app)));
}

static eth_t
_gtk_application_add_window(void)
{
  eth_args arg = eth_start(2);
  eth_t app_ = eth_arg2(arg, gobject_type);
  eth_t win_ = eth_arg2(arg, gobject_type);
  GObject *app = get_gobj(app_);
  GObject *win = get_gobj(win_);
  gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(win));
  eth_return(arg, eth_nil);
}

////////////////////////////////////////////////////////////////////////////////
//                        GtkApplicationWindow API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_application_window_new(void)
{
  eth_args arg = eth_start(1);
  eth_t app = eth_arg2(arg, gobject_type);
  GtkWidget *win = gtk_application_window_new(GTK_APPLICATION(get_gobj(app)));
  g_object_ref_sink(win);
  eth_return(arg, create_gobject(win));
}

////////////////////////////////////////////////////////////////////////////////
//                            GtkWindow API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_window_new(void)
{
  eth_args arg = eth_start(1);
  eth_t type = eth_arg2(arg, eth_number_type);
  GtkWidget *win = gtk_window_new(eth_num_val(type));
  g_object_ref_sink(win);
  eth_return(arg, create_gobject(win));
}

static eth_t
_gtk_window_set_title(void)
{
  eth_args arg = eth_start(2);
  eth_t win = eth_arg2(arg, gobject_type);
  eth_t title = eth_arg2(arg, eth_string_type);
  gtk_window_set_title(GTK_WINDOW(get_gobj(win)), eth_str_cstr(title));
  eth_return(arg, eth_nil);
}

static eth_t
_gtk_window_set_default_size(void)
{
  eth_args arg = eth_start(3);
  eth_t win = eth_arg2(arg, gobject_type);
  eth_t w = eth_arg2(arg, eth_number_type);
  eth_t h = eth_arg2(arg, eth_number_type);
  gtk_window_set_default_size(GTK_WINDOW(get_gobj(win)), eth_num_val(w), eth_num_val(h));
  eth_return(arg, eth_nil);
}

////////////////////////////////////////////////////////////////////////////////
//                            GtkContainer API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_container_add(void)
{
  eth_args arg = eth_start(2);
  eth_t cont = eth_arg2(arg, gobject_type);
  eth_t wgt = eth_arg2(arg, gobject_type);
  gtk_container_add(GTK_CONTAINER(get_gobj(cont)), GTK_WIDGET(get_gobj(wgt)));
  eth_return(arg, eth_nil);
}

static void
_forall_callback(GtkWidget *wgt, eth_t f)
{
  g_object_ref(wgt);
  eth_t w = create_gobject(wgt);
  eth_reserve_stack(1);
  eth_sp[0] = w;
  eth_t ret = eth_apply(f, 1);
  if (eth_is_exn(ret))
    eth_warning("unhandled exception in GtkCallback (~w)", ret);
  eth_drop(ret);
}

static eth_t
_gtk_container_forall(void)
{
  eth_args arg = eth_start(2);
  eth_t cont = eth_arg2(arg, gobject_type);
  eth_t func = eth_arg2(arg, eth_function_type);
  gtk_container_forall(GTK_CONTAINER(get_gobj(cont)), (GtkCallback)_forall_callback, func);
  eth_return(arg, eth_nil);
}

////////////////////////////////////////////////////////////////////////////////
//                              GtkBox API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_box_new(void)
{
  eth_args arg = eth_start(2);
  eth_t ori = eth_arg2(arg, eth_number_type);
  eth_t spc = eth_arg2(arg, eth_number_type);
  GtkWidget *box = gtk_box_new(eth_num_val(ori), eth_num_val(spc));
  eth_return(arg, create_gobject(box));
}

////////////////////////////////////////////////////////////////////////////////
//                            GtkButtonBox API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_button_box_new(void)
{
  eth_args arg = eth_start(1);
  eth_t ori = eth_arg2(arg, eth_number_type);
  GtkWidget *box = gtk_button_box_new(eth_num_val(ori));
  g_object_ref_sink(box);
  eth_return(arg, create_gobject(box));
}

////////////////////////////////////////////////////////////////////////////////
//                             GtkButton API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_button_new_with_label(void)
{
  eth_args arg = eth_start(1);
  eth_t label = eth_arg2(arg, eth_string_type);
  GtkWidget *but = gtk_button_new_with_label(eth_str_cstr(label));
  g_object_ref_sink(but);
  eth_return(arg, create_gobject(but));
}

////////////////////////////////////////////////////////////////////////////////
//                             GtkEntry API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_entry_new(void)
{
  GtkWidget *ent = gtk_entry_new();
  g_object_ref_sink(ent);
  return create_gobject(ent);
}

static eth_t
_gtk_entry_get_text(void)
{
  eth_args arg = eth_start(1);
  eth_t ent = eth_arg2(arg, gobject_type);
  const char *text = gtk_entry_get_text(GTK_ENTRY(get_gobj(ent)));
  if (text == NULL)
    eth_throw(arg, eth_failure());
  guint16 len = gtk_entry_get_text_length(GTK_ENTRY(get_gobj(ent)));
  eth_return(arg, eth_create_string2(text, len));
}

static eth_t
_gtk_entry_set_text(void)
{
  eth_args arg = eth_start(2);
  eth_t ent = eth_arg2(arg, gobject_type);
  eth_t text = eth_arg2(arg, eth_string_type);
  gtk_entry_set_text(GTK_ENTRY(get_gobj(ent)), eth_str_cstr(text));
  eth_return(arg, eth_nil);
}

////////////////////////////////////////////////////////////////////////////////
//                             GtkStack API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_stack_set_visible_child(void)
{
  eth_args arg = eth_start(2);
  eth_t stack = eth_arg2(arg, gobject_type);
  eth_t child = eth_arg2(arg, gobject_type);
  gtk_stack_set_visible_child(GTK_STACK(get_gobj(stack)), GTK_WIDGET(get_gobj(child)));
  eth_return(arg, eth_nil);
}

////////////////////////////////////////////////////////////////////////////////
//                            GtkBuilder API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_builder_new(void)
{
  GtkBuilder *bldr = gtk_builder_new();
  return create_gobject(bldr);
}

static eth_t
_gtk_builder_add_from_file(void)
{
  eth_use_variant_as(_GError, "GError")

  eth_args arg = eth_start(2);
  eth_t bldr_ = eth_arg2(arg, gobject_type);
  eth_t file_ = eth_arg2(arg, eth_string_type);
  GtkBuilder *bldr = (GtkBuilder*)get_gobj(bldr_);
  const char *file = eth_str_cstr(file_);

  GError *error = NULL;
  if (gtk_builder_add_from_file(bldr, file, &error) == 0)
  {
    eth_t err = _GError(eth_str(error->message));
    g_clear_error(&error);
    eth_throw(arg, err);
  }
  eth_return(arg, eth_nil);
}

static eth_t
_gtk_builder_get_object(void)
{
  eth_args arg = eth_start(2);
  eth_t bldr_ = eth_arg2(arg, gobject_type);
  eth_t name_ = eth_arg2(arg, eth_string_type);
  GtkBuilder *bldr = (GtkBuilder*)get_gobj(bldr_);
  const char *name = eth_str_cstr(name_);
  GObject *wgt = gtk_builder_get_object(bldr, name);
  if (wgt == NULL)
    eth_throw(arg, eth_failure());
  g_object_ref(wgt);
  eth_return(arg, create_gobject(wgt));
}

////////////////////////////////////////////////////////////////////////////////
//                          GtkStyleContext API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_style_context_add_provider(void)
{
  eth_args arg = eth_start(3);
  eth_t sty_ = eth_arg2(arg, gobject_type);
  eth_t src_ = eth_arg2(arg, gobject_type);
  eth_t pri_ = eth_arg2(arg, eth_number_type);
  GtkStyleContext *sty = GTK_STYLE_CONTEXT(get_gobj(sty_));
  GtkStyleProvider *src = GTK_STYLE_PROVIDER(get_gobj(src_));
  gint pri = eth_num_val(pri_);
  //gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), src, pri);
  gtk_style_context_add_provider(sty, src, pri);
  eth_return(arg, eth_nil);
}

////////////////////////////////////////////////////////////////////////////////
//                           GtkCssProvider API
////////////////////////////////////////////////////////////////////////////////
static eth_t
_gtk_css_provider_new(void)
{
  GtkCssProvider *css = gtk_css_provider_new();
  return create_gobject(css);
}

static eth_t
_gtk_css_provider_load_from_path(void)
{
  eth_use_variant_as(_GError, "GError")

  eth_args arg = eth_start(2);
  eth_t css_ = eth_arg2(arg, gobject_type);
  eth_t path_ = eth_arg2(arg, eth_string_type);
  GtkCssProvider *css = GTK_CSS_PROVIDER(get_gobj(css_));
  const char *path = eth_str_cstr(path_);
  GError *error = NULL;
  gtk_css_provider_load_from_path(css, path, &error);
  if (error)
  {
    eth_t err = _GError(eth_str(error->message));
    g_clear_error(&error);
    eth_throw(arg, err);
  }
  eth_return(arg, eth_nil);
}


////////////////////////////////////////////////////////////////////////////////
//                               Main
////////////////////////////////////////////////////////////////////////////////
int
ether_module(eth_module *mod, eth_root *topenv)
{
  init_gobject();
  eth_add_destructor(mod, gobject_type, (void*)eth_destroy_type);

#define DEFNUM(name) eth_define(mod, STRINGIFY(_##name), eth_num(name))
#define DEFFUN(name, arity) \
  eth_define(mod, STRINGIFY(name), eth_create_proc(name, arity, NULL, NULL))

 /*---------*
  | GObject |
  *---------*/
  DEFFUN(_g_signal_connect_closure, 4);

 /*--------------*
  | GApplication |
  *--------------*/
  DEFFUN(_g_application_run, 1);

 /*------*
  | Misc |
  *------*/
  DEFFUN(_gtk_is_container, 1);

 /*----------------*
  | GtkApplication |
  *----------------*/
  DEFNUM(G_APPLICATION_FLAGS_NONE);
  DEFNUM(G_APPLICATION_IS_SERVICE);
  DEFNUM(G_APPLICATION_IS_LAUNCHER);
  DEFNUM(G_APPLICATION_HANDLES_OPEN);
  DEFNUM(G_APPLICATION_HANDLES_COMMAND_LINE);
  DEFNUM(G_APPLICATION_SEND_ENVIRONMENT);
  DEFNUM(G_APPLICATION_NON_UNIQUE);
  DEFNUM(G_APPLICATION_CAN_OVERRIDE_APP_ID);
  DEFNUM(G_APPLICATION_ALLOW_REPLACEMENT);
  DEFNUM(G_APPLICATION_REPLACE);
  DEFFUN(_gtk_application_new, 2);
  DEFFUN(_gtk_application_add_window, 2);

 /*-----------*
  | GtkWidget |
  *-----------*/
  DEFFUN(_gtk_widget_destroy, 1);
  DEFFUN(_gtk_widget_show_all, 1);
  DEFFUN(_gtk_widget_get_style_context, 1);

 /*----------------------*
  | GtkApplicationWindow |
  *----------------------*/
  DEFFUN(_gtk_application_window_new, 1);

 /*-----------*
  | GtkWindow |
  *-----------*/
  DEFNUM(GTK_WINDOW_TOPLEVEL);
  DEFNUM(GTK_WINDOW_POPUP);
  DEFFUN(_gtk_window_new, 1);
  DEFFUN(_gtk_window_set_title, 2);
  DEFFUN(_gtk_window_set_default_size, 3);

 /*-----------------*
  | GTK_ORIENTATION |
  *-----------------*/
  DEFNUM(GTK_ORIENTATION_HORIZONTAL);
  DEFNUM(GTK_ORIENTATION_VERTICAL);

 /*--------------*
  | GtkContainer |
  *--------------*/
  DEFFUN(_gtk_container_add, 2);
  DEFFUN(_gtk_container_forall, 2);

 /*--------*
  | GtkBox |
  *--------*/
  DEFFUN(_gtk_box_new, 2);

 /*--------------*
  | GtkButtonBox |
  *--------------*/
  DEFFUN(_gtk_button_box_new, 1);

 /*-----------*
  | GtkButton |
  *-----------*/
  DEFFUN(_gtk_button_new_with_label, 1);

 /*----------*
  | GtkEntry |
  *----------*/
  DEFFUN(_gtk_entry_new, 0);
  DEFFUN(_gtk_entry_get_text, 1);
  DEFFUN(_gtk_entry_set_text, 2);

 /*----------*
  | GtkStack |
  *----------*/
  DEFFUN(_gtk_stack_set_visible_child, 2);

 /*------------*
  | GtkBuilder |
  *------------*/
  DEFFUN(_gtk_builder_new, 0);
  DEFFUN(_gtk_builder_add_from_file, 2);
  DEFFUN(_gtk_builder_get_object, 2);

 /*-----------------*
  | GtkStyleContext |
  *-----------------*/
  DEFFUN(_gtk_style_context_add_provider, 3);

 /*----------------*
  | GtkCssProvider |
  *----------------*/
  DEFFUN(_gtk_css_provider_new, 0);
  DEFFUN(_gtk_css_provider_load_from_path, 2);


  if (not eth_load_module_from_script2(topenv, NULL, mod, "./lib.eth", NULL, mod))
    return -1;
  return 0;
}
