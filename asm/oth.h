/* OTH.H */

/* vystupni zaznamy a direktivy (prevedeno z oth.pas) */

#ifndef OTH_H
#define OTH_H

#include "key.h"
#include "def.h"
#include "expr.h"

#define DataMax 20

enum ClassTyp { Data, Fixup, RepBeg, RepEnd, NewPos, LineBeg, LineEnd };
typedef enum ClassTyp ClassTyp;
enum FixTyp   { FixOfs, FixSeg, FixNear, FixFar, FixRel };   /* FixRel - relativni k RIP */
typedef enum FixTyp FixTyp;

typedef struct ResultRec ResultRec;
typedef ResultRec * ResultPtr;

/* variantni zaznam z Pascalu - vsechny varianty jako samostatne polozky,
   Pascal: class -> cls */
struct ResultRec
{
   ResultPtr next;
   ClassTyp  cls;

   /* Data */
   small     DataLen;
   small     DataSeq [DataMax + 1];   /* DataSeq [1..DataMax] */

   /* Fixup */
   FixTyp    FixSort;
   card      FixWidth;
   TreePtr   FixFrame;
   TreePtr   FixTarget;
   TreePtr   FixAdd;
   longint   FixVal;
   bool      Fix64;      /* 8bajtova polozka v segmentu USE64 - hodnota FixVal64 */
   int64_t   FixVal64;

   /* RepBeg */
   longint   count;

   /* NewPos */
   ObjInx    NewInx;
   bool      New32;
   longint   NewOfs;
};

extern ResultPtr FirstRes;
extern ResultPtr LastRes;
extern ResultPtr Entry;

void InitRes (void);
void Put     (small b, char c);
void PutData (ExprRec * e, card width);
void PutNear (ExprRec * e, card width);
void PutRel  (ExprRec * e, card width, card extra);
void PutFar  (ExprRec * e, card width);
void PutLine (bool start);
void SegmentDir (void);
void EndsDir (void);
void ProcDir (void);
void EndpDir (void);
void EndDir (void);
void GroupDir (void);
void ExtrnDir (void);
void PublicDir (void);
void LabelDir (void);
void EquDir (void);
void OrgDir (void);
void AssumeDir (void);
void AlignDir (void);
void EvenDir (void);
void DefDir (card t);

#endif /* OTH_H */
