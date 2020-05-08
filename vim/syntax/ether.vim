" Vim syntax file
" Language: opium
" Maintainer: Ivan Pidhurskyi

if exists("b:current_syntax")
  finish
endif

set comments=sr:(*,mb:*,ex:*)

set iskeyword+=?,'

syn match ethIdentifier /\<[a-z_][a-zA-Z0-9_]*[\'?]\?\>/

syn match ethSymbol     /\<[A-Z][a-zA-Z0-9_]*\>/

"syn region ethModuleDef matchgroup=ethModule start=/\<module\>/ end=/\<end\>/ contains=TOP

"syn keyword ethUse use as

"syn region ethImplHead matchgroup=ethTrait start=/\<impl\>/ end=/\<end\>/ skipwhite skipnl contains=ethTraitName,ethStructName,ethImplTail,ethImplFor
"syn keyword ethImplFor for contained
"hi link ethImplFor ethTrait

"syn region ethImplTail start=/=/me=s-1 matchgroup=ethTrait end=/\<end\>/me=s-1 skipwhite skipnl contains=TOP contained

"syn match Special /@/

"syn region ethTraitWrap matchgroup=ethTrait start=/\<trait\>/ end=/\<end\>/ contains=ethTraitDef
"syn region ethTraitDef matchgroup=ethTraitName start=/\<\k\+\>/ matchgroup=ethTrait end=/\<end\>/me=s-1 skipwhite skipnl contains=TOP containedin=ethTraitWrap contained
"syn match ethTraitName /\k\+/ contained

"syn keyword ethStruct struct nextgroup=ethStructName skipwhite skipnl
"syn match   ethStructName /\k\+/ contained

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""'
" Builtins:
syn keyword ethBuiltinFunction pair? symbol? number? string? boolean? function? tuple? record? file? regexp?

syn keyword ethBuiltinFunction to_number to_symbol
syn keyword ethBuiltinFunction list

syn keyword ethBuiltinFunction dump

syn keyword ethBuiltinFunction len
syn keyword ethBuiltinFunction cat join chr ord
syn keyword ethBuiltinFunction to_upper to_lower
syn keyword ethBuiltinFunction cmp casecmp
syn keyword ethBuiltinFunction sub find
syn keyword ethBuiltinFunction chomp chop

syn keyword ethBuiltinFunction match gsub rev_split split

syn keyword ethBuiltinFunction car cdr
syn keyword ethBuiltinFunction first second third

syn keyword ethBuiltinFunction range linspace
syn keyword ethBuiltinFunction unfold_left unfold_right init
syn keyword ethBuiltinFunction len rev_append append rev
syn keyword ethBuiltinFunction rev_map map rev_zip zip
syn keyword ethBuiltinFunction rev_mapi mapi rev_zipi zipi
syn keyword ethBuiltinFunction iter iteri
syn keyword ethBuiltinFunction rev_filter_map filter_map
syn keyword ethBuiltinFunction rev_flat_map flat_map flatten
syn keyword ethBuiltinFunction rev_filter filter find partition
syn keyword ethBuiltinFunction remove insert
syn keyword ethBuiltinFunction fold_left fold_right
syn keyword ethBuiltinFunction scan_left scan_right
syn keyword ethBuiltinFunction sort merge
syn keyword ethBuiltinFunction drop rev_take take
syn keyword ethBuiltinFunction any? all? member? memq?
syn keyword ethBuiltinFunction assoc assq
syn keyword ethBuiltinFunction transpose
"syn keyword ethBuiltinFunction maximum minimum

syn keyword ethBuiltinFunction id flip const
syn keyword ethBuiltinFunction curry uncurry

syn keyword ethBuiltinFunction even? odd?
syn keyword ethBuiltinFunction min max minmax

syn keyword ethBuiltinFunction open_in open_out open_append
syn keyword ethBuiltinFunction open_pipe_in open_pipe_out
syn keyword ethBuiltinFunction close
syn keyword ethBuiltinFunction input
syn keyword ethBuiltinFunction print print_to newline
syn keyword ethBuiltinFunction write_to write
syn keyword ethBuiltinFunction read_line read_line_of
syn keyword ethBuiltinFunction read read_of
syn keyword ethBuiltinFunction read_file
syn keyword ethBuiltinFunction tell seek

syn keyword ethBuiltinFunction printf eprintf fprintf format
syn keyword ethBuiltinFunction apply
syn keyword ethBuiltinFunction die raise exit
syn keyword ethBuiltinFunction system shell
syn keyword ethBuiltinFunction load

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" Base:
"syn keyword Function sin cos tan
"syn keyword Function asin acos atan atan2
"syn keyword Function sinh cosh tanh
"syn keyword Function asinh acosh atanh
"syn keyword Function floor ceil trunc round
"syn keyword Function sqrt cbrt
"syn keyword Function finite nan?
"syn keyword Function min max
"syn keyword Function hypot abs
"syn keyword Function log log10 log2
"syn keyword Function table
"syn keyword Function substr strstr ltrim rtrim trim
"syn keyword Function read readline readlines flush rewind getpos setpos
"syn keyword Function scanl
"syn keyword Function repeat iterate
"syn keyword Function unfold
"syn keyword Function zero? positive? negative?
"syn keyword Function split join

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""'
" Miscelenious:
syn keyword Special command_line

syn region ethOf matchgroup=StorageClass start=/\<of\>/ matchgroup=ethType end=/\[\?\<[a-z]\k\+\>]\?/ skipwhite skipnl contains=ethSymbol,ethIdentifier

syn match ethModule /\<[A-Z][a-zA-Z0-9_]*\s*\./he=e-1 nextgroup=ethModule,ethMember
syn match ethMember /\<[a-z_][a-zA-Z0-9_]*['?]?\>/

syn match Special /\<_\>/

syn keyword ethType fn

syn region ethBraces matchgroup=Delimiter start=/(/ end=/)/ contains=TOP skipwhite skipnl

syn region ethTable matchgroup=Type start=/{/ end=/}/ contains=TOP skipwhite skipnl
syn region ethList matchgroup=Type start=/\[/ end=/\]/ skipwhite skipnl contains=TOP
syn region ethArray matchgroup=Type start=/\[\s*|/ end=/|\s*\]/ skipwhite skipnl contains=TOP

syn keyword StorageClass pub
syn keyword Special __builtin

syn match ethDelimiter /[,;()]/

" open:
syn region ethOpen matchgroup=ethKeyword start=/\<open\>/ end=/\<in\>/ contains=ethDelimiter,ethIdentifier
syn region ethUsing matchgroup=ethKeyword start=/\<using\>/ end=/\<in\>/ contains=ethUsingAs,ethIdentifier
syn keyword ethUsingAs as contained
hi link ethUsingAs ethKeyword

" module
syn region ethModuleDef matchgroup=Keyword start=/\<module\>/ end=/\<end\>/ contains=TOP skipnl skipwhite

syn match ethKeyword /\<let\>/
syn match ethConditional /\<if\%(\s\+let\)\?\>/
syn region ethTryWith matchgroup=ethException start=/\<try\>/ end=/\<with\>/ contains=TOP skipwhite skipnl
syn region ethMatch matchgroup=ethConditional start=/\<case\>/ end=/\<of\>/ contains=TOP skipwhite skipnl
syn match ethConditional /\<then\>/
syn region ethBegin matchgroup=ethConditional start=/\<then\s\+begin\>/ end=/\<end\>/ contains=TOP skipwhite skipnl
syn region ethBegin matchgroup=ethKeyword start=/\<begin\>/ end=/\<end\>/ contains=TOP skipwhite skipnl
syn region ethObject matchgroup=ethKeyword start=/\<object\>/ end=/\<end\>/ contains=TOP skipnl skipwhite
syn keyword ethMethod method contained containedin=ethObject
hi link ethMethod Keyword
syn keyword ethInherit inherit contained containedin=ethObject
hi link ethInherit Keyword
syn keyword ethVal val contained containedin=ethObject
hi link ethVal Keyword
syn region ethDo matchgroup=ethKeyword start=/\<do\>/ end=/\<done\>/ contains=TOP skipwhite skipnl
syn keyword ethKeyword let rec and or in as with
syn keyword ethConditional unless when else otherwize
syn keyword ethAssert assert

syn keyword ethLazy lazy

syn match ethOperator /[-+=*/%><&|.!^~]\+/
syn match ethOperator /:\|\$/
syn keyword ethOperator is eq not mod land lor lxor lshl lshr ashl ashr lnot

syn match Keyword /!/
syn match ethUnit /(\s*)/

syn keyword ethNil nil
syn keyword ethBoolean true false
syn keyword ethConstant stdin stdout stderr

syn match ethLambda /\\/
syn match ethLambda /->/

"syn match ethTableRef /::/ nextgroup=ethKey
"syn match ethKey /\k\+/ contained

syn region Comment start=/^#!/ end=/$/ contains=ethCommentLabel
syn region Comment start=/(\*\%([^)]\|$\)/ end=/\*)/ contains=Comment skipwhite skipnl
syn match ethCommentLabel /[A-Z]\w*:/ contained

" Integer
syn match Number '\<\d\+'
syn match Number '0[xX][0-9a-fA-F]\+'

" Floating point number with decimal no E or e
syn match Number '\<\d\+\.\d\+'

" Floating point like number with E and no decimal point (+,-)
syn match Number '\<\d[[:digit:]]*[eE][\-+]\=\d\+'

" Floating point like number with E and decimal point (+,-)
syn match Number '\<\d[[:digit:]]*\.\d*[eE][\-+]\=\d\+'

syn keyword Number nan inf

" String
syn region String start=/"/ skip=/\\"/ end=/"/ skipnl skipwhite contains=ethFormat
" Regexp
syn region String matchgroup=ethRegexp start=+\\+ skip=+\\/+ end=+/[a-zA-Z]*+ skipnl skipwhite
hi link ethRegexp Type

"" Shell
"syn region String matchgroup=ethOperator start=/`/ skip=/\\`/ end=/\`/ skipnl skipwhite contains=ethFormat

""RerEx
"" qq[...]
"syn region String matchgroup=ethQq start=/\<qr\[/ skip=/\\]/ end=/\]/ skipnl skipwhite contains=ethFormat
"" qr(...)
"syn region String matchgroup=ethQq start=/\<qr(/ skip=/\\)/ end=/)[a-zA-Z]*/ skipnl skipwhite contains=ethFormat
"" qr{...}
"syn region String matchgroup=ethQq start=/\<qr{/ skip=/\\}/ end=/}[a-zA-Z]*/ skipnl skipwhite contains=ethFormat
"" qr/.../
"syn region String matchgroup=ethQq start=+\<qr/+ skip=+\\/+ end=+/[a-zA-Z]*+ skipnl skipwhite contains=ethFormat
"" qr|...|
"syn region String matchgroup=ethQq start=+\<qr|+ skip=+\\|+ end=+|[a-zA-Z]*+ skipnl skipwhite contains=ethFormat
"" qr+...+
"syn region String matchgroup=ethQq start=/\<qr+/ skip=/\\+/ end=/+[a-zA-Z]*/ skipnl skipwhite contains=ethFormat

""Search Replace
"" /../../
"syn region String matchgroup=ethQq start="\<s[g]*/" skip=+\\/+ end=+/+ nextgroup=ethSrPattern1,ethSrPattern1End skipnl skipwhite contains=ethFormat
"syn region ethSrPattern1 start=+.+ matchgroup=ethQq skip=+\\/+ end=+/[a-zA-Z]*+ skipnl skipwhite contains=ethFormat contained
"syn match ethSrPattern1End +/[a-zA-Z]*+ contained
"hi link ethSrPattern1End ethQq
"" |..|..|
"syn region String matchgroup=ethQq start="\<s[g]*|" skip=+\\|+ end=+|+ nextgroup=ethSrPattern2,ethSrPattern2End skipnl skipwhite contains=ethFormat
"syn region ethSrPattern2 start=+.+ matchgroup=ethQq skip=+\\|+ end=+|[a-zA-Z]*+ skipnl skipwhite contains=ethFormat contained
"syn match ethSrPattern2End +|[a-zA-Z]*+ contained
"hi link ethSrPattern2End ethQq
"" +..+..+
"syn region String matchgroup=ethQq start="\<s[g]*+" skip=/\\+/ end=/+/ nextgroup=ethSrPattern3,ethSrPattern3End skipnl skipwhite contains=ethFormat
"syn region ethSrPattern3 start=+.+ matchgroup=ethQq skip=/\\+/ end=/+[a-zA-Z]*/ skipnl skipwhite contains=ethFormat contained
"syn match ethSrPattern3End /+[a-zA-Z]*/ contained
"hi link ethSrPattern3End ethQq

syn match  SpecialChar /\\\d\+/ containedin=ethSrPattern1,ethSrPattern2,ethSrPattern3 contained
hi link ethSrPattern1 String
hi link ethSrPattern2 String
hi link ethSrPattern3 String

" Inline expression
syn region ethFormat matchgroup=ethSpecial start=/%{/ end=/}/ contained contains=TOP

" Special characters
syn match ethSpecial /\\$/ containedin=String contained
syn match SpecialChar "\\a" containedin=String contained
syn match SpecialChar "\\b" containedin=String contained
syn match SpecialChar "\\e" containedin=String contained
syn match SpecialChar "\\f" containedin=String contained
syn match SpecialChar "\\n" containedin=String contained
syn match SpecialChar "\\r" containedin=String contained
syn match SpecialChar "\\t" containedin=String contained
syn match SpecialChar "\\v" containedin=String contained
syn match SpecialChar "\\?" containedin=String contained
syn match SpecialChar "\\%" containedin=String contained
syn match SpecialChar "\\x[0-9a-fA-F]\{1,2}" containedin=String contained



hi link ethModule Identifier
"hi link ethUse Define

"hi link ethStruct     StorageClass
"hi link ethStructName Type

hi link ethTrait      Structure
hi link ethTraitName  StorageClass
hi link ethStruct     Structure
hi link ethStructName StorageClass

hi link ethSpecial Special

hi link ethKeyword Statement
hi link ethConditional Conditional
hi link ethException Exception

hi link ethAssert Keyword
hi link ethLazy Keyword

hi link ethType Type
hi link ethDef Type
hi link ethLambda Operator
hi link ethOperator Operator
"hi link ethUnit PreProc
hi link ethUnit Constant
hi link ethDelimiter Delimiter
hi link ethConditional Conditional
hi link ethNil Constant
hi link ethBoolean Boolean
hi link ethConstant Constant
hi link ethSymbol Constant

hi link ethKey Identifiers
hi link ethTableRef SpecialChar

hi link ethCommentLabel Comment

hi link ethQq Keyword

hi link ethBuiltinFunction Function
"hi link ethIdentifier Identifier
