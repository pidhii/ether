# [Contents](contents)
- [Examples](examples)
  - [basics](basics)
  - [merge sort](merge-sort)
- [Build and installation](installation)
- [Syntax higlighting](syntax-higlighting)
  - [Vim](vim)
- [Builtin types and functions](builtins)
  - [booleans](booleans)
  - [numbers](numbers)
  - [strings](strings)
  - [symbols](symbols)
  - [nil](nil)
  - [pairs and lists](pairs-and-cons-lists)
  - [regular expressions](regular-expressions)
  - [ranges](ranges)
  - [tuples](tuples)
  - [records](records)
  - [variants](variants)
  - [functions](functions)
- [Builtin operators](builtin-operators)
- [Exceptions and error handling](exceptions-and-error-handling)
- [IO](io)

# [Check-list](check-list)
- [ ] REPL
- [x] Pattern matching
- [x] Closures
- [x] Variant-types
- [x] Lightweight record-types
- [x] Tuples
- [ ] Persistent tables
- [ ] Persistent vectors
- [ ] Mutables
- [x] Regular expressions
- [ ] Macros
- [ ] User-defined operators
  - [x] redefinition of builtin operators
  - [ ] definition of new operators
- [ ] Named arguments
- [x] Exceptions
- [x] Modules
- [x] Memory management with naїve reference counting ONLY
- [x] No memory leaks
- [ ] `match` like in Caml
- [ ] Coroutines
- [ ] Fuck Python, I'm the queen


# [Examples](examples)

## Basics
```ocaml
(* Simple function *)
let square x = x * x in
print (square 2);


(* Recursive function *)
let rec factorial x =
  if x <= 1 then 1
  else x * factorial (x - 1)
in
print (factorial 3);


(* Multiple cross-referencing functions *)
let rec even? x =
  x == 0 || odd? (x - 1)
and odd? x =
  x /= 0 && even? (x - 1)
in
print (even? 42);


(* Better way to write factorial: tail-recursion *)
let rec factorial2 x acc =
  if x <= 1 then acc
  else factorial2 (x - 1) (acc * x)
in
print (factorial2 1000000 1);


(* Symbols *)
print Symbols_begin_with_UpperCase_character;


(* Exception handling *)
try factorial 1000000
with Stack_overflow -> print "Stack overflow";


(* Currying *)
let add x y = x + y in
let increment = add 1 in
print (increment 3);


(* Lists *)
print ([1, 2, 3] eq 1 :: 2 :: 3 :: []);


(* Pattern matching *)
let x :: xs = [1, 2, 3] in
print (x, xs);


(* Higher order functions *)
let rec map f xs =
  if let x :: xs' = xs then
    f x :: map f xs'
  else []
in
print (map increment [1, 2, 3]);


(* Closures *)
print (map (fn x -> x^2) [1, 2, 3]);


(* Tuples *)
let (a, b) = (A, B) in
print (a, b);


(* Records *)
let { login } = { name = "Jill", login = "Mikle Jackson" } in
print login;


(* Variants (tagged wrapper-types) *)
let safe_div x y = Some (x / y) if y /= 0 else false in
let Some x = safe_div 1 2 in
print x;


(* FizzBuzz
 *
 * Note a `()`: this is a synonim for `nil`.
 *)
for_each [1 .. 100] (fn i ->
  when i mod 3 /= 0 && i mod 5 /= 0 then print i;
  when i mod 3 == 0 then print "Fizz";
  when i mod 5 == 0 then print "Buzz";
);
```

## Merge sort
```ocaml
(*
 * Split a list on two halves using slow/fast iterators approach.
 *
 * Note that the left part is returned in reversed order, however it does not
 * realy matter for the algorithm.
 *
 * Also note that the `loop` is calling itself from a propper tail position.
 * Thus this function is evaluated withing fixed/finite stack frame, and it
 * will never generate a `Stack_overflow` exception.
 *)
let split xs =
  (* This is the way you write loops: just create an auxilary function *)
  let rec loop slow fast acc =
    (* Try to move fast iterator on two positions further; otherwize, we are
     * done  *)
    if let _::_::fast = fast then
      (* Now move slow iterator by one position and continue the loop *)
      let x :: slow = slow in
      loop slow fast (x :: acc)
    else (acc, slow)
  in loop xs xs nil
in

(*
 * Merge two sorted lists preserving order.
 *)
let merge xs ys =
  let rec loop xs ys acc =
    (* This dispatch would look much better with MATCH expression like in OCaml,
     * however I haven't yet implemented it. *)
    if let x :: xs' = xs then
      if let y :: ys' = ys then
        (* Take the smallest of x and y and prepend it to the result *)
        if x < y
        then loop xs' ys  (x :: acc)
        else loop xs  ys' (y :: acc)
      else
        (* No more ys left, append all remaining xs to the result *)
        rev_append acc xs
    else
      (* No more xs left, append all remaining ys to the result *)
      rev_append acc ys
  in
  loop xs ys nil
in

(*
 * Sort a list of numbers in increasing order.
 *)
let rec sort xs =
  (* Check if length of the list is greater than 1; otherwize, there is nothing
   * to sort *)
  if let _::_::_ = xs then
    let (l, r) = split xs in
    merge (sort l) (sort r)
  else xs
in

(*
 * Read a list of numbers.
 *)
let read_list file =
  (* Builtin `read_line_of` will throw `End_of_file` exception upon reaching
   * end of file, so we could use TRY/WITH expression to do the job.
   *
   * However, we want to be tail-recursive, but it is impossible to achieve
   * if we perform recursive call withing TRY-block. Instead, we will wrap
   * the `read_line_of` into a new function and use "Maybe"-monad (sort of).
   *
   * In case of succesfull reading, it will return a variant `Some <line>`;
   * otherwize, it returns nil. Note that in fact it does not matter what do
   * we return in case of End_of_file exception. We only need it to be NOT the
   * `Some _`-variant.
   *)
  let read_line_opt file =
    try Some (read_line_of file)
    with End_of_file -> ()
  in

  (* And here is the actual loop to read the list *)
  let rec loop acc =
    if let Some x = read_line_opt file then
      (* Note the `$`: this is an application operator from Haskell. The whole
       * role of `$` is to have the lowest precedence of all operators *)
      loop $ (to_number $ chomp x) :: acc
    else acc
  in loop nil
in

(* Read a list from standard input.
 *
 * Press <ENTER> after each entered number. ...we could have done it better,
 * however I think this is out of the scope of this tutorial.
 * Use <C-d> (close input) to finish entering the list. *)
let l = read_list stdin in
print ("Given list:", l);

(* Sort it *)
print ("Sorted list:", sort l);
```

# [Build and installation](installation)
Build and install with [CMake](https://cmake.org/runningcmake/).  
*Debug* and *Release* build types are supported.

To build *Release* configuration do
- create directory for temporary files:  
  ```
  mkdir build
  ```
- run CMake to generate build-scripts:  
  ```
  cmake -D CMAKE_BUILD_TYPE=Release \ # we want Release-configuration
        -D CMAKE_INSTALL_PREFIX=<where-to-install> \
        -B build \                    # our temporary directory
        -S .                          # path to Ether sources
  ```
- build and install (we are using GNU Make here):
  ```
  make -C build install
  ```
- additionaly you can run some tests:
  ```
  make -C build test 
  ```
- now you can add Ether to your system environment:
  ```sh
  prefix=<full-path-to-installation-directory>
  export PATH=$prefix/bin:path
  export PKG_CONFIG_PATH=${PKG_CONFIG_PATH:+${PKG_CONFIG_PATH}:}$prefix/lib/pkgconfig
  ```

# [Syntax higlighting](syntax-higlighting)
As you may have noticed, ether syntax is wery similar to ML's one, so generaly
you can just set your editor to treat it like OCaml for examle. However there
are differences, and some of ether-only expressions tend to appear very often
in the code (e.g. `if let <pattern>`).

## Vim
I'm maintaining native syntax configuration only Vim (not familiar with other
editors). Sytax and indentation can be loaded from [vim/syntax/ether.vim](vim/syntax/ether.vim)
and [vim/indent/ether.vim](vim/indent/ether.vim), respectively.

To make Vim recognise ether scripts you can add following line to your .vimrc:
```vim
autocmd BufRead,BufNewFile *.eth set filetype=ether syntax=ether
```

If you use [NERDCommenter](https://www.vim.org/scripts/script.php?script_id=1218)
you can also add:
```vim
let g:NERDCustomDelimiters = {
  \ 'ether': { 'left': '#', 'leftAlt': '(*', 'rightAlt': '*)', 'nested': 1 }
  \ }
```

# [Builtin types and functions](builtins)

## Booleans
Guess what!
Ether have 2 objects of this type, namely:
* `true`, and
* `false`

Note, unlike in many common programming languages, all conditional expressions
test exactly on being *"not false"*, rather than *"not false or non-null"*.


## Numbers
By default numbers are internally represented with `long double` type in C.
Also, one can compile Ether to use other C types.
Being `long double` on my PC means to have 80-bit representation with 64 significand bits.
So you can also feel free to use it inplace of `ssize_t` in C.

* > Although the x86 architecture, and specifically the x87 floating-point instructions on x86, supports 80-bit extended-precision operations, ...
* > A 64-bit significand provides sufficient precision to avoid loss of precision when the results are converted back to double precision format in the vast number of cases.

See more on [wiki](https://en.wikipedia.org/wiki/Extended_precision), and don't bother about performance.

### Arithmetic operators
Operator | Description | Exceptions
---------|-------------|-----------
`<x> + <y>` | Addition. | `Type_error`
`<x> - <y>` | Substraction. | `Type_error`
`<x> * <y>` | Multiplication. | `Type_error`
`<x> / <y>` | Float division. | `Type_error`
`<x> ^ <y>` | Raise *x* to the power of *y*. | `Type_error`

### Bitwize (logical) operators
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

### Comparison operators
Operator | Description | Exceptions
---------|-------------|-----------
`<x> < <y>` | Less than. | `Type_error`
`<x> > <y>` | Greater than. | `Type_error`
`<x> <= <y>` | Less or equal. | `Type_error`
`<x> >= <y>` | Greater or equal. | `Type_error`
`<x> == <y>` | Equality. | `Type_error`
`<x> /= <y>` | Inequality. | `Type_error`

**Note:** so these will work ONLY for numbers, don't try to plug strings in there.

## Strings

## Symbols

## Nil

## Pairs and cons-lists

## Regular expressions
Regular expressions are using PCRE as backend, so refer to the PCRE documentation for the syntax of patterns.

### Syntax
`\<regexp>/[imsx]`
where
* **i** - caseless
* **m** - multiline mode
* **s** - '.' will match newline too
* **x** - extended mode

### Operators
Operator | Description | Exceptions
---------|-------------|-----------
`<string> =~ <regexp>` | Check if string matches regexp. | `Regexp_error`
`<string> >~ <regexp>` | Get all matches from the string. Return [false](booleans) if match failed. | `Regexp_error`

### Functions
Function | Description | Exceptions
---------|-------------|-----------
`split <regexp> <string>`| Split string at positions where regexp matches. Matched pattern is excluded from the string. Return a list of substrings. | `Regexp_error`
`rev_split <regexp> <string>` | Same as `split` but order of substring is reversed. | `Regexp_error`

## Ranges

## Tuples

## Records

## Variants

## Functions

# Builtin operators

# Exceptions and error handling

# IO

### Functions
Function | Description | Exceptions
---------|-------------|-----------
`open_in <path>` | Same as `fopen(<path>, "r")`. Return [file](file) object.| `Type_error`, `Invalid_argument`, `System_error <errno>`
`open_out <path>` | Same as `fopen(<path>, "w")`. Return [file](file) object. | `Type_error`, `Invalid_argument`, `System_error <errno>`
`open_append <path>`  | Same as `fopen(<path>, "a")`. Return [file](file) object. | `Type_error`, `Invalid_argument`, `System_error <errno>`
`open_pipe_in <command>` | Same as `popen(<command>, "r")`. Return [file](file) object. | `Type_error`, `Invalid_argument`, `System_error <errno>`
`open_pipe_out <command>` |  Same as `popen(<command>, "w")`. Return [file](file) object. | `Type_error`, `Invalid_argument`, `System_error <errno>`
`close <file>` | Close file. For files opened with `open_pipe_*` return exit status of the subprocess. (otherwize - [nil](nil)) | `Type_error`, `Invalid_argument`, `System_error <errno>`

#### Input
Function | Description | Exceptions
---------|-------------|-----------
`input <prompt>` | Print prompt and read a line from standard input. | `End_of_file`, `Type_error`, `Invalid_argument`, `System_error <errno>`
`read_of <file> <n>` | Read *n* bytes from the file to be returned as a string. | `End_of_file`, `Type_error`, `Invalid_argument`, `System_error UNDEFINED`
`read <n>` | Equivalent to `read_of stdin`. | `End_of_file`, `Type_error`, `Invalid_argument`, `System_error UNDEFINED`
`read_line_of <file>` | Read a line from the file. Trailing newline-character is NOT removed. | `End_of_file`, `Type_error`, `Invalid_argument`, `System_error <errno>`
`read_line !` | Equivalent to `read_line_of stdin` | `End_of_file`, `Type_error`, `Invalid_argument`, `System_error <errno>`
`read_file <file>` | Read whole line into a string. (ONLY WORKS WITH **FILES**) | `End_of_file`, `Type_error`, `Invalid_argument`, `System_error <errno>`

#### Output

