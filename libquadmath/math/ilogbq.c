/* s_ilogbl.c -- long double version of s_ilogb.c.
 * Conversion to IEEE quad long double by Jakub Jelinek, jj@ultra.linux.cz.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: $";
#endif

/* ilogbl(long double x)
 * return the binary exponent of non-zero x
 * ilogbl(0) = FP_ILOGB0
 * ilogbl(NaN) = FP_ILOGBNAN (no signal is raised)
 * ilogbl(+-Inf) = INT_MAX (no signal is raised)
 */

#include <math.h>
#include "quadmath-imp.h"
#ifndef FP_ILOGB0
# define FP_ILOGB0 INT_MIN
#endif
#ifndef FP_ILOGBNAN
# define FP_ILOGBNAN INT_MAX
#endif

int ilogbq (__float128 x)
{
	int64_t hx,lx;
	int ix;

	GET_FLT128_WORDS64(hx,lx,x);
	hx &= 0x7fffffffffffffffLL;
	if(hx <= 0x0001000000000000LL) {
	    if((hx|lx)==0)
		{
#ifndef UNDER_CE
			errno = EDOM;
#endif
			feraiseexcept (FE_INVALID);
			return FP_ILOGB0;
		}	/* ilogbl(0) = FP_ILOGB0 */
	    else			/* subnormal x */
		if(hx==0) {
		    for (ix = -16431; lx>0; lx<<=1) ix -=1;
		} else {
		    for (ix = -16382, hx<<=15; hx>0; hx<<=1) ix -=1;
		}
	    return ix;
	}
	else if (hx<0x7fff000000000000LL) return (hx>>48)-0x3fff;
	else if (FP_ILOGBNAN != INT_MAX) {
	    /* ISO C99 requires ilogbl(+-Inf) == INT_MAX.  */
	    if (((hx^0x7fff000000000000LL)|lx) == 0)
		{
#ifndef UNDER_CE
			errno = EDOM;
#endif
			feraiseexcept (FE_INVALID);
			return INT_MAX;
		}
	}
	{
#ifndef UNDER_CE
		errno = EDOM;
#endif
		feraiseexcept (FE_INVALID);
		return FP_ILOGBNAN;
	}
}
