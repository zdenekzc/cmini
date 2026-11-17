/* util.c - obycejne C (gcc), vola funkci prelozenou cparserem (tests/examples/multi) */

#ifdef __cplusplus
extern "C" {
#endif
int cp_callback (int x);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

int c_twice (int x)
{
   return 2 * x;
}

int c_apply (int x)
{
   return cp_callback (x) * 2;
}

#ifdef __cplusplus
}
#endif
