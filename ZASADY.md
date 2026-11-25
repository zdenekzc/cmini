# Zásady projektu (pro všechny podprojekty)

Tyto zásady platí pro celý strom (`cmini`, `asm`, `mini-commander`, `tiny-ide`,
`toaruos-tools`, `cstencils`, `tests`). README odkazuje sem.

## Makefile

- `Makefile` s velkým M je **generovaný** — obvykle skriptem `*-makefiles.py`
  (`cmini-makefiles.py` generuje `Makefile`, `asm/Makefile`, `cmini/Makefile`).
  Ručně se needituje; změna se dělá v generátoru a pak se spustí znovu.
- `makefile` s malým m je **ručně psaný** (např. `mini-commander/Makefile`,
  `tiny-ide/Makefile`, pokud nemají hlavičku „generated“).
- Výjimka v rámci tohoto stromu: `cmini/Makefile` a `asm/Makefile` jsou generované
  (`cmini-makefiles.py`), i když jsou v gitu.

## Generované soubory a git

- Platí pro **navazující projekty** (mimo `cmini/` a `asm/`): přípony `.hpp` a `.cpp`
  jsou **generované**. Obvykle se nedávají do gitu.
- Výjimka: důležité generované parsery, pokud je má projekt v gitu mít.
- **Soubory v `cmini/` a `asm/` jsou ruční** (včetně `.h`, `.c`, `.cc`), nejsou generované.

## Adresáře

| Adresář | Obsah | V gitu |
|---|---|---|
| `_import/` | stahovaná data (ISO ToaruOS, zdrojáky třetích stran) | ne (`.gitignore`) |
| `_build/` | všechno pracovní a binární, **každý nástroj do vlastního podadresáře** (`_build/cmini`, `_build/vystup/...`). Přímo v `_build/` jsou jen **kernel, initrd (ramdisk) a iso** pro ToaruOS | ne |
| `_output/` | výsledky testů určené uživateli (studentům) | ne |

Pravidla pro `_output/`:

- Kopírovat i zdrojové texty příkladů (`.cc`, `.asm`), aby byly vidět spolu s výstupem.
- Žádné skripty v `_output` nejsou. Pro nové přeložení stačí smazat `_output` a spustit
  testy (`make vystup`, nebo jednotlivé `tests/0*.sh`), které ho vytvoří znovu.
- Žádné stahování.

## Testy

- Testy v `tests/` (`01-…`, `build-…`, `run-…`) musí fungovat i při **smazaném
  `_build` a `_import`** — samy si to vytvoří, nebo to jasně řeknou.
- Když chybí balíček nebo nástroj (gcc, qemu, wine, fakeroot, …), test to **výslovně
  upozorní** (co chybí a jak to doinstalovat), místo tichého selhání.

## ISO pro ToaruOS

- Před vytvořením `.iso` zkontrolovat, zda je nainstalován **`fakeroot`**.
- Zkontrolovat, zda má **`sudo` v ISO setuid bit** (`stat -c %a`, mode `4755`, vlastník root).
- Pokud chybí fakeroot nebo setuid bit, vypsat **dobře viditelné varování**
  (bloky z `!!!`/`WARNING`, na stderr), protože nevíme, zda půjde ve školních
  učebnách fakeroot doinstalovat.

## Jazyky a překladače

- **CMini** (také „CMini“) je jednoduché C s třídami. Zdrojové soubory jsou `.cc` a `.h`.
  Slouží třem účelům:
  a) ukázka překladače bez použití gramatik,
  b) reference pro podmnožinu C++, kterou používají navazující projekty, které nemusíme psát v čistém C,
  c) překladač, který zajišťuje nezávislost našich projektů na plném C++.
- **Bootstrap** (první překlad CMini bez předchozího CMini) použije jeden z C překladačů,
  první nalezený v tomto pořadí: **TinyCC**, **ChibiCC**, **GCC** (jen C).
- Stav kódu: `cmini-makefiles.py` a `Makefile` zatím používají `gcc` napevno a
  bootstrap pro TinyCC/chibicc je jen samostatný cíl `test-tcc`. Automatický výběr
  podle pořadí ještě není implementovaný.

## Překladač cmm (C--, Cmm)

- Je v navazujících projektech, v adresáři `cmm/`, řízený gramatikou `cmm.g`.
- Napsaný v jazyce, který zvládne CMini. Výjimka: moduly, které používají AsmJit,
  LLVM IR, Qt a podobné rozsáhlé knihovny, se píšou v C++ a překládají GCC C++ nebo Clangem.
- Generování parserů, datových struktur pro AST a tree walkeru, který vytváří původní
  zdrojový text (soubory `*product*`), zajišťuje generátor v Pythonu v adresářích `grm/` a `code/`.
- Později bude existovat také generátor parserů napsaný v CMini v adresáři `gram/`
  a `utils/` (adresář `utils` se už objevuje v `cstencils`).

## cstencils a CDL (Compiler Definition Language)

- `cstencils/` je experiment napsaný v CMini. Z deklarací tříd generuje například
  spojové seznamy.
- `cstencils/code.h` obsahuje třídy `Class`, `Func`, ..., které tvoří základ pro
  generování zdrojových textů v parser generátoru napsaném v C-- (`gram/`).
- Z tohoto experimentu má vzniknout základ pro **CDL** (Compiler Definition Language,
  také `cdl`). Plán je zmíněný v projektu arm64-compiler.
- CDL nesmí záviset na parser generátoru. Proto se do projektu cecko zatím kopírují
  všechny potřebné soubory parser generátoru do adresáře `gram/`, aby `gram/` byl stabilní.

## Varianty back-endu CMini

- **a) pasm**: `asm/*` a `cmini/instr.c`. CMini skládá program jako seznam uzlů
  z `asm/asmlist.h` (`AsmProgram`, `AsmIns2`, `AsmLabel`, ...) a pasm ho přečte přímo
  (`AsmAssemble`, fáze 2). Text pro assembler (`AsmPrint`, `-o x.asm`, `-S`) je jen
  výstup navíc. Pro srovnání existuje fáze 1: `CPARSER_ASMLIST=text` (`AsmAssembleText`).
  Na x86_64 Linux je to **preferovaná varianta**.
- **b) translate**: `cmini/translate.c`. CMini přeloží zdroj na prostý C. Používá se na
  jiných architekturách a OS (např. ToaruOS pro Arm64). Na x86_64 Linux se používá jen
  v testech této varianty.
- **c) AsmJit**: `cmini/instr1.cc` … `instr5.cc`. Zůstává jen jako ilustrace vývoje
  CMini. **Netestuje se.**

## Stav kódu vůči těmto zásadám

- `toaruos-tools/mkramdisk.sh` kontroluje `fakeroot` a po sestavení archivu kontroluje,
  že `bin/sudo` má setuid bit a vlastníka root (`tar -tvzf`). Při chybě vypíše
  varování v rámečku `!!!`. `.iso` zatím skript nevytváří (vytváří ramdisk),
  takže pravidlo se uplatní na ramdisk; až vznikne skript na `.iso`, dát tam stejnou kontrolu.
- Ostatní body zatím **neověřeny** (`_output` s kopiemi zdrojů a skriptem na
  přegenerování, testy při smazaném `_build`/`_import`). Ověří se při práci s nimi.
