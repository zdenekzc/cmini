/* IFC.H - rozhrani vstupu a vystupu front-endu (inp.c, out.c) k ovladaci

   Funkce definuje ovladac (test_instr.cc):
      getInpText     zdrojovy text (ukonceny nulou), plati do konce prekladu
      clearOutLines  zacatek vystupu (OpenOut)
      showOutLine    jedna radka vystupu (bez konce radky)
      showError      chybove hlaseni - nevraci se (test_instr.cc: longjmp na setjmp
                     v ovladaci; ne vyjimka C++ - front-end v C nema .eh_frame)
*/

#ifndef IFC_H
#define IFC_H

#ifdef __cplusplus
extern "C" {
#endif

const char * getInpText (void);

void clearOutLines (void);
void showOutLine (const char * text);

void showError (const char * msg);

#ifdef __cplusplus
}
#endif

#endif /* IFC_H */
