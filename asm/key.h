/* KEY.H */

/* klicova slova assembleru (prevedeno z key.pas) */

#ifndef KEY_H
#define KEY_H

enum keyword
{
   /* Directives */
   ip186, ip286, ip286c, ip286p, ip287, ip386,
   ip386c, ip386p, ip387, ip8086, ip8087, ipX64,
   iAlign,
   iAssume, iComment, iDb, iDd, iDf, iDq,
   iDt, iDw, iEnd, iEndp, iEnds, iEqu,
   iEven, iExtrn, iGroup, iLabel, iOrg, iProc,
   iPublic, iRecord, iSegment, iStruc,
   /* Intel 8086 */
   iAaa, iAad, iAam, iAas, iAdc, iAdd,
   iAnd, iCall, iCbw, iClc, iCld, iCli,
   iCmc, iCmp, iCmps, iCmpsb, iCmpsw, iCwd,
   iDaa, iDas, iDec, iDiv, iEsc, iHlt,
   iIdiv, iImul, iIn, iInc, iInt, iInto,
   iIret, iJa, iJae, iJb, iJbe, iJc,
   iJcxz, iJe, iJg, iJge, iJl, iJle,
   iJmp, iJna, iJnae, iJnb, iJnbe, iJnc,
   iJne, iJng, iJnge, iJnl, iJnle, iJno,
   iJnp, iJns, iJnz, iJo, iJp, iJpe,
   iJpo, iJs, iJz, iLahf, iLds, iLea,
   iLes, iLock, iLods, iLodsb, iLodsw, iLoop,
   iLoope, iLoopne, iLoopnz, iLoopz, iMov, iMovs,
   iMovsb, iMovsw, iMul, iNeg, iNop, iNot,
   iOr, iOut, iPop, iPopf, iPush, iPushf,
   iRcl, iRcr, iRep, iRepe, iRepne, iRepnz,
   iRepz, iRet, iRetf, iRetn, iRol, iRor,
   iSahf, iSal, iSar, iSbb, iScas, iScasb,
   iScasw, iShl, iShr, iStc, iStd, iSti,
   iStos, iStosb, iStosw, iSub, iTest, iWait,
   iXchg, iXlat, iXor,
   /* Intel 8087 */
   iF2xm1, iFabs, iFadd, iFaddp, iFbld, iFbstp,
   iFchs, iFclex, iFcom, iFcomp, iFcompp, iFdecstp,
   iFdisi, iFdiv, iFdivp, iFdivr, iFdivrp, iFeni,
   iFfree, iFiadd, iFicom, iFicomp, iFidiv, iFidivr,
   iFild, iFimul, iFincstp, iFinit, iFist, iFistp,
   iFisub, iFisubr, iFld, iFld1, iFldcw, iFldenv,
   iFldl2e, iFldl2t, iFldlg2, iFldln2, iFldpi, iFldz,
   iFmul, iFmulp, iFnclex, iFndisi, iFneni, iFninit,
   iFnop, iFnrstor, iFnsave, iFnstcw, iFnstenv, iFnstsw,
   iFpatan, iFprem, iFptan, iFrndint, iFrstor, iFsave,
   iFscale, iFsqrt, iFst, iFstcw, iFstenv, iFstp,
   iFstsw, iFsub, iFsubp, iFsubr, iFsubrp, iFtst,
   iFwait, iFxam, iFxch, iFxtract, iFyl2x, iFyl2xp1,
   /* Intel 80186 */
   iBound, iEnter, iIns, iInsb, iInsw, iLeave,
   iOuts, iOutsb, iOutsw, iPopa, iPusha,
   /* Intel 80286 */
   iArpl, iClts, iLar, iLgdt, iLidt, iLldt,
   iLmsw, iLsl, iLtr, iSgdt, iSidt, iSldt,
   iSmsw, iStr, iVerr, iVerw,
   /* Intel 80287 */
   iFsetpm,
   /* Intel 80386 */
   iBsf, iBsr, iBt, iBtc, iBtr, iBts,
   iCdq, iCmpsd, iCwde, iInsd, iIretd, iJecxz,
   iLfs, iLgs, iLodsd, iLss, iMovsd, iMovsx,
   iMovzx, iOutsd, iPopad, iPopfd, iPushad, iPushfd,
   iScasd, iSeta, iSetae, iSetb, iSetbe, iSetc,
   iSete, iSetg, iSetge, iSetl, iSetle, iSetna,
   iSetnae, iSetnb, iSetnbe, iSetnc, iSetne, iSetng,
   iSetnge, iSetnl, iSetnle, iSetno, iSetnp, iSetns,
   iSetnz, iSeto, iSetp, iSetpe, iSetpo, iSets,
   iSetz, iShld, iShrd, iStosd, iXlatb,
   /* Intel 80387 */
   iFcos, iFprem1, iFsin, iFsincos, iFucom, iFucomp,
   iFucompp,
   /* x86-64 */
   iCdqe, iCmpsq, iCqo, iJrcxz, iLodsq, iMovsq,
   iMovsxd, iScasq, iStosq, iSyscall,
   /* SSE, SSE2 (skalarni operace, registry XMM) */
   iAddsd, iAddss, iAndnpd, iAndnps, iAndpd, iAndps, iComisd, iComiss,
   iCvtsd2si, iCvtsd2ss, iCvtsi2sd, iCvtsi2ss, iCvtss2sd, iCvtss2si,
   iCvttsd2si, iCvttss2si, iDivsd, iDivss, iMaxsd, iMaxss, iMinsd, iMinss,
   iMovapd, iMovaps, iMovd, iMovq, iMovss, iMulsd, iMulss, iOrpd, iOrps,
   iPxor, iSqrtsd, iSqrtss, iSubsd, iSubss, iUcomisd, iUcomiss, iXorpd,
   iXorps,
   /* Operators */
   ipType, iEq, iGe, iGt, iHigh, iLe,
   iLength, iLow, iLt, iMask, iMod, iNe,
   iOffset, iPtr, iSeg, iShort, iSize, iSt,
   iThis, iType, iWidth,
   /* Tokens */
   iAbs, iAt, iByte, iCommon, iDup, iDword,
   iFar, iFword, iMemory, iNear, iNothing, iPara,
   iQword, iStack, iTbyte, iUse16, iUse32, iUse64, iWord,
   /* Conditional Directives */
   ipErr, ipErr1, ipErr2, ipErrb, ipErrdef, ipErrdif,
   ipErrdifi, ipErre, ipErridn, ipErridni, ipErrnb, ipErrndef,
   ipErrnz, iElse, iEndIf, iIF, iIF1, iIF2,
   iIFb, iIFdef, iIFdif, iIFdifi, iIFe, iIFidn,
   iIFidni, iIFnb, iIFndef,
   /* Macro Directives */
   iEndm, iExitm, iIrp, iIrpc, iLocal, iMacro,
   iPurge, iRept,
   /* Listing Directives */
   ipCref, ipLall, ipLfcond, ipList, ipRadix, ipSall,
   ipSfcond, ipTfcond, ipXall, ipXcref, ipXlist, iInclude,
   iName, iPage, iSubttl, iTitle,
   /* Directives */
   ipAlpha, ipCode, ipConst, ipData, iqData, ipFarData,
   iqFarData, ipModel, ipMsFloat, ipSeq, ipStack, iComm,
   iDosseg, iIncludeLib,
   /* Special */
   iPeriod, iDollar, iQuestion, iOther
};

#ifndef __cplusplus
typedef enum keyword keyword;    /* C: jmeno vyctu neni typ (asmlist.c) */
#endif

#define KeyMax 488

typedef struct
{
   keyword k;
   char    n [10 + 1];
}
   KeyRec;

/* KeyTable [1..KeyMax] - serazeno podle jmena (index 0 se nepouziva) */
extern const KeyRec KeyTable [KeyMax + 1];

#endif /* KEY_H */
