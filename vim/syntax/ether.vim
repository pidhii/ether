" Vim syntax file
" Language: opium
" Maintainer: Ivan Pidhurskyi

if exists("b:current_syntax")
  finish
endif

set comments=b:#

set iskeyword+=?,'
syn match ethIdentifier /\<[a-z_][a-zA-Z0-9_]*['?]?\>/
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
syn keyword Function number symbol regex svector dvector
syn keyword Function write display newline print printf fprintf format
syn keyword Function car cdr
syn keyword Function pairs
syn keyword Function apply vaarg
syn keyword Function id
syn keyword Function die raise
syn keyword Function force
syn keyword Function system shell
syn keyword Function loadfile
syn keyword Function exit
syn keyword Function addressof

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" Base:
syn keyword Function sin cos tan
syn keyword Function asin acos atan atan2
syn keyword Function sinh cosh tanh
syn keyword Function asinh acosh atanh
syn keyword Function floor ceil trunc round
syn keyword Function sqrt cbrt
syn keyword Function finite nan?
syn keyword Function min max
syn keyword Function hypot abs
syn keyword Function log log10 log2
syn keyword Function list rlist array table 
syn keyword Function length
syn keyword Function strlen substr strstr chop ltrim rtrim trim concat
syn keyword Function open popen
syn keyword Function read readline readlines flush rewind getpos setpos
syn keyword Function flip const uncurry
syn keyword Function range
syn keyword Function revappend reverse
syn keyword Function any? all?
syn keyword Function revmap map
syn keyword Function zip
syn keyword Function foreach
syn keyword Function foldl reduce
syn keyword Function scanl
syn keyword Function repeat iterate
syn keyword Function unfold
syn keyword Function revfilter filter
syn keyword Function drop take
syn keyword Function zero? positive? negative? even? odd?
syn keyword Function match split join

syn match ethModule /\<[A-Z][a-zA-Z0-9_]*\s*\./he=e-1

syn keyword ethType fn
syn region ethTable matchgroup=Type start=/{/ end=/}/ contains=TOP skipwhite skipnl
syn region ethList matchgroup=ethType start=/\[/ end=/\]/ skipwhite skipnl contains=TOP
syn region ethArray matchgroup=ethType start=/\[\s*|/ end=/|\s*\]/ skipwhite skipnl contains=TOP

syn keyword StorageClass pub
"hi link ethModuleName Identifier

" import [as]:
syn region ethImport matchgroup=ethKeyword start=/\<import\>/ end=/\<in\>/ contains=ethImportAs,ethDelimiter,ethIdentifier,ethUnqualified
syn keyword ethImportAs as
hi link ethImportAs Keyword
syn keyword ethUnqualified unqualified
hi link ethUnqualified ethKeyword

syn keyword ethKeyword let rec mut and or in return
syn region ethBegin matchgroup=ethKeyword start=/\<begin\>/ end=/\<end\>/ contains=TOP
syn keyword ethAssert assert

syn keyword ethKeyword if unless when then else try with
syn keyword ethLazy lazy

syn match ethOperator /[-+=*/%><&|.!ο]\+/
syn match ethOperator /:\|\$/
syn keyword ethOperator is eq equal not mod land lor lxor lshl lshr ashl ashr lnot

syn match ethDelimiter /[,;()]/

syn match ethUnit /(\s*)/

syn keyword ethNil nil
syn keyword ethBoolean true false
syn keyword ethConstant stdin stdout stderr

syn match ethLambda /\\/
syn match ethLambda /->/

syn keyword ethLoad load

"syn match ethTableRef /::/ nextgroup=ethKey 
"syn match ethKey /\k\+/ contained

syn match Comment /#.*$/ contains=ethCommentLabel
syn match ethCommentLabel /[A-Z]\w*:/ contained

" Integer
syn match Number '\<\d\+'
syn match Number '0[xX][0-9a-fA-F]\+'

" Floating point number with decimal no E or e
syn match Number '\<\d\+\.\d*'

" Floating point like number with E and no decimal point (+,-)
syn match Number '\<\d[[:digit:]]*[eE][\-+]\=\d\+'

" Floating point like number with E and decimal point (+,-)
syn match Number '\<\d[[:digit:]]*\.\d*[eE][\-+]\=\d\+'

syn keyword Number nan inf

" String
" "..."
syn region String start=/"/ skip=/\\"/ end=/"/ skipnl skipwhite contains=ethFormat
" qq[...]
syn region String matchgroup=ethQq start=/\<qq\[/ skip=/\\]/ end=/\]/ skipnl skipwhite contains=ethFormat
" qq(...)
syn region String matchgroup=ethQq start=/\<qq(/ skip=/\\)/ end=/)/ skipnl skipwhite contains=ethFormat
" qq{...}
syn region String matchgroup=ethQq start=/\<qq{/ skip=/\\}/ end=/}/ skipnl skipwhite contains=ethFormat
" qq/.../
syn region String matchgroup=ethQq start=+\<qq/+ skip=+\\/+ end=+/+ skipnl skipwhite contains=ethFormat
" qq|...|
syn region String matchgroup=ethQq start=+\<qq|+ skip=+\\|+ end=+|+ skipnl skipwhite contains=ethFormat
" qq+...+
syn region String matchgroup=ethQq start=/\<qq+/ skip=/\\+/ end=/+/ skipnl skipwhite contains=ethFormat

" Shell
syn region String matchgroup=ethOperator start=/`/ skip=/\\`/ end=/\`/ skipnl skipwhite contains=ethFormat

"RerEx
" qq[...]
syn region String matchgroup=ethQq start=/\<qr\[/ skip=/\\]/ end=/\]/ skipnl skipwhite contains=ethFormat
" qr(...)
syn region String matchgroup=ethQq start=/\<qr(/ skip=/\\)/ end=/)[a-zA-Z]*/ skipnl skipwhite contains=ethFormat
" qr{...}
syn region String matchgroup=ethQq start=/\<qr{/ skip=/\\}/ end=/}[a-zA-Z]*/ skipnl skipwhite contains=ethFormat
" qr/.../
syn region String matchgroup=ethQq start=+\<qr/+ skip=+\\/+ end=+/[a-zA-Z]*+ skipnl skipwhite contains=ethFormat
" qr|...|
syn region String matchgroup=ethQq start=+\<qr|+ skip=+\\|+ end=+|[a-zA-Z]*+ skipnl skipwhite contains=ethFormat
" qr+...+
syn region String matchgroup=ethQq start=/\<qr+/ skip=/\\+/ end=/+[a-zA-Z]*/ skipnl skipwhite contains=ethFormat

"Search Replace
" /../../
syn region String matchgroup=ethQq start="\<s[g]*/" skip=+\\/+ end=+/+ nextgroup=ethSrPattern1,ethSrPattern1End skipnl skipwhite contains=ethFormat
syn region ethSrPattern1 start=+.+ matchgroup=ethQq skip=+\\/+ end=+/[a-zA-Z]*+ skipnl skipwhite contains=ethFormat contained
syn match ethSrPattern1End +/[a-zA-Z]*+ contained
hi link ethSrPattern1End ethQq
" |..|..|
syn region String matchgroup=ethQq start="\<s[g]*|" skip=+\\|+ end=+|+ nextgroup=ethSrPattern2,ethSrPattern2End skipnl skipwhite contains=ethFormat
syn region ethSrPattern2 start=+.+ matchgroup=ethQq skip=+\\|+ end=+|[a-zA-Z]*+ skipnl skipwhite contains=ethFormat contained
syn match ethSrPattern2End +|[a-zA-Z]*+ contained
hi link ethSrPattern2End ethQq
" +..+..+
syn region String matchgroup=ethQq start="\<s[g]*+" skip=/\\+/ end=/+/ nextgroup=ethSrPattern3,ethSrPattern3End skipnl skipwhite contains=ethFormat
syn region ethSrPattern3 start=+.+ matchgroup=ethQq skip=/\\+/ end=/+[a-zA-Z]*/ skipnl skipwhite contains=ethFormat contained
syn match ethSrPattern3End /+[a-zA-Z]*/ contained
hi link ethSrPattern3End ethQq

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
hi link ethAssert Keyword
hi link ethLazy Keyword

hi link ethType Type
hi link ethDef Type
hi link ethLambda Operator
hi link ethOperator Operator
hi link ethLoad Include
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

"hi link ethIdentifier Identifier
