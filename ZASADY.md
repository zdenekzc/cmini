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
- Přidat jednoduchý skript, který přeložené příklady **smaže a znovu zkompiluje**.
- `_output` musí fungovat bez složitých závislostí a bez stahování (jen to, co je
  v `_build`, a nástroje, které uživatel má).

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

## Stav kódu vůči těmto zásadám

- `toaruos-tools/mkramdisk.sh` kontroluje `fakeroot` a po sestavení archivu kontroluje,
  že `bin/sudo` má setuid bit a vlastníka root (`tar -tvzf`). Při chybě vypíše
  varování v rámečku `!!!`. `.iso` zatím skript nevytváří (vytváří ramdisk),
  takže pravidlo se uplatní na ramdisk; až vznikne skript na `.iso`, dát tam stejnou kontrolu.
- Ostatní body zatím **neověřeny** (`_output` s kopiemi zdrojů a skriptem na
  přegenerování, testy při smazaném `_build`/`_import`). Ověří se při práci s nimi.
