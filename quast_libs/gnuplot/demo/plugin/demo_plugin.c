/*
  The files demo_plugin.c and gnuplot_plugin.h serve as templates
  showing how to create an external shared object containing functions
  that can be accessed from inside a gnuplot session using the import
  command.  For example, after compiling this file into a shared
  object (gnuplot_plugin.so or gnuplot_plugin.dll) the functions it
  contains can be imported as shown:
 
  gnuplot> import divisors(x) from "demo_plugin"
  gnuplot> import mysinc(x) from "demo_plugin:sinc"
  gnuplot> import nsinc(N,x) from "demo_plugin"
 */

#include "gnuplot_plugin.h"
#include "math.h"

/* This funcstion returns the number of divisors of the first argument */

DLLEXPORT struct value divisors(int nargs, struct value *arg, void *p)
{
  int a = IVAL(arg[0]);
  int i = 1;
  int j = a;
  struct value r;
  r.type = INTGR;
  r.v.int_val = 0;

  /* Enforce a match between the number of parameters declared
   * by the gnuplot import command and the number implemented here.
   */
  RETURN_ERROR_IF_WRONG_NARGS(r, nargs, 1);

  /* Sanity check on argument type */
  RETURN_ERROR_IF_NONNUMERIC(r, arg[0]);

  while (i <= j)
    {
      if (a == i*j)
	{
	  r.v.int_val += 1 + (i!=j);
	}
      i++;
      j=a/i;
    }

  return r;
}

DLLEXPORT struct value sinc(int nargs, struct value *arg, void *p)
{
  double x = RVAL(arg[0]);
  struct value r;
  r.type = CMPLX;

  /* Enforce a match between the number of parameters declared
   * by the gnuplot import command and the number implemented here.
   */
  RETURN_ERROR_IF_WRONG_NARGS(r, nargs, 1);

  /* Sanity check on argument type */
  RETURN_ERROR_IF_NONNUMERIC(r, arg[0]);

  r.v.cmplx_val.real = sin(x)/x;
  r.v.cmplx_val.imag = 0.0;
  
  return r;
}

DLLEXPORT struct value nsinc(int nargs, struct value *arg, void *p)
{
  struct value r;
  int n;	/* 1st parameter */
  double x;	/* 2nd parameter */

  /* Enforce a match between the number of parameters declared
   * by the gnuplot import command and the number implemented here.
   */
  RETURN_ERROR_IF_WRONG_NARGS(r, nargs, 2);

  /* Sanity check on argument type */
  RETURN_ERROR_IF_NONNUMERIC(r, arg[0]);
  RETURN_ERROR_IF_NONNUMERIC(r, arg[1]);

  n = IVAL(arg[0]);
  x = RVAL(arg[1]);

  r.type = CMPLX;
  r.v.cmplx_val.real = n * sin(x)/x;
  r.v.cmplx_val.imag = 0.0;
  
  return r;
}
