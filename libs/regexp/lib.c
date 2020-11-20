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

#include <pcre.h>

static eth_t
_create(void)
{
  eth_use_symbol(Regexp_error);
  eth_args args = eth_start(2);
  eth_t pat = eth_arg2(args, eth_string_type);
  eth_t opt = eth_arg2(args, eth_number_type);
  eth_t re = eth_create_regexp(eth_str_cstr(pat), eth_num_val(opt), NULL, NULL);
  if (not re)
    eth_throw(args, Regexp_error);
  eth_return(args, re);
}

static eth_t
_study(void)
{
  eth_args args = eth_start(1);
  eth_t re = eth_arg2(args, eth_regexp_type);
  eth_study_regexp(re);
  eth_return(args, re);
}

int
ether_module(eth_module *mod, eth_root *topenv)
{
  eth_define(mod, "_create", eth_create_proc(_create, 2, NULL, NULL));
  eth_define(mod, "_study", eth_create_proc(_study, 1, NULL, NULL));

  eth_define(mod, "pcre_anchored", eth_num(PCRE_ANCHORED));
  eth_define(mod, "pcre_auto_callout", eth_num(PCRE_AUTO_CALLOUT));
  eth_define(mod, "pcre_bsr_anycrlf", eth_num(PCRE_BSR_ANYCRLF));
  eth_define(mod, "pcre_bsr_unicode", eth_num(PCRE_BSR_UNICODE));
  eth_define(mod, "pcre_caseless", eth_num(PCRE_CASELESS));
  eth_define(mod, "pcre_dollar_endonly", eth_num(PCRE_DOLLAR_ENDONLY));
  eth_define(mod, "pcre_dotall", eth_num(PCRE_DOTALL));
  eth_define(mod, "pcre_dupnames", eth_num(PCRE_DUPNAMES));
  eth_define(mod, "pcre_extended", eth_num(PCRE_EXTENDED));
  eth_define(mod, "pcre_extra", eth_num(PCRE_EXTRA));
  eth_define(mod, "pcre_firstline", eth_num(PCRE_FIRSTLINE));
  eth_define(mod, "pcre_javascript_compat", eth_num(PCRE_JAVASCRIPT_COMPAT));
  eth_define(mod, "pcre_multiline", eth_num(PCRE_MULTILINE));
#ifdef PCRE_NEVER_UTF
  eth_define(mod, "pcre_never_utf", eth_num(PCRE_NEVER_UTF));
#endif
  eth_define(mod, "pcre_newline_any", eth_num(PCRE_NEWLINE_ANY));
  eth_define(mod, "pcre_newline_anycrlf", eth_num(PCRE_NEWLINE_ANYCRLF));
  eth_define(mod, "pcre_newline_cr", eth_num(PCRE_NEWLINE_CR));
  eth_define(mod, "pcre_newline_crlf", eth_num(PCRE_NEWLINE_CRLF));
  eth_define(mod, "pcre_newline_lf", eth_num(PCRE_NEWLINE_LF));
  eth_define(mod, "pcre_no_auto_capture", eth_num(PCRE_NO_AUTO_CAPTURE));
#ifdef PCRE_NO_AUTO_POSSESS
  eth_define(mod, "pcre_no_auto_possess", eth_num(PCRE_NO_AUTO_POSSESS));
#endif
  eth_define(mod, "pcre_no_start_optimize", eth_num(PCRE_NO_START_OPTIMIZE));
  eth_define(mod, "pcre_no_utf16_check", eth_num(PCRE_NO_UTF16_CHECK));
  eth_define(mod, "pcre_no_utf32_check", eth_num(PCRE_NO_UTF32_CHECK));
  eth_define(mod, "pcre_no_utf8_check", eth_num(PCRE_NO_UTF8_CHECK));
  eth_define(mod, "pcre_ucp", eth_num(PCRE_UCP));
  eth_define(mod, "pcre_ungreedy", eth_num(PCRE_UNGREEDY));
  eth_define(mod, "pcre_utf16", eth_num(PCRE_UTF16));
  eth_define(mod, "pcre_utf32", eth_num(PCRE_UTF32));
  eth_define(mod, "pcre_utf8", eth_num(PCRE_UTF8));

  if (not eth_add_module_script(mod, "lib.eth", topenv))
    return 1;
  return 0;
}
