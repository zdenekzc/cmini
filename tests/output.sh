#!/bin/sh
# output.sh (make vystup) - vznikle soubory pro studenty v adresari _output/:
# spusti postupne vsechny tests/NN-*.sh (kazdy vytvori jeden podadresar
# _output/<generator>, lze je spoustet i samostatne) a napise _output/README.txt.
# Prekladace musi byt prelozene (make vystup => make all).  Test 04 (ToaruOS) bezi
# bez okna QEMU: output.sh nastavi TOARU_GUI=0.
#
#   a) 01-asm          asm/tests/*.asm - assembler pasm
#   b) 02-cmini-pasm    c-mini => pasm (cmini/instr.c): priklady, asm, cmini (sam sebou)
#   c) 03-cmini-translate      totez pres C (cmini/translate.c)
#   d) 04-cstencils    cstencils prelozeny cmini (glibc, pasm) = g++ build
#   e) 05-toaru         cmini a pasm v ToaruOS (VM, bez okna); bez QEMU/ISO se preskoci
#
# Adresar se nezalohuje - kdykoli se vytvori znovu.

cd "$(dirname "$0")/.." || exit 1
TOP=$(pwd)
if [ "${VYSTUP_TOARU:-1}" = 0 ]
then   # vystupy ToaruOS (04) z drivejsiho behu zustanou
   find _output -mindepth 1 -maxdepth 1 ! -name 'toaruos*' -exec rm -rf {} + 2> /dev/null
   rm -rf _build/vystup
else
   rm -rf _output _build/vystup
fi
mkdir -p _output || exit 1

# okno QEMU (test 04) jen pri samostatnem spusteni testu
TOARU_GUI=0
export TOARU_GUI

chyby=""
# VYSTUP_TOARU=0 (make test-all) - bez testu ToaruOS v QEMU (04)
for s in tests/[0-9][0-9]-*.sh
do
   case "${VYSTUP_TOARU:-1}:$s" in
      0:tests/05-toaru*) echo "  (vynechano: $s - VYSTUP_TOARU=0)"; continue ;;
   esac
   sh $s || { echo "  CHYBA  $s"; chyby="$chyby $(basename $s .sh)"; }
done

cat > $TOP/_output/README.txt <<'EOT'
Vznikle soubory - vytvari tests/output.sh (make vystup) pomoci skriptu
tests/01-asm.sh, 02-cmini-pasm.sh, 03-cmini-translate.sh, 04-cstencils.sh, 05-toaru.sh (kazdy lze
spustit i samostatne - prepise jen svuj podadresar); adresar se nezalohuje,
kdykoli se vytvori znovu.

Kazdy generator ma podadresare:
  examples/   testovaci programy tests/examples/*.cc:
              vygenerovany text, spustitelny soubor .bin (kde to jde) a jeho vystup .out
              (vzor tests/examples/expected/*.txt; quicksort a heapsort maji nahodna
              data - kontroluji se samy, radek O.K.)
  cmini/ asm/
              zdrojove texty prekladace, ktere generator zvladne prelozit;
              soubor je zde jen tehdy, kdyz vysledek prelozi i gcc
              (podadresar chybi, kdyz se nepovede nic)

asm/examples/     priklady pro assembler pasm (asm/tests): .lst vypis,
                  .bin ELF (hello32, hello64; libc64, sse64 slinkovane gcc)
cmini-pasm/        c-mini (cmini/instr.c): text pro assembler pasm (.asm),
                  vypis se strojovym kodem (.lst)
                  cmini/cmini.bin - prekladac c-mini (instr_drv.c, front-end, instr.c)
                                  prelozeny sam sebou: stejny text pro assembler
                                  jako verze z g++ (i na svych zdrojich - pevny bod)
                  cmini/cmini.bin -C - translate.c + front-end prelozeny c-mini
                  cmini/, asm/ - po souborech (test_instr_lexer.bin -0 -c -S, objekt
                                 prelozi pasm); asm/pasm.bin - assembler prelozeny c-mini
cmini-translate/          c-mini => C (cmini/translate.c), behova knihovna cminirt/cminirt.c
                  cmini/cmini.bin -C - translate.c prelozeny sam sebou;
                                    na svych zdrojich da stejny C (pevny bod)
toaruos/      cmini a pasm v ToaruOS (tests/05-toaru.sh): programy
                  prelozene v ToaruOS, vystup se porovna se vzorem
EOT

rm -rf _build/vystup
[ -z "$chyby" ] && exit 0
echo "output.sh: CHYBA v$chyby"   # make test-all selze
exit 1
