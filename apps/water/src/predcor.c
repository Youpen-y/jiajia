#include <jia.h>

#include "mdvar.h"
#include "parameters.h"
#include "mddata.h"
#include "split.h"

PREDIC(C,NOR1)  /* predicts new values for displacement and its five
                    derivatives */
double C[];
int NOR1;       /* NOR1 = NORDER + 1 = 7 (for a sixth-order method) */
{
/*   this routine calculates predicted F(X), F'(X), F''(X), ... */

    int JIZ;
    int  JI;
    int  L;
    double S;
    int func, mol, dir, atom;

    JIZ=2;
    /* .....loop over F(X), F'(X), F''(X), ..... */
    for (func = 0; func < NORDER; func++) {
	for (mol = StartMol[jiapid]; mol < StartMol[jiapid+1]; mol++)
	    for ( dir = 0; dir < NDIR; dir++)
		for ( atom = 0; atom < NATOM; atom++ ) {
		    JI = JIZ;
		    /* sum over Taylor Series */
		    S = 0.0;
		    for ( L = func; L < NORDER; L++) {
			S += C[JI] * VAR[mol].F[L+1][dir][atom];
			JI++;
		    } /* for */
		    VAR[mol].F[func][dir][atom] += S;
		} /* for atom */
	JIZ += NOR1;
    } /* for func */
} /* end of subroutine PREDIC */

CORREC(PCC,NOR1)    /* corrects the predicted values, based on forces
                        etc. computed in the interim */
double PCC[];       /* the predictor-corrector constants */
int NOR1;           /* NORDER + 1 = 7 for a sixth-order method) */
{
/*
.....this routine calculates corrected F(X), F'(X), F"(X), ....
     from corrected F(X) = predicted F(X) + PCC(1)*(FR-SD)
     where SD is predicted accl. F"(X) and FR is computed 
     accl. (force/mass) at predicted position
*/

    double Y;
    int mol, dir, atom, func;

    for (mol = StartMol[jiapid]; mol < StartMol[jiapid+1]; mol++) {
/*
    for (mol = 0; mol < NMOL; mol++) {
*/
	for (dir = 0; dir < NDIR; dir++) {
	    for (atom = 0; atom < NATOM; atom++) {
		Y = VAR[mol].F[FORCES][dir][atom] - VAR[mol].F[ACC][dir][atom];
		for ( func = 0; func < NOR1; func++) 
		    VAR[mol].F[func][dir][atom] += PCC[func] * Y;   
	    } /* for atom */		
	} /* for dir */
    } /* for mol */
} /* end of subroutine CORREC */

