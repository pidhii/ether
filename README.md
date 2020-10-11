# [Contents](#contents)
- [Examples](#examples)
- [Build and installation](#build-and-installation)
- [Running in interactive mode](#repl)
- [Syntax higlighting](#syntax-higlighting)
- [Builtin types](#builtin-types)
- [Builtin operators](#builtin-operators)
- [Syntax](#syntax)
- [Exceptions and error handling](#exceptions-and-error-handling)
- [Available modules](#available-modules)
- [Where to get help](#where-to-get-help)
- [FAQ](#faq)

# [Check-list](#check-list)
- [x] REPL
- [x] Pattern matching
- [x] Closures
- [x] Variant-types
- [x] Lightweight record-types
- [ ] *Objects?*
  - [ ] *fields?*
  - [ ] *methods*
  - [ ] *inheritance*
- [x] Tuples
- [ ] Persistent tables
- [x] Persistent vectors
- [x] Regular expressions
- [ ] Macros
- [ ] User-defined operators
  - [x] redefinition of builtin operators
  - [x] definition of new operators
  - [ ] precedence for user-defined operators
- [ ] Named arguments
- [x] Exceptions
- [x] Modules
- [ ] `match` like in Caml
- [ ] `do`*-natation for monads?*
- [ ] Coroutines
- [ ] Fuck Python, I'm the queen

# [Optimization](optimizations)
- [x] Smart reference counting
- [ ] Capture unpacked fields if available
- [ ] Unpack structs outside closure (if applicable)
- [ ] Inline small functions at first pass
- [ ] Dynamic optimization
  - [ ] inline captures
  - [ ] *branch prediction?*
- [ ] Detect fixed values in loops
- [ ] JIT compilation
- [ ] Propagate type information when possible
  - [x] intrinsic operators
  - [x] constructors
  - [ ] functions
    - [ ] C functions
    - [ ] closures
- [ ] Merge sequential unpacks (if applicable)
- [ ] Smaller instructions
- [ ] Delay lambda constructors (when used conditionaly)

# [Examples](#examples)
- [basics](./samples/basics.eth)
- [merge-sort](./samples/mergesort.eth)

# [Build and installation](#build-and-installation)
Build and install with [CMake](https://cmake.org/runningcmake/).  
*Debug* and *Release* build types are supported.

To build *Release* configuration do
- create directory for temporary files:  
  ```sh
  $ mkdir build
  ```
- run CMake to generate build-scripts:  
  ```sh
  $ cmake -D CMAKE_BUILD_TYPE=Release \ # we want Release-configuration
          -D CMAKE_INSTALL_PREFIX=<where-to-install> \
          -B build \                    # temporary directory for a build
          -S .                          # path to Ether sources
  ```
- build and install (we are using GNU Make here):
  ```sh
  $ make -C build install
  ```
- additionaly you can run some tests:
  ```sh
  $ make -C build test 
  ```
- now you can add Ether to your system environment:
  ```sh
  $ prefix=<full-path-to-installation-directory>
  $ export PATH=$prefix/bin:path
  $ export PKG_CONFIG_PATH=${PKG_CONFIG_PATH:+${PKG_CONFIG_PATH}:}$prefix/lib/pkgconfig
  ```
  or you can use [env.sh](./env.sh) to setup environment in current shell:
  ```sh
  $ source env.sh <path-to-installation>
  ```

# [REPL](#repl)
To run Ether in interactive mode just run it straightaway:
```
$ ether
Ether REPL
version: 0.2.0
build: Release
build flags:  -Wall -Werror -Wextra -Wno-unused -Wno-error=cpp -rdynamic -O3 -DNDEBUG
prefix: /home/pidhii/sandbox/create/ether/Release/install

Enter <C-d> (EOF) to exit
Commands:
  '.'                   to reset input buffer (cancel current expression)
  '.help'               show help and available commands
  '.help <ident>'       show help for given identifier
  '.complete-empty'     display all available identifiers when completing empty word
  '.no-complete-empty'  disable effect of the previous command

>
```

**Note** that some syntacticly valid expressions will not work for REPL. It is
due to "machanisms" of REPL are different to those applied to script processing.

# [Syntax higlighting](#syntax-higlighting)
As you may have noticed, ether syntax is wery similar to ML's one, so generaly
you can just set your editor to treat it like OCaml for examle. However there
are differences, and some of ether-only expressions tend to appear very often
in the code (e.g. `if let <pattern>`).

## Vim
I'm maintaining native syntax configuration only for Vim (not familiar with other
editors). See [ether-vim](https://github.com/pidhii/ether-vim) for the plugin.
You can install with [pathogen](https://github.com/tpope/vim-pathogen).

To make Vim recognise ether scripts you can add following line to your .vimrc:
```vim
autocmd BufRead,BufNewFile *.eth set filetype=ether syntax=ether
```

If you use [NERDCommenter](https://www.vim.org/scripts/script.php?script_id=1218)
you can also add:
```vim
let g:NERDCustomDelimiters = {
  \ 'ether': { 'left': '--', 'leftAlt': '--[[', 'rightAlt': ']]', 'nested': 1 }
  \ }
```

# [Builtin types](#builtin-types)

## [Booleans](#booleans)
Guess what!
Ether have 2 objects of this type, namely:
* `true`, and
* `false`

Note, unlike in many common programming languages, all conditional expressions
test exactly on being *"not false"*, rather than *"not false or non-null"*.

These are singleton objects, thus you can use comparison on physical equality
for these. Also you can use `true` and `false` in patterns.
```
not true is false;
not false is true;

let (true, false) = (true, false) in
...
```

## [Numbers](#numbers)
By default numbers are internally represented with `long double` type in C.
Also, one can compile Ether to use other C types.
Being `long double` on my PC means to have 80-bit representation with 64 significand bits.
So you can also feel free to use it inplace of `ssize_t` in C.

* > Although the x86 architecture, and specifically the x87 floating-point instructions on x86, supports 80-bit extended-precision operations, ...
* > A 64-bit significand provides sufficient precision to avoid loss of precision when the results are converted back to double precision format in the vast number of cases.

See more on [wiki](https://en.wikipedia.org/wiki/Extended_precision), and don't bother about performance.


## [Strings](#strings)
Strings are implemented similar to `std::string` in C++:
- compatible with C strings (terminating null-byte)
- however, length is stored excplicitly, thus
- string can contain null-bytes

Most utilities to work with strings are contained in the [String-module](https://github.com/pidhii/ether/wiki/String).

## Symbols

## Nil

## Pairs and cons-lists

#### `<x> :: <y>`

#### `car <x>`
#### `cdr <x>`

## Regular expressions (regexp)
Regular expressions are using PCRE as backend, so refer to the PCRE documentation for the syntax of patterns.

### Syntax for regular expressions
`\<regexp>/[imsx]`
where
* **i** - caseless
* **m** - multiline mode
* **s** - '.' will match newline too
* **x** - extended mode

## Ranges

## Tuples
#### `first <x>`
#### `second <x>`
#### `third <x>`

## Records

## Variants

## Functions



# [Builtin operators](#builtin-operators)

## Arithmetic operators
Operator | Description | Exceptions
---------|-------------|-----------
`<x> + <y>` | Addition. | `Type_error`
`<x> - <y>` | Substraction. | `Type_error`
`<x> * <y>` | Multiplication. | `Type_error`
`<x> / <y>` | Float division. | `Type_error`
`<x> ^ <y>` | Raise *x* to the power of *y*. | `Type_error`

## Bitwize (logical) operators
Operator | Description | Exceptions
---------|-------------|-----------
`<x> land <y>` | Logical AND. | `Type_error`
`<x> lor <y>` | Logical OR. | `Type_error`
`<x> lxor <y>` | Logical XOR. | `Type_error`
`<x> lshl <y>` | Logical left shift. | `Type_error`
`<x> lshr <y>` | Logical right shift. | `Type_error`
`<x> ashl <y>` | Arithmetic left shift. | `Type_error`
`<x> ashr <y>` | Arithmetic right shift. | `Type_error`
`lnot <x>` | Logical negation. | `Type_error`

## Comparison operators
Operator | Description | Exceptions
---------|-------------|-----------
`<x> < <y>` | Less than. | `Type_error`
`<x> > <y>` | Greater than. | `Type_error`
`<x> <= <y>` | Less or equal. | `Type_error`
`<x> >= <y>` | Greater or equal. | `Type_error`
`<x> == <y>` | Equality. | `Type_error`
`<x> /= <y>` | Inequality. | `Type_error`

## Strings and regular expressions
Operator | Description | Exceptions
---------|-------------|-----------
`<string> =~ <regexp>` | Check if string matches regexp. | `Type_error`, `Regexp_error`
`<string> ++ <string>` | String concatenation | `Type_error`

## Miscellanious
Operator | Description
---------|------------
`<function> $ <expr>` | Function application operator as in Haskell
`<expr> \|> <function>` | Function application operator as in OCaml



# [Exceptions and error handling](#exceptions-and-error-handling)



# [Available modules](#avaliable-modules)

## Builtin modules
Name | Description
-----|------------
List | ...
String | ...
Regexp | Utilities for regular expressions
Vector | ...
Math | ...
IO | Input/Output utilities
CmdArg | Processing command-line arguments
Option | Option-manad utilities
Os | Interface to operating system
Ref | **Unsafe** mutables

## Other modules
Name | Description
-----|------------
[Libuv](https://github.com/pidhii/ether-libuv) | Asynchronous I/O

# [Where to get help](#where-to-get-help)
Enter [REPL](#repl) and try:
```
.help [<module-path>.]<variable>
```
# [FAQ](#faq)
Just joking =)
