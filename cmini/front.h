/* FRONT.H - front-end setup shared by the cmini drivers (front.c) */

#ifndef FRONT_H
#define FRONT_H

#ifdef __cplusplus
extern "C" {
#endif

/* loads the source src and prepares the lexer and declarations; 0 = ok */
int  TrStart (const char * src);

/* -D name[=value] */
void TrDefine (char * arg);

#ifdef __cplusplus
}
#endif

#endif
