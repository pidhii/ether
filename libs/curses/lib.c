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
#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <curses.h>

static eth_t
curses_exn(void)
{
  eth_use_symbol(Curses_error);
  return eth_exn(Curses_error);
}

typedef struct {
  eth_header header;
  bool dodel;
  WINDOW *cwin;
} window;

static inline WINDOW*
get_cwin(eth_t x)
{
  return ((window*)(x))->cwin;
}

static
eth_type *window_type;

static eth_t
create_window(WINDOW *cwin)
{
  window *win = malloc(sizeof(window));
  eth_init_header(win, window_type);
  win->cwin = cwin;
  return ETH(win);
}

static void
destroy_window(eth_type *type, eth_t x)
{
  window *win = (void*)x;
  if (win->dodel)
    delwin(win->cwin);
  free(win);
}

static eth_t g_stdscr = NULL;

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_stdscr(void)
{
  if (not g_stdscr)
    return eth_nil;
  return g_stdscr;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_initscr(void)
{
  initscr();
  g_stdscr = create_window(stdscr);
  ((window*)g_stdscr)->dodel = false;
  eth_ref(g_stdscr);
  return g_stdscr;
}

static eth_t
_endwin(void)
{
  endwin();
  eth_unref(g_stdscr);
  g_stdscr = NULL;
  return eth_nil;
}

static eth_t
_isendwin(void)
{
  return eth_boolean(isendwin());
}

static eth_t
_curs_set(void)
{
  eth_t x = *eth_sp++;
  int ret = curs_set(x != eth_false);
  eth_drop(x);
  return eth_num(ret);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_start_color(void)
{
  if (start_color() == ERR)
    return curses_exn();
  return eth_nil;
}

static eth_t
_has_colors(void)
{
  return eth_boolean(has_colors());
}

static eth_t
_wattron(void)
{
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t attr = eth_arg2(args, eth_number_type);
  unsigned int a = eth_num_val(attr);
  int err = wattron(((window*)win)->cwin, a);
  if (err == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_wattroff(void)
{
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t attr = eth_arg2(args, eth_number_type);
  unsigned int a = eth_num_val(attr);
  int err = wattroff(((window*)win)->cwin, a);
  if (err == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_COLOR_PAIR(void)
{
  eth_args args = eth_start(1);
  eth_t pair = eth_arg2(args, eth_number_type);
  unsigned int p = eth_num_val(pair);
  eth_return(args, eth_num(COLOR_PAIR(p)));
}

static eth_t
_init_color(void)
{
  eth_args args = eth_start(4);
  eth_t id = eth_arg2(args, eth_number_type);
  eth_t r = eth_arg2(args, eth_number_type);
  eth_t g = eth_arg2(args, eth_number_type);
  eth_t b = eth_arg2(args, eth_number_type);
  int err = init_color(eth_num_val(id), eth_num_val(r), eth_num_val(g), eth_num_val(b));
  if (err == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}


static eth_t
_init_pair(void)
{
  eth_args args = eth_start(3);
  eth_t id = eth_arg2(args, eth_number_type);
  eth_t fg = eth_arg2(args, eth_number_type);
  eth_t bg = eth_arg2(args, eth_number_type);
  int err = init_pair(eth_num_val(id), eth_num_val(fg), eth_num_val(bg));
  if (err == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_getxy(void)
{
  eth_use_tuple_as(tup2, 2);
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  int x, y;
  getyx(get_cwin(win), y, x);
  eth_t xy = eth_tup2(eth_num(x), eth_num(y));
  eth_return(args, xy);
}

static eth_t
_getparxy(void)
{
  eth_use_tuple_as(tup2, 2);
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  int x, y;
  getparyx(get_cwin(win), y, x);
  eth_t xy = eth_tup2(eth_num(x), eth_num(y));
  eth_return(args, xy);
}

static eth_t
_getbegxy(void)
{
  eth_use_tuple_as(tup2, 2);
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  int x, y;
  getbegyx(get_cwin(win), y, x);
  eth_t xy = eth_tup2(eth_num(x), eth_num(y));
  eth_return(args, xy);
}

static eth_t
_getmaxxy(void)
{
  eth_use_tuple_as(tup2, 2);
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  int x, y;
  getmaxyx(get_cwin(win), y, x);
  eth_t xy = eth_tup2(eth_num(x), eth_num(y));
  eth_return(args, xy);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_werase(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (werase(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_wclear(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (wclear(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_wclrtobot(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (wclrtobot(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_wclrtoeol(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (wclrtoeol(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_mvwgetnstr(void)
{
  eth_use_tuple_as(tup2_type, 2)

  eth_args args = eth_start(3);
  eth_t win = eth_arg2(args, window_type);
  eth_t pos = eth_arg2(args, tup2_type);
  eth_t len = eth_arg2(args, eth_number_type);

  eth_t x = eth_tup_get(pos, 0);
  eth_t y = eth_tup_get(pos, 1);
  if (not eth_is_num(x) or not eth_is_num(y))
    eth_throw(args, eth_type_error());

  int n = eth_num_val(len);
  char *buf = malloc(n + 1);
  if (mvwgetnstr(get_cwin(win), eth_num_val(y), eth_num_val(x), buf, n) == ERR)
  {
    free(buf);
    eth_return(args, curses_exn());
  }
  eth_return(args, eth_create_string_from_ptr(buf));
}

static eth_t
_wgetnstr(void)
{
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t len = eth_arg2(args, eth_number_type);

  int n = eth_num_val(len);
  char *buf = malloc(n + 1);
  if (wgetnstr(get_cwin(win), buf, n) == ERR)
  {
    free(buf);
    eth_return(args, curses_exn());
  }
  eth_return(args, eth_create_string_from_ptr(buf));
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_mvwaddstr(void)
{
  eth_use_tuple_as(tup2_type, 2)
  eth_args args = eth_start(3);
  eth_t win = eth_arg2(args, window_type);
  eth_t pos = eth_arg2(args, tup2_type);
  eth_t str = eth_arg2(args, eth_string_type);
  eth_t x = eth_tup_get(pos, 0);
  eth_t y = eth_tup_get(pos, 1);
  if (not eth_is_num(x) or not eth_is_num(y))
    eth_throw(args, eth_type_error());
  if (mvwaddstr(get_cwin(win), eth_num_val(y), eth_num_val(x),
        eth_str_cstr(str)) == ERR)
  {
    eth_return(args, curses_exn());
  }
  eth_return(args, eth_nil);
}

static eth_t
_waddstr(void)
{
  eth_use_tuple_as(tup2_type, 2)
  eth_args args = eth_start(3);
  eth_t win = eth_arg2(args, window_type);
  eth_t str = eth_arg2(args, eth_string_type);
  if (waddstr(get_cwin(win), eth_str_cstr(str)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_newwin(void)
{
  eth_use_tuple_as(tup4_type, 4)
  eth_args args = eth_start(1);
  eth_t xywh = eth_arg2(args, tup4_type);
  eth_t x = eth_tup_get(xywh, 0);
  eth_t y = eth_tup_get(xywh, 1);
  eth_t w = eth_tup_get(xywh, 2);
  eth_t h = eth_tup_get(xywh, 3);
  if (not eth_is_num(x) or not eth_is_num(y) or
      not eth_is_num(w) or not eth_is_num(h))
  {
    eth_throw(args, eth_type_error());
  }
  WINDOW *win = newwin(eth_num_val(h), eth_num_val(w), eth_num_val(y),
      eth_num_val(x));
  if (win == NULL)
    eth_return(args, curses_exn());
  eth_return(args, create_window(win));
}

static eth_t
_mvwin(void)
{
  eth_use_tuple_as(tup2_type, 2)
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t pos = eth_arg2(args, tup2_type);
  eth_t x = eth_tup_get(pos, 0);
  eth_t y = eth_tup_get(pos, 1);
  if (not eth_is_num(x) or not eth_is_num(y))
    eth_throw(args, eth_type_error());
  if (mvwin(get_cwin(win), eth_num_val(y), eth_num_val(x)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_subwin(void)
{
  eth_use_tuple_as(tup4_type, 4)
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t xywh = eth_arg2(args, tup4_type);
  eth_t x = eth_tup_get(xywh, 0);
  eth_t y = eth_tup_get(xywh, 1);
  eth_t w = eth_tup_get(xywh, 2);
  eth_t h = eth_tup_get(xywh, 3);
  if (not eth_is_num(x) or not eth_is_num(y) or
      not eth_is_num(w) or not eth_is_num(h))
  {
    eth_throw(args, eth_type_error());
  }
  WINDOW *swin = subwin(get_cwin(win), eth_num_val(h), eth_num_val(w),
      eth_num_val(y), eth_num_val(x));
  if (swin == NULL)
    eth_return(args, curses_exn());
  eth_return(args, create_window(swin));
}

static eth_t
_derwin(void)
{
  eth_use_tuple_as(tup4_type, 4)
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t xywh = eth_arg2(args, tup4_type);
  eth_t x = eth_tup_get(xywh, 0);
  eth_t y = eth_tup_get(xywh, 1);
  eth_t w = eth_tup_get(xywh, 2);
  eth_t h = eth_tup_get(xywh, 3);
  if (not eth_is_num(x) or not eth_is_num(y) or
      not eth_is_num(w) or not eth_is_num(h))
  {
    eth_throw(args, eth_type_error());
  }
  WINDOW *swin = derwin(get_cwin(win), eth_num_val(h), eth_num_val(w),
      eth_num_val(y), eth_num_val(x));
  if (swin == NULL)
    eth_return(args, curses_exn());
  eth_return(args, create_window(swin));
}

static eth_t
_mvderwin(void)
{
  eth_use_tuple_as(tup2_type, 2)
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t pos = eth_arg2(args, tup2_type);
  eth_t x = eth_tup_get(pos, 0);
  eth_t y = eth_tup_get(pos, 1);
  if (not eth_is_num(x) or not eth_is_num(y))
    eth_throw(args, eth_type_error());
  if (mvderwin(get_cwin(win), eth_num_val(y), eth_num_val(x)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}


// TODO: sync functions (see same man page)

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_wborder(void)
{
  eth_use_tuple_as(tup4_type, 4)
  eth_args args = eth_start(3);
  eth_t win = eth_arg2(args, window_type);
  eth_t lsrstsbs = eth_arg2(args, tup4_type);
  eth_t ls = eth_tup_get(lsrstsbs, 0);
  eth_t rs = eth_tup_get(lsrstsbs, 1);
  eth_t ts = eth_tup_get(lsrstsbs, 2);
  eth_t bs = eth_tup_get(lsrstsbs, 3);
  eth_t lttrblbr = eth_arg2(args, tup4_type);
  eth_t lt = eth_tup_get(lttrblbr, 0);
  eth_t tr = eth_tup_get(lttrblbr, 1);
  eth_t bl = eth_tup_get(lttrblbr, 2);
  eth_t br = eth_tup_get(lttrblbr, 3);
  if (not eth_is_num(ls) or not eth_is_num(rs) or
      not eth_is_num(ts) or not eth_is_num(bs) or
      not eth_is_num(lt) or not eth_is_num(tr) or
      not eth_is_num(bl) or not eth_is_num(br))
  {
    eth_throw(args, eth_type_error());
  }
  int err = wborder(get_cwin(win),
      eth_num_val(ls), eth_num_val(rs), eth_num_val(ts), eth_num_val(bs),
      eth_num_val(lt), eth_num_val(tr), eth_num_val(bl), eth_num_val(br));
  if (err == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

// TODO: lines (see same man page)

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_mvwgetch(void)
{
  eth_use_tuple_as(tup2_type, 2)
  eth_args args = eth_start(2);
  eth_t win = eth_arg2(args, window_type);
  eth_t pos = eth_arg2(args, tup2_type);
  eth_t x = eth_tup_get(pos, 0);
  eth_t y = eth_tup_get(pos, 1);
  if (not eth_is_num(x) or not eth_is_num(y))
    eth_throw(args, eth_type_error());
  int ch = mvwgetch(get_cwin(win), eth_num_val(y), eth_num_val(x));
  if (ch == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_create_string_from_char(ch));
}

static eth_t
_wgetch(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  int ch = wgetch(get_cwin(win));
  if (ch == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_create_string_from_char(ch));
}

// TODO: ungetch, has_key (see same man page)

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
static eth_t
_wrefresh(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (wrefresh(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_wnoutrefresh(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (wnoutrefresh(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_doupdate(void)
{
  if (doupdate() == ERR)
    return curses_exn();
  return eth_nil;
}

static eth_t
_redrawwin(void)
{
  eth_args args = eth_start(1);
  eth_t win = eth_arg2(args, window_type);
  if (redrawwin(get_cwin(win)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

static eth_t
_wredrawln(void)
{
  eth_args args = eth_start(3);
  eth_t win = eth_arg2(args, window_type);
  eth_t beg = eth_arg2(args, eth_number_type);
  eth_t num = eth_arg2(args, eth_number_type);
  if (wredrawln(get_cwin(win), eth_num_val(beg), eth_num_val(num)) == ERR)
    eth_return(args, curses_exn());
  eth_return(args, eth_nil);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
int
ether_module(eth_module *mod, eth_root *topenv)
{
  window_type = eth_create_type("WINDOW");
  window_type->destroy = destroy_window;
  eth_add_destructor(mod, window_type, (void*)eth_destroy_type);

  eth_define(mod, "stdscr", eth_create_proc(_stdscr, 0, NULL, NULL));

  eth_define(mod, "initscr", eth_create_proc(_initscr, 0, NULL, NULL));
  eth_define(mod, "endwin", eth_create_proc(_endwin, 0, NULL, NULL));
  eth_define(mod, "isendwin", eth_create_proc(_isendwin, 0, NULL, NULL));
  eth_define(mod, "curs_set", eth_create_proc(_curs_set, 1, NULL, NULL));

  eth_define(mod, "start_color", eth_create_proc(_start_color, 0, NULL, NULL));
  eth_define(mod, "has_colors", eth_create_proc(_has_colors, 0, NULL, NULL));
  eth_define(mod, "wattron", eth_create_proc(_wattron, 2, NULL, NULL));
  eth_define(mod, "wattroff", eth_create_proc(_wattroff, 2, NULL, NULL));
  eth_define(mod, "COLOR_PAIR", eth_create_proc(_COLOR_PAIR, 1, NULL, NULL));
  eth_define(mod, "init_color", eth_create_proc(_init_color, 4, NULL, NULL));
  eth_define(mod, "init_pair", eth_create_proc(_init_pair, 3, NULL, NULL));

  eth_define(mod, "getxy", eth_create_proc(_getxy, 1, NULL, NULL));
  eth_define(mod, "getparxy", eth_create_proc(_getparxy, 1, NULL, NULL));
  eth_define(mod, "getbegxy", eth_create_proc(_getbegxy, 1, NULL, NULL));
  eth_define(mod, "getmaxxy", eth_create_proc(_getmaxxy, 1, NULL, NULL));

  eth_define(mod, "werase", eth_create_proc(_werase, 1, NULL, NULL));
  eth_define(mod, "wclear", eth_create_proc(_wclear, 1, NULL, NULL));
  eth_define(mod, "wclrtobot", eth_create_proc(_wclrtobot, 1, NULL, NULL));
  eth_define(mod, "wclrtoeol", eth_create_proc(_wclrtoeol, 1, NULL, NULL));

  eth_define(mod, "mvwgetnstr", eth_create_proc(_mvwgetnstr, 3, NULL, NULL));
  eth_define(mod, "wgetnstr", eth_create_proc(_wgetnstr, 2, NULL, NULL));

  eth_define(mod, "mvwaddstr", eth_create_proc(_mvwaddstr, 3, NULL, NULL));
  eth_define(mod, "waddstr", eth_create_proc(_waddstr, 2, NULL, NULL));

  eth_define(mod, "newwin", eth_create_proc(_newwin, 1, NULL, NULL));
  eth_define(mod, "mvwin", eth_create_proc(_mvwin, 2, NULL, NULL));
  eth_define(mod, "subwin", eth_create_proc(_subwin, 2, NULL, NULL));
  eth_define(mod, "derwin", eth_create_proc(_derwin, 2, NULL, NULL));
  eth_define(mod, "mvderwin", eth_create_proc(_mvderwin, 2, NULL, NULL));

  eth_define(mod, "wborder", eth_create_proc(_wborder, 3, NULL, NULL));

  eth_define(mod, "mvwgetch", eth_create_proc(_mvwgetch, 2, NULL, NULL));
  eth_define(mod, "wgetch", eth_create_proc(_wgetch, 1, NULL, NULL));

  eth_define(mod, "wrefresh", eth_create_proc(_wrefresh, 1, NULL, NULL));
  eth_define(mod, "wnoutrefresh", eth_create_proc(_wnoutrefresh, 1, NULL, NULL));
  eth_define(mod, "doupdate", eth_create_proc(_doupdate, 0, NULL, NULL));
  eth_define(mod, "redrawwin", eth_create_proc(_redrawwin, 1, NULL, NULL));
  eth_define(mod, "wredrawln", eth_create_proc(_wredrawln, 3, NULL, NULL));

  // Key codes:
  eth_define(mod, "key_code_yes", eth_num(KEY_CODE_YES));
  eth_define(mod, "key_min", eth_num(KEY_MIN));
  eth_define(mod, "key_break", eth_num(KEY_BREAK));
  eth_define(mod, "key_sreset", eth_num(KEY_SRESET));
  eth_define(mod, "key_reset", eth_num(KEY_RESET));
  eth_define(mod, "key_down", eth_num(KEY_DOWN));
  eth_define(mod, "key_up", eth_num(KEY_UP));
  eth_define(mod, "key_left", eth_num(KEY_LEFT));
  eth_define(mod, "key_right", eth_num(KEY_RIGHT));
  eth_define(mod, "key_home", eth_num(KEY_HOME));
  eth_define(mod, "key_backspace", eth_num(KEY_BACKSPACE));
  eth_define(mod, "key_f0", eth_num(KEY_F0));
  for (int i = 1; i <= 12; ++i)
  {
    char buf[42];
    sprintf(buf, "key_f%d", i);
    eth_define(mod, buf, eth_num(KEY_F(i)));
  }
  eth_define(mod, "key_dl", eth_num(KEY_DL));
  eth_define(mod, "key_il", eth_num(KEY_IL));
  eth_define(mod, "key_dc", eth_num(KEY_DC));
  eth_define(mod, "key_ic", eth_num(KEY_IC));
  eth_define(mod, "key_eic", eth_num(KEY_EIC));
  eth_define(mod, "key_clear", eth_num(KEY_CLEAR));
  eth_define(mod, "key_eos", eth_num(KEY_EOS));
  eth_define(mod, "key_eol", eth_num(KEY_EOL));
  eth_define(mod, "key_sf", eth_num(KEY_SF));
  eth_define(mod, "key_sr", eth_num(KEY_SR));
  eth_define(mod, "key_npage", eth_num(KEY_NPAGE));
  eth_define(mod, "key_ppage", eth_num(KEY_PPAGE));
  eth_define(mod, "key_stab", eth_num(KEY_STAB));
  eth_define(mod, "key_ctab", eth_num(KEY_CTAB));
  eth_define(mod, "key_catab", eth_num(KEY_CATAB));
  eth_define(mod, "key_enter", eth_num(KEY_ENTER));
  eth_define(mod, "key_print", eth_num(KEY_PRINT));
  eth_define(mod, "key_ll", eth_num(KEY_LL));
  eth_define(mod, "key_a1", eth_num(KEY_A1));
  eth_define(mod, "key_a3", eth_num(KEY_A3));
  eth_define(mod, "key_b2", eth_num(KEY_B2));
  eth_define(mod, "key_c1", eth_num(KEY_C1));
  eth_define(mod, "key_c3", eth_num(KEY_C3));
  eth_define(mod, "key_btab", eth_num(KEY_BTAB));
  eth_define(mod, "key_beg", eth_num(KEY_BEG));
  eth_define(mod, "key_cancel", eth_num(KEY_CANCEL));
  eth_define(mod, "key_close", eth_num(KEY_CLOSE));
  eth_define(mod, "key_command", eth_num(KEY_COMMAND));
  eth_define(mod, "key_copy", eth_num(KEY_COPY));
  eth_define(mod, "key_create", eth_num(KEY_CREATE));
  eth_define(mod, "key_end", eth_num(KEY_END));
  eth_define(mod, "key_exit", eth_num(KEY_EXIT));
  eth_define(mod, "key_find", eth_num(KEY_FIND));
  eth_define(mod, "key_help", eth_num(KEY_HELP));
  eth_define(mod, "key_mark", eth_num(KEY_MARK));
  eth_define(mod, "key_message", eth_num(KEY_MESSAGE));
  eth_define(mod, "key_move", eth_num(KEY_MOVE));
  eth_define(mod, "key_next", eth_num(KEY_NEXT));
  eth_define(mod, "key_open", eth_num(KEY_OPEN));
  eth_define(mod, "key_options", eth_num(KEY_OPTIONS));
  eth_define(mod, "key_previous", eth_num(KEY_PREVIOUS));
  eth_define(mod, "key_redo", eth_num(KEY_REDO));
  eth_define(mod, "key_reference", eth_num(KEY_REFERENCE));
  eth_define(mod, "key_refresh", eth_num(KEY_REFRESH));
  eth_define(mod, "key_replace", eth_num(KEY_REPLACE));
  eth_define(mod, "key_restart", eth_num(KEY_RESTART));
  eth_define(mod, "key_resume", eth_num(KEY_RESUME));
  eth_define(mod, "key_save", eth_num(KEY_SAVE));
  eth_define(mod, "key_sbeg", eth_num(KEY_SBEG));
  eth_define(mod, "key_scancel", eth_num(KEY_SCANCEL));
  eth_define(mod, "key_scommand", eth_num(KEY_SCOMMAND));
  eth_define(mod, "key_scopy", eth_num(KEY_SCOPY));
  eth_define(mod, "key_screate", eth_num(KEY_SCREATE));
  eth_define(mod, "key_sdc", eth_num(KEY_SDC));
  eth_define(mod, "key_sdl", eth_num(KEY_SDL));
  eth_define(mod, "key_select", eth_num(KEY_SELECT));
  eth_define(mod, "key_send", eth_num(KEY_SEND));
  eth_define(mod, "key_seol", eth_num(KEY_SEOL));
  eth_define(mod, "key_sexit", eth_num(KEY_SEXIT));
  eth_define(mod, "key_sfind", eth_num(KEY_SFIND));
  eth_define(mod, "key_shelp", eth_num(KEY_SHELP));
  eth_define(mod, "key_shome", eth_num(KEY_SHOME));
  eth_define(mod, "key_sic", eth_num(KEY_SIC));
  eth_define(mod, "key_sleft", eth_num(KEY_SLEFT));
  eth_define(mod, "key_smessage", eth_num(KEY_SMESSAGE));
  eth_define(mod, "key_smove", eth_num(KEY_SMOVE));
  eth_define(mod, "key_snext", eth_num(KEY_SNEXT));
  eth_define(mod, "key_soptions", eth_num(KEY_SOPTIONS));
  eth_define(mod, "key_sprevious", eth_num(KEY_SPREVIOUS));
  eth_define(mod, "key_sprint", eth_num(KEY_SPRINT));
  eth_define(mod, "key_sredo", eth_num(KEY_SREDO));
  eth_define(mod, "key_sreplace", eth_num(KEY_SREPLACE));
  eth_define(mod, "key_sright", eth_num(KEY_SRIGHT));
  eth_define(mod, "key_srsume", eth_num(KEY_SRSUME));
  eth_define(mod, "key_ssave", eth_num(KEY_SSAVE));
  eth_define(mod, "key_ssuspend", eth_num(KEY_SSUSPEND));
  eth_define(mod, "key_sundo", eth_num(KEY_SUNDO));
  eth_define(mod, "key_suspend", eth_num(KEY_SUSPEND));
  eth_define(mod, "key_undo", eth_num(KEY_UNDO));
  eth_define(mod, "key_mouse", eth_num(KEY_MOUSE));
  eth_define(mod, "key_resize", eth_num(KEY_RESIZE));
  /*eth_define(mod, "key_event", eth_num(KEY_EVENT));*/
  eth_define(mod, "key_max", eth_num(KEY_MAX));

  eth_define(mod, "COLOR_BLACK", eth_num(COLOR_BLACK));
  eth_define(mod, "COLOR_RED", eth_num(COLOR_RED));
  eth_define(mod, "COLOR_GREEN", eth_num(COLOR_GREEN));
  eth_define(mod, "COLOR_YELLOW", eth_num(COLOR_YELLOW));
  eth_define(mod, "COLOR_BLUE", eth_num(COLOR_BLUE));
  eth_define(mod, "COLOR_MAGENTA", eth_num(COLOR_MAGENTA));
  eth_define(mod, "COLOR_CYAN", eth_num(COLOR_CYAN));
  eth_define(mod, "COLOR_WHITE", eth_num(COLOR_WHITE));

  eth_define(mod, "A_NORMAL", eth_num(A_NORMAL));
  eth_define(mod, "A_STANDOUT", eth_num(A_STANDOUT));
  eth_define(mod, "A_UNDERLINE", eth_num(A_UNDERLINE));
  eth_define(mod, "A_REVERSE", eth_num(A_REVERSE));
  eth_define(mod, "A_BLINK", eth_num(A_BLINK));
  eth_define(mod, "A_DIM", eth_num(A_DIM));
  eth_define(mod, "A_BOLD", eth_num(A_BOLD));

  eth_define(mod, "A_PROTECT", eth_num(A_PROTECT));
  eth_define(mod, "A_INVIS", eth_num(A_INVIS));
  eth_define(mod, "A_ALTCHARSET", eth_num(A_ALTCHARSET));
#if defined(A_ITALIC)
  eth_define(mod, "A_ITALIC", eth_num(A_ITALIC));
#endif
  eth_define(mod, "A_CHARTEXT", eth_num(A_CHARTEXT));
  eth_define(mod, "A_COLOR", eth_num(A_COLOR));


  eth_module *ethmod = eth_load_module_from_script2(topenv, "./lib.eth", NULL, mod);
  if (not ethmod)
    return -1;
  eth_copy_defs(ethmod, mod);
  eth_destroy_module(ethmod);

  return 0;
}
