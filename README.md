
<p align='center'>
  <img src="./ether-logo.1280x640.png">
</p>

!!! **IMPORTANT** !!! <br>
Documentation of the language on my [wiki](https://github.com/pidhii/ether/wiki) is outdated.
I hope to update it. But developement is still quite rapid so I dont know if it would make sence. <br>

# [Contents](#contents)
- [Examples](#examples)
- [Build and installation](#build-and-installation)
- [Running in interactive mode](#repl)
- [Syntax higlighting](#syntax-higlighting)
- [Ether Wiki](https://github.com/pidhii/ether/wiki)
- [Where to get help](#where-to-get-help)
- [FAQ](#faq)

# [Check-list](#check-list)
General:
- [x] REPL
- [x] Packet manager (*dope*)

Features:
- [x] Pattern matching
- [x] Closures
- [x] Variant-types
- [x] Lightweight record-types
- [ ] Persistent tables *...some more flexible dictionary-like thing*
- [x] Classes *...for the sake of inheritance*
  - [x] Named classes
  - [x] Objects of unnamed classes
- [x] Tuples
- [x] Persistent vectors
- [x] Regular expressions
- [ ] Macros <br> *I want it to be something derived from Lisp.* 
- [x] User-defined operators
  - [x] redefinition of builtin operators
  - [x] allow definition of new operators
  - [ ] precedence for user-defined operators
- [ ] Named arguments <br> *Can try to exploit records as reading multiple values is quite efficient with them.*
- [x] Exceptions
- [x] Modules
- [ ] Coroutines
- [ ] Multiple return-values (like in Lua)
- [ ] Ability to enter REPL at break-points (for debug)
- [ ] Type constrains *...whatever it means.* <br> *Need a tool to check signatures (arity + return-value) of functions.* <br> *Easy to implement via language itself but it should have some dedicated syntax + care about performance.*  

Syntax:
- [x] simplyfied syntax for loops
- [ ] `match` like in Caml
- [ ] *"*`do`*-natation" for monads?*

###
- [ ] Fuck Python, I'm the queen

# [Optimization](#optimizations)
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



# [Where to get help](#where-to-get-help)
This world is cruel.



# [FAQ](#faq)
Just joking =)
