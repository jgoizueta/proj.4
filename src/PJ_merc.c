#define PJ_LIB__
#include "proj_internal.h"
#include "proj.h"
#include "projects.h"
#include <float.h>

PROJ_HEAD(merc, "Mercator") "\n\tCyl, Sph&Ell\n\tlat_ts=";
PROJ_HEAD(webmerc, "Web Mercator / Pseudo Mercator") "\n\tCyl, Sph\n\t";

#define EPS10 1.e-10

#if !defined(HAVE_C99_MATH)
#define HAVE_C99_MATH 0
#endif

#if HAVE_C99_MATH
#define log1px log1p
#define expm1x expm1
#else
static double log1px(double x) {
  volatile double
    y = 1 + x,
    z = y - 1;
  /* Here's the explanation for this magic: y = 1 + z, exactly, and z
   * approx x, thus log(y)/z (which is nearly constant near z = 0) returns
   * a good approximation to the true log(1 + x)/x.  The multiplication x *
   * (log(y)/z) introduces little additional error. */
  return z == 0 ? x : x * log(y) / z;
}
static double expm1x(double x) {
    /* very simplistic exmp1 implementation */
    if (fabs(x) <= DBL_EPSILON) {
        return x;
    }
    return exp(x) - 1.0;
}
#endif

static double logtanpfpim1(double x) {       /* log(tan(x/2 + M_FORTPI)) */
    if (fabs(x) <= DBL_EPSILON) {
        /* tan(M_FORTPI + .5 * x) can be approximated by  1.0 + x */
        return log1px(x);
    }
    return log(tan(M_FORTPI + .5 * x));
}

static double hpip2atanexp(double x) {       /* M_HALFPI - 2. * atan(exp(x)) */
    if (fabs(x) <= DBL_EPSILON) {
        /* atan(exp(x))  can be approximated by (exp(x)-1)/2 + pi/4 */
        return -expm1x(x));
    }
    return M_HALFPI - 2. * atan(exp(x));
}

static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10) {
        proj_errno_set(P, PJD_ERR_TOLERANCE_CONDITION);
        return xy;
    }
    xy.x = P->k0 * lp.lam;
    xy.y = - P->k0 * log(pj_tsfn(lp.phi, sin(lp.phi), P->e));
    return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    if (fabs(fabs(lp.phi) - M_HALFPI) <= EPS10) {
        proj_errno_set(P, PJD_ERR_TOLERANCE_CONDITION);
        return xy;
}
    xy.x = P->k0 * lp.lam;
    xy.y = P->k0 * logtanpfpim1(lp.phi);
    return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    if ((lp.phi = pj_phi2(P->ctx, exp(- xy.y / P->k0), P->e)) == HUGE_VAL) {
        proj_errno_set(P, PJD_ERR_TOLERANCE_CONDITION);
        return lp;
}
    lp.lam = xy.x / P->k0;
    return lp;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    lp.phi = hpip2atanexp(xy.y / P->k0);
    lp.lam = xy.x / P->k0;
    return lp;
}


PJ *PROJECTION(merc) {
    double phits=0.0;
    int is_phits;

    if( (is_phits = pj_param(P->ctx, P->params, "tlat_ts").i) ) {
        phits = fabs(pj_param(P->ctx, P->params, "rlat_ts").f);
        if (phits >= M_HALFPI)
            return pj_default_destructor(P, PJD_ERR_LAT_TS_LARGER_THAN_90);
    }

    if (P->es != 0.0) { /* ellipsoid */
        if (is_phits)
            P->k0 = pj_msfn(sin(phits), cos(phits), P->es);
        P->inv = e_inverse;
        P->fwd = e_forward;
    }

    else { /* sphere */
        if (is_phits)
            P->k0 = cos(phits);
        P->inv = s_inverse;
        P->fwd = s_forward;
    }

    return P;
}

PJ *PROJECTION(webmerc) {

    /* Overriding k_0, lat_0 and lon_0 with fixed parameters */
    P->k0 = 1.0;
    P->phi0 = 0.0;
    P->lam0 = 0.0;

    P->b = P->a;
    /* Clean up the ellipsoidal parameters to reflect the sphere */
    P->es = P->e = P->f = 0;
    pj_calc_ellipsoid_params (P, P->a, 0);
    P->inv = s_inverse;
    P->fwd = s_forward;
    return P;
}
