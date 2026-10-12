#ifndef THREADS_FIXED_H
#define THREADS_FIXED_H

#include <stdint.h>

typedef int32_t fixed_t;

/* 14 bits used for fractional part in 17.14 FIXED_T representation. */
#define FRACTION (1 << 14)

/* Used for rounding approximation when converting from FIXED_T point to INT_32 representation. */
#define HALF_FRACTION (1 << 13)

/* Converts an INT_32 number to FIXED_T representation. */
#define INT2FIXED(n) ((n) * (FRACTION))

/* Converts an FIXED_T representation to INT_32 representation.
   Rounds to the nearest integer. See Appendix B section B.6
   4.4 BSD Scheduler in the Pintos documentation. */
#define FIXED2INT_NEAREST(x) ( ((x) < 0) ? ((x)-(HALF_FRACTION))/(FRACTION) : ((x)+(HALF_FRACTION))/(FRACTION) )

/* Converts FIXED_T number X to its INT_32 representation.
   Rounds to zero. */
#define FIXED2INT_ZERO(x) ((x) / (FRACTION))

/* ADDITION, MULTIPLICATION, SUBTRACTION and DIVISION of two 17.14 numbers. */
#define FIXED_ADD(x,y) ((x) + (y))
#define FIXED_SUB(x,y) ((x) - (y))
// Sometimes the multiplication can overflow but should still be a valid FIXED_T number.
// Hence the cast to INT_64 for both MULTIPLICATION and DIVISION.
#define FIXED_MUL(x,y) (((int64_t) (x)) * (y) / (FRACTION))
#define FIXED_DIV(x,y) (((int64_t) (x)) * (FRACTION) / (y))

/* ADDITION, MULTIPLICATION, SUBTRACTION and DIVISION of FIXED_T number X AND INT_32 number N.
   Result is of type FIXED_T. */
#define FIXED_INT_ADD(x,n) ((x) + (n) * (FRACTION))
#define FIXED_INT_SUB(x,n) ((x) - (n) * (FRACTION))
#define FIXED_INT_MUL(x,n) ((x) * (n))
#define FIXED_INT_DIV(x,n) ((x) / (n))

#endif  /* threads/fixed.h */
