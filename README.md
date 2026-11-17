# cmini, pasm and the tools around them

cmini is a compiler for a subset of C++ (classes, inheritance, virtual methods,
references, templates `Sequence<T>` and `Dictionary<K,V>`, `std::string`, `cout`)
to x86-64 code. It is written in C and compiles itself (self-hosting).

pasm is the assembler it uses. It reads the assembler text and writes ELF64,
Windows PE or COFF object files and runnable programs.

The development history (one chapter per change, numbered 0 - 86 and more) is
in [CHANGELOG.txt](CHANGELOG.txt).

## What cmini can translate

The sources in `tests/examples/*.cc` show the supported language. Most of them
are compiled, run and their output is compared with `tests/examples/expected/`
(`tests/build-test.sh`).

| Source | Shows |
|---|---|
| `quicksort.cc`, `heapsort.cc` | arrays, references, recursion, `inline` |
| `dama.cc`, `jezdec.cc`, `avl.cc`, `btree.cc`, `veze.cc` | structs, pointers, `new`, constructors |
| `classes.cc`, `dyncast.cc` | inheritance, virtual methods, `dynamic_cast` |
| `dtors.cc` | destructors, objects by value |
| `floats.cc` | `float`, `double` (SSE2) |
| `pointers.cc` | pointers and function pointers |
| `strings.cc` | `std::string` |
| `sequence.cc`, `dictionary.cc` | `Sequence<T>`, `Dictionary<K,V>` |
| `cout.cc` | `cout << a << b << endl;` |
| `preproc.cc` | `#include`, `#define` (with `LEXER=lexer`) |
| `intmodel.cc`, `chars.cc` | integer and character types |
| `objcopy.cc`, `multi/` | several source files (object files, linking) |

A short example, classes from `tests/examples/dyncast.cc` (abridged):

```cpp
class Shape                              /* abstraktni */
{
   public:
      int id;
      Shape () { id = 0; }
      virtual double area () = 0;
      virtual const char * name () { return "shape"; }
      virtual ~Shape () { }
};

class Circle : public Shape
{
   public:
      double r;
      Circle () { r = 1; }
      double area () { return 3 * r * r; }
      const char * name () { return "circle"; }
};
```

cmini does not support general templates. It supports the built-in types
`string`, `Sequence<T>` and `Dictionary<K,V>`, which need no header and no
template declaration. A small example:

```cpp
#include <stdio.h>
#include <string>
using namespace std;

int main ()
{
    Sequence <string> words;
    words.add ("alpha");
    words.add ("beta");
    words.add ("alpha");

    Dictionary <string, int> count;
    for (string w : words)
        count [w] ++;

    printf ("%d words, %d distinct\n", (int) words.size (), (int) count.size ());
    printf ("alpha: %d\n", count.get ("alpha"));
    printf ("second: %s\n", words [1].c_str ());
    return 0;
}
```

Its output:

```
3 words, 2 distinct
alpha: 2
second: beta
main returned 0
```

Output with `cout` and `cerr` is translated to `printf` and `dprintf` (see
`tests/examples/cout.cc`). The rest of the C++ library is not available: from
`namespace std` only `string` is supported, so `fstream` and general iostream
streams are rejected by the compiler.

Compile and run the whole `dyncast.cc` (`cmini.bin` runs the program at once and
prints `main returned N`). Its output is compared with `tests/examples/expected/dyncast.txt`:

```sh
./cmini/cmini.bin tests/examples/dyncast.cc
```

## Build and tests

Build everything (creates `asm/pasm.bin`, `asm/libpasm.a`, `cmini/cmini.bin`, ...):

```sh
python3 cecko-makefiles.py      # generates Makefile (once, or after adding files)
make
```

Quick test set (about 1 minute):

```sh
make test-quick
```

Complete set without ToaruOS in QEMU:

```sh
make test-all
```

ToaruOS in QEMU (needs qemu and the ToaruOS image):

```sh
make test-toaru-all
```

Variants of the build (system asmjit, lexer with preprocessor) and the details
of each script are in chapter 0 of [CHANGELOG.txt](CHANGELOG.txt).

## Compiling a single file

Full list of switches: `./cmini/cmini.bin -h`.

```sh
./cmini/cmini.bin -o prog prog.cc      # executable prog, assembler text prog.asm, listing prog.lst
./cmini/cmini.bin -c prog.o prog.cc    # ELF64 object file (for gcc / glibc)
./cmini/cmini.bin -C prog.cc           # translate cmini to C (output to stdout, -o out.c)
./asm/pasm.bin -e prog.asm             # assemble an assembler file to an executable
```

## Directories

| Directory | Contents |
|---|---|
| `cmini/` | the compiler: lexer, parser (`decl.c`, `expr.c`, `sem.c`), generator `instr.c`, translator to C (`translate.c`), runtime (`cminirt.c`) |
| `asm/` | the assembler pasm and its library `libpasm.a`, tests in `asm/tests` |
| `tests/` | test scripts (`01-04`, `build-*.sh`, `output.sh`), `tests/examples/*.cc` with expected outputs in `tests/examples/expected/` |
| `toaruos-tools/` | cmini and pasm for the ToaruOS operating system (guest sources, `check.sh`) |
| `_output/` | files for students, created by `make vystup` (not in backups) |
| `_build/` | object files and test outputs (created by `make`, removed by `make clean`) |

## More

- [CHANGELOG.txt](CHANGELOG.txt) - history of changes, chapter by chapter
- [asm/README.txt](asm/README.txt) - the assembler pasm (16/32/64 bit, PE, COFF, ELF)
- [toaruos-tools/README.md](toaruos-tools/README.md) - the ToaruOS version
