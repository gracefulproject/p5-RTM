#include "profiler.h"

/*----------------------------------------------------------------------*/
/*------------------------- Temperature Reading ------------------------*/

/***********************************************************************
 * in readTemp(), read the sensors and put the result in val[]
 * In addition, the value of sensor-2 will be used as return value.
 ***********************************************************************/
uint readTemp()
{
    uint i, done, S[] = {SC_TS0, SC_TS1, SC_TS2};

    for(i=0; i<3; i++) {
        done = 0;
        // set S-flag to 1 and wait until F-flag change to 1
        sc[S[i]] = 0x80000000;
        do {
            done = sc[S[i]] & 0x01000000;
        } while(!done);
        // turnoff S-flag and read the value
        sc[S[i]] = sc[S[i]] & 0x0FFFFFFF;
        tempVal[i] = sc[S[i]] & 0x00FFFFFF;
    }
    return tempVal[2];
}

/* getRealTemp() can be compute-intensive process. Thus, it should be called
 * only when necessary.
 * Note: use tVal 0 to use sensor-2 value from tempVal[]
 * */
REAL getRealTemp(uint tVal)
{
    // based on my data in matlab, there are 3 quadratic equations:
    // y = 4.5e-07*x^2 - 0.0084*x + 77 = 0.45*X^2 - 8.4*X + 77; where X = x/1000
    // y = 3.1e-07*x^2 - 0.0069*x + 76
    // y = 3.5e-07*x^2 - 0.0074*x + 77
    REAL x;
    if(tVal==0)
        x = (REAL)tempVal[2] / 1000.0;
    else
        x = (REAL)tVal / 1000.0;
    REAL y1 = 0.45*x*x - 8.4*x + 77;
    REAL y2 = 0.31*x*x - 6.9*x + 76;
    REAL y3 = 0.35*x*x - 7.4*x + 77;
    REAL result = (y1+y2+y3)/3.0;
    return result;
}
