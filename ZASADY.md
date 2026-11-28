# Zásady projektu (pro všechny podprojekty)

Tyto zásady platí pro celý strom (`cmini`, `asm`, `mini-commander`, `tiny-ide`,
`toaruos-tools`, `cstencils`, `tests`). README odkazuje sem.

## Makefile

- `Makefile` s velkým M je **generovaný** — obvykle skriptem `*-makefiles.py`
  (`cmini-makefiles.py` generuje `Makefile`, `asm/Makefile`, `cmini/Makefile`).
  Ručně se needituje; změna se dělá v generátoru a pak se spustí znovu.
- `makefile` s malým m je **ručně psaný** (např. `mini-commander/makefile`,
  `tiny-ide/makefile`).
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
| `_build/` | všechno pracovní a binární, **v hlavním adresáři** (ne v `toaruos-tools/_build`), **každý nástroj do vlastního podadresáře** (`_build/cmini`, `_build/asm`, `_build/tiny-ide/...`, `_build/mini-commander/...`, `_build/vystup/...`, `_build/cross/...`). Přímo v `_build/` jsou jen **kernel, initrd (ramdisk) a iso** pro ToaruOS | ne |
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

## Jazyk komentářů a textů

- **Nové komentáře a texty** v kódu píšeme **anglicky**: komentáře v `.c`/`.h`/`.cc`/`.py`/`.sh`,
  hlavičky souborů, chybové a nápovědné zprávy skriptů i testů, dokumentace skriptů.
- Platí pro celý strom (`cmini`, `asm`, `mini-commander`, `tiny-ide`, `toaruos-tools`,
  `cstencils`, `tests`) bez výjimek.
- Česky zůstávají jen texty, které už existují (historické části README, `CHANGELOG.txt`,
  tyto zásady).

## Překladač cmm (C--, Cmm) a CDL - pro všechny projekty

- Překladač **cmm** (C--) je řízený gramatikou `cmm.g`, v adresáři `cmm/`.
- Napsaný v jazyce, který zvládne CMini. Výjimka: moduly, které používají AsmJit,
  LLVM IR, Qt a podobné rozsáhlé knihovny, se píšou v C++ a překládají GCC C++ nebo Clangem.
- Generování parserů, datových struktur pro AST a tree walkeru, který vytváří původní
  zdrojový text (soubory `*product*`), zajišťuje generátor v Pythonu v adresářích `grm/` a `code/`.
- Později bude existovat také generátor parserů napsaný v CMini v adresáři `gram/`
  a `utils/` (adresář `utils` se už objevuje v `cstencils`).
- **CDL** (Compiler Definition Language, také `cdl`) nesmí záviset na parser generátoru.
  Proto se do projektu cecko zatím kopírují všechny potřebné soubory parser generátoru
  do adresáře `gram/`, aby `gram/` byl stabilní.
- V tomto projektu (`cmini`) se cmm, grm, gram ani CDL **nepoužívají** a nepatří sem:
  nepřidávají se soubory, skripty ani testy, které na ně závisí.

## cminirt (behova knihovna)

- Adresar `cminirt/` v hlavnim adresari: `cminirt.c` a `cminirt.h`. Pouziva ho cmini (programy
  z `-C`, `cmini-translate.bin` s `-C`) i prekladac cmm v navazujicich projektech.
- Prekladace a skripty ho berou z `cminirt/`; `-I cminirt` je nutne, kdekoli se prekladaji programy
  z `-C` (`#include "cminirt.h"`).

## cstencils

- `cstencils/` je experiment napsaný v CMini. Z deklarací tříd generuje například spojové seznamy.
  Je součástí tohoto projektu (testy 03 a 04).
- `cstencils/code.h` obsahuje třídy `Class`, `Func`, ..., které tvoří základ pro generování
  zdrojových textů v parser generátoru napsaném v C-- (`gram/`). Z tohoto experimentu má
  vzniknout základ pro CDL (viz výše).

## Varianty back-endu CMini

- **a) pasm**: `asm/*` a `cmini/instr.c`. CMini skládá program jako seznam uzlů
  z `asm/asmlist.h` (`AsmProgram`, `AsmIns2`, `AsmLabel`, ...) a pasm ho přečte přímo
  (`AsmAssemble`, fáze 2). Text pro assembler (`AsmPrint`, `-o x.asm`, `-S`) je jen
  výstup navíc. Pro srovnání existuje fáze 1: `CPARSER_ASMLIST=text` (`AsmAssembleText`).
  Na x86_64 Linux je to **preferovaná varianta**. `cmini.bin` je **vždy** tato varianta
  (asm/asmlist); `-C` v něm není.
- **b) translate**: `cmini/translate.c`, samostatný program `cmini-translate.bin`
  (ovladač `translate_drv.c`). CMini přeloží zdroj na prostý C. Používá se na
  jiných architekturách a OS (např. ToaruOS pro Arm64). Na x86_64 Linux se používá jen
  v testech této varianty. Není součástí `make all`; přeloží ho až testy, které ho
  skutečně potřebují (`make -C cmini cmini-translate.bin`).
- **c) AsmJit**: `cmini/instr1.cc` … `instr5.cc`. Zůstává jen jako ilustrace vývoje
  CMini. **Netestuje se.**

## Attribution v commitech a PR

- Do commit zpráv **neuvádět** `Co-Authored-By: Claude Code`.
- Místo toho uvést řádek `Assisted-by:` s názvem použitého modelu nebo nástroje, např.:
  - `Assisted-by: DeepSeek Thinking (LLM.ai e-INFRA CZ)`
  - `Assisted-by: <jiný název modelu> (LLM.ai e-INFRA CZ)`
  - `Assisted-by: Claude Code`
- Řádek se píše na konec zprávy commitu (a na konec popisu PR), po prázdném řádku.

## Stav kódu vůči těmto zásadám

- `toaruos-tools/mkramdisk.sh` kontroluje `fakeroot` a po sestavení archivu kontroluje,
  že `bin/sudo` má setuid bit a vlastníka root (`tar -tvzf`). Při chybě vypíše
  varování v rámečku `!!!`. `.iso` zatím skript nevytváří (vytváří ramdisk),
  takže pravidlo se uplatní na ramdisk; až vznikne skript na `.iso`, dát tam stejnou kontrolu.
- Ostatní body zatím **neověřeny** (`_output` s kopiemi zdrojů a skriptem na
  přegenerování, testy při smazaném `_build`/`_import`). Ověří se při práci s nimi.
