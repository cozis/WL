#ifndef WL_EVAL_INCLUDED
#define WL_EVAL_INCLUDED

#ifndef WL_AMALGAMATION
#include "assemble.h"
#endif

// TODO: pretty sure this is unused
int eval(WL_Program p, WL_Arena *a, char *errbuf, int errmax);

#endif // WL_EVAL_INCLUDED
