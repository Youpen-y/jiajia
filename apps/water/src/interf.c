#include <jia.h>

#include "math.h"
#include "mdvar.h"
#include "water.h"
#include "wwpot.h"
#include "cnst.h"
#include "parameters.h"
#include "mddata.h"
#include "split.h"
#include "global.h"

#include <stdio.h>
#include "fileio.h"
extern FILE *dump;


#define	LOCAL_COMP
#ifdef	LOCAL_COMP
typedef struct tmp_dummy {
    double Desti[NDIR][NATOM];
} dmol_type;
dmol_type localVAR[MAXMOLS];
#endif	LOCAL_COMP



INTERF(DEST,VIR)
int DEST;
double *VIR;
{
     /* This routine gets called both from main() and from mdmain().
        When called from main(), it is used to estimate the initial
        accelerations by computing intermolecular forces.  When called
        from mdmain(), it is used to compute intermolecular forces.
        The parameter DEST specifies whether results go into the 
        accelerations or the forces. Uses routine UPDATE_FORCES in this
        file, and routine CSHIFT in file cshift.U */
/*
.....this routine calculates inter-molecular interaction forces
     the distances are arranged in the order  M-M, M-H1, M-H3, H1-M,
     H3-M, H1-H3, H1-H1, H3-H1, H3-H3, O-O, O-H1, O-H3, H1-O, H3-O, 
     where the M are "centers" of the molecules.
*/

    int mol, comp, dir, icomp;
    int comp_last, half_mol;
    int    KC, K;
    double YL[14], XL[14], ZL[14], RS[14], FF[14], RL[14]; /* per-
            interaction arrays that hold some computed distances */
    double  FTEMP;
    double LVIR = 0.0;

#ifdef	LOCAL_COMP
    bzero(localVAR, sizeof(dmol_type) * NMOL);
#endif	LOCAL_COMP

    half_mol = NMOL/2;
    for (mol = StartMol[jiapid]; mol < StartMol[jiapid+1]; mol++) {
        comp_last = mol + half_mol;
	if (NMOL%2==0 && ((!(mol%2) && (mol < half_mol)) || ((mol%2) && mol > half_mol))) comp_last -= 1;
        for (icomp = mol+1; icomp <= comp_last; icomp++) {
	    comp = icomp;
	    if (comp > NMOL1) comp = comp%NMOL;

            /*  compute some intermolecular distances */

      	    CSHIFT(VAR[mol].F[DISP][XDIR],VAR[comp].F[DISP][XDIR],
	           VAR[mol].VM[XDIR],VAR[comp].VM[XDIR],XL,BOXH,BOXL);

            CSHIFT(VAR[mol].F[DISP][YDIR],VAR[comp].F[DISP][YDIR],
	           VAR[mol].VM[YDIR],VAR[comp].VM[YDIR],YL,BOXH,BOXL);

            CSHIFT(VAR[mol].F[DISP][ZDIR],VAR[comp].F[DISP][ZDIR],
	           VAR[mol].VM[ZDIR],VAR[comp].VM[ZDIR],ZL,BOXH,BOXL);


            KC=0;
            for (K = 0; K < 9; K++) {
           	RS[K]=XL[K]*XL[K]+YL[K]*YL[K]+ZL[K]*ZL[K];
		if (RS[K] > CUT2) 
	  	    KC++;
 	    } /* for K */
      	    if (KC != 9) {
      		for (K = 0; K < 14; K++) 
       		    FF[K]=0.0;
      		if (RS[0] < CUT2) {
        	    FF[0]=QQ4/(RS[0]*sqrt(RS[0]))+REF4;
	    	    LVIR = LVIR + FF[0]*RS[0];
                } /* if */
      		for (K = 1; K < 5; K++) {
        	    if (RS[K] < CUT2) { 
			FF[K]= -QQ2/(RS[K]*sqrt(RS[K]))-REF2;
	  		LVIR = LVIR + FF[K]*RS[K];
		    } /* if */
		    if (RS[K+4] <= CUT2) { 
			RL[K+4]=sqrt(RS[K+4]);
	  		FF[K+4]=QQ/(RS[K+4]*RL[K+4])+REF1;
	  		LVIR = LVIR + FF[K+4]*RS[K+4];
		    } /* if */
      		} /* for K */
      		if (KC == 0) {
        	    RS[9]=XL[9]*XL[9]+YL[9]*YL[9]+ZL[9]*ZL[9];
	   	    RL[9]=sqrt(RS[9]);
	  	    FF[9]=AB1*exp(-B1*RL[9])/RL[9];
		    LVIR = LVIR + FF[9]*RS[9];
		    for (K = 10; K < 14; K++) { 
			FTEMP=AB2*exp(-B2*RL[K-5])/RL[K-5];
	  		FF[K-5]=FF[K-5]+FTEMP;
	  		LVIR= LVIR+FTEMP*RS[K-5];
	  		RS[K]=XL[K]*XL[K]+YL[K]*YL[K]+ZL[K]*ZL[K];
	  		RL[K]=sqrt(RS[K]);
	  		FF[K]=(AB3*exp(-B3*RL[K])-AB4*exp(-B4*RL[K]))/RL[K];
	  		LVIR = LVIR + FF[K]*RS[K];
		    } /* for K */
      		} /* if KC == 0 */

		UPDATE_FORCES(DEST, mol, comp, XL, YL, ZL, FF);

    	    }  /* if KC != 9 */  
        } /* for comp */
    } /* for mol */

#ifdef	LOCAL_COMP


    {
	int		i, pid, k;
	
	for (k = 1; k < jiahosts; k++) {
	  pid = (jiapid + k) % jiahosts;
	  jia_lock(gl->MolLock[pid]);
	  for (mol=StartMol[pid]; mol<StartMol[pid+1]; mol++) {
	    
	    if (localVAR[mol].Desti[XDIR][O] ||
		localVAR[mol].Desti[XDIR][H1] ||
		localVAR[mol].Desti[XDIR][H2] ||
		localVAR[mol].Desti[YDIR][O] ||
		localVAR[mol].Desti[YDIR][H1] ||
		localVAR[mol].Desti[YDIR][H2] ||
		localVAR[mol].Desti[ZDIR][O] ||
		localVAR[mol].Desti[ZDIR][H1] ||
		localVAR[mol].Desti[ZDIR][H2])
	    {
		
		VAR[mol].F[DEST][XDIR][O] += localVAR[mol].Desti[XDIR][O];
		VAR[mol].F[DEST][XDIR][H1] += localVAR[mol].Desti[XDIR][H1];
		VAR[mol].F[DEST][XDIR][H2] +=localVAR[mol].Desti[XDIR][H2];
		VAR[mol].F[DEST][YDIR][O]  += localVAR[mol].Desti[YDIR][O];
		VAR[mol].F[DEST][YDIR][H1] += localVAR[mol].Desti[YDIR][H1];
		VAR[mol].F[DEST][YDIR][H2] += localVAR[mol].Desti[YDIR][H2];
		VAR[mol].F[DEST][ZDIR][O]  +=localVAR[mol].Desti[ZDIR][O];
		VAR[mol].F[DEST][ZDIR][H1] +=localVAR[mol].Desti[ZDIR][H1];
		VAR[mol].F[DEST][ZDIR][H2] +=localVAR[mol].Desti[ZDIR][H2];
	      }
	  }
	jia_unlock(gl->MolLock[pid]);  
	}
      }
#endif	LOCAL_COMP
            /*  accumulate the running sum from private 
                 per-interaction partial sums   */
    jia_lock(gl->InterfVirLock);
    *VIR = *VIR + LVIR;
    jia_unlock(gl->InterfVirLock);

            /* wait till all forces are updated */
    if (jiahosts > 1)
      jia_barrier();

    /* Add to myself */
    for (mol=StartMol[jiapid]; mol<StartMol[jiapid+1]; mol++) {
      
      VAR[mol].F[DEST][XDIR][O] += localVAR[mol].Desti[XDIR][O];
      VAR[mol].F[DEST][XDIR][H1] += localVAR[mol].Desti[XDIR][H1];
      VAR[mol].F[DEST][XDIR][H2] +=localVAR[mol].Desti[XDIR][H2];
      VAR[mol].F[DEST][YDIR][O]  += localVAR[mol].Desti[YDIR][O];
      VAR[mol].F[DEST][YDIR][H1] += localVAR[mol].Desti[YDIR][H1];
      VAR[mol].F[DEST][YDIR][H2] += localVAR[mol].Desti[YDIR][H2];
      VAR[mol].F[DEST][ZDIR][O]  +=localVAR[mol].Desti[ZDIR][O];
      VAR[mol].F[DEST][ZDIR][H1] +=localVAR[mol].Desti[ZDIR][H1];
      VAR[mol].F[DEST][ZDIR][H2] +=localVAR[mol].Desti[ZDIR][H2];
    }


    /* divide final forces by masses */

    for (mol = StartMol[jiapid]; mol < StartMol[jiapid+1]; mol++) {
	for ( dir = XDIR; dir  <= ZDIR; dir++) {
	    VAR[mol].F[DEST][dir][H1] = VAR[mol].F[DEST][dir][H1] * FHM;
	    VAR[mol].F[DEST][dir][H2] = VAR[mol].F[DEST][dir][H2] * FHM;
	    VAR[mol].F[DEST][dir][O]  = VAR[mol].F[DEST][dir][O] * FOM;
	} /* for dir */
    } /* for mol */
}/* end of subroutine INTERF */

UPDATE_FORCES(DEST, mol, comp, XL, YL, ZL, FF)
        /* from the computed distances etc., compute the 
            intermolecular forces and update the force (or 
            acceleration) locations */

double XL[], YL[], ZL[], FF[];
{
    int K;
    double G110[3], G23[3], G45[3], TT1[3], TT[3], TT2[3];
    double GG[15][3];

	/*   CALCULATE X-COMPONENT FORCES */
	for (K = 0; K < 14; K++)  {
	    GG[K+1][XDIR] = FF[K]*XL[K];
	    GG[K+1][YDIR] = FF[K]*YL[K];
	    GG[K+1][ZDIR] = FF[K]*ZL[K];
	}

	G110[XDIR] = GG[10][XDIR]+GG[1][XDIR]*C1;
	G110[YDIR] = GG[10][YDIR]+GG[1][YDIR]*C1;
	G110[ZDIR] = GG[10][ZDIR]+GG[1][ZDIR]*C1;
	G23[XDIR] = GG[2][XDIR]+GG[3][XDIR];
	G23[YDIR] = GG[2][YDIR]+GG[3][YDIR];
	G23[ZDIR] = GG[2][ZDIR]+GG[3][ZDIR];
	G45[XDIR]=GG[4][XDIR]+GG[5][XDIR];
	G45[YDIR]=GG[4][YDIR]+GG[5][YDIR];
	G45[ZDIR]=GG[4][ZDIR]+GG[5][ZDIR];
	TT1[XDIR] =GG[1][XDIR]*C2;
	TT1[YDIR] =GG[1][YDIR]*C2;
	TT1[ZDIR] =GG[1][ZDIR]*C2;
	TT[XDIR] =G23[XDIR]*C2+TT1[XDIR];
	TT[YDIR] =G23[YDIR]*C2+TT1[YDIR];
	TT[ZDIR] =G23[ZDIR]*C2+TT1[ZDIR];
	TT2[XDIR]=G45[XDIR]*C2+TT1[XDIR];
	TT2[YDIR]=G45[YDIR]*C2+TT1[YDIR];
	TT2[ZDIR]=G45[ZDIR]*C2+TT1[ZDIR];
            /* lock locations for the molecule to be updated */
#ifdef	LOCAL_COMP
    localVAR[mol].Desti[XDIR][O] +=
    	G110[XDIR] + GG[11][XDIR] +GG[12][XDIR]+C1*G23[XDIR];
    localVAR[mol].Desti[XDIR][H1] += 
    	GG[6][XDIR]+GG[7][XDIR]+GG[13][XDIR]+TT[XDIR]+GG[4][XDIR];
    localVAR[mol].Desti[XDIR][H2] +=
    	GG[8][XDIR]+GG[9][XDIR]+GG[14][XDIR]+TT[XDIR]+GG[5][XDIR];
    localVAR[mol].Desti[YDIR][O]  += 
    	G110[YDIR]+GG[11][YDIR]+GG[12][YDIR]+C1*G23[YDIR];
    localVAR[mol].Desti[YDIR][H1] += 
    	GG[6][YDIR]+GG[7][YDIR]+GG[13][YDIR]+TT[YDIR]+GG[4][YDIR];
    localVAR[mol].Desti[YDIR][H2] += 
    	GG[8][YDIR]+GG[9][YDIR]+GG[14][YDIR]+TT[YDIR]+GG[5][YDIR];
    localVAR[mol].Desti[ZDIR][O]  +=
    	G110[ZDIR]+GG[11][ZDIR]+GG[12][ZDIR]+C1*G23[ZDIR];
    localVAR[mol].Desti[ZDIR][H1] +=
    	GG[6][ZDIR]+GG[7][ZDIR]+GG[13][ZDIR]+TT[ZDIR]+GG[4][ZDIR];
    localVAR[mol].Desti[ZDIR][H2] +=
    	GG[8][ZDIR]+GG[9][ZDIR]+GG[14][ZDIR]+TT[ZDIR]+GG[5][ZDIR];
    
    localVAR[comp].Desti[XDIR][O] +=
    	-G110[XDIR]-GG[13][XDIR]-GG[14][XDIR]-C1*G45[XDIR];
    localVAR[comp].Desti[XDIR][H1] +=
    	-GG[6][XDIR]-GG[8][XDIR]-GG[11][XDIR]-TT2[XDIR]-GG[2][XDIR];
    localVAR[comp].Desti[XDIR][H2] +=
    	-GG[7][XDIR]-GG[9][XDIR]-GG[12][XDIR]-TT2[XDIR]-GG[3][XDIR];
    localVAR[comp].Desti[YDIR][O] += 
    	-G110[YDIR]-GG[13][YDIR]-GG[14][YDIR]-C1*G45[YDIR];
    localVAR[comp].Desti[YDIR][H1] += 
    	-GG[6][YDIR]-GG[8][YDIR]-GG[11][YDIR]-TT2[YDIR]-GG[2][YDIR];
    localVAR[comp].Desti[YDIR][H2] += 
    	-GG[7][YDIR]-GG[9][YDIR]-GG[12][YDIR]-TT2[YDIR]-GG[3][YDIR];
    localVAR[comp].Desti[ZDIR][O] +=
    	-G110[ZDIR]-GG[13][ZDIR]-GG[14][ZDIR]-C1*G45[ZDIR];
    localVAR[comp].Desti[ZDIR][H1] +=
    	-GG[6][ZDIR]-GG[8][ZDIR]-GG[11][ZDIR]-TT2[ZDIR]-GG[2][ZDIR];
    localVAR[comp].Desti[ZDIR][H2] +=
    	-GG[7][ZDIR]-GG[9][ZDIR]-GG[12][ZDIR]-TT2[ZDIR]-GG[3][ZDIR];

#else	LOCAL_COMP

    jia_lock(gl->MolLock[mol]);
    VAR[mol].F[DEST][XDIR][O] +=
    	G110[XDIR] + GG[11][XDIR] +GG[12][XDIR]+C1*G23[XDIR];
    VAR[mol].F[DEST][XDIR][H1] += 
    	GG[6][XDIR]+GG[7][XDIR]+GG[13][XDIR]+TT[XDIR]+GG[4][XDIR];
    VAR[mol].F[DEST][XDIR][H2] +=
    	GG[8][XDIR]+GG[9][XDIR]+GG[14][XDIR]+TT[XDIR]+GG[5][XDIR];
    VAR[mol].F[DEST][YDIR][O]  += 
    	G110[YDIR]+GG[11][YDIR]+GG[12][YDIR]+C1*G23[YDIR];
    VAR[mol].F[DEST][YDIR][H1] += 
    	GG[6][YDIR]+GG[7][YDIR]+GG[13][YDIR]+TT[YDIR]+GG[4][YDIR];
    VAR[mol].F[DEST][YDIR][H2] += 
    	GG[8][YDIR]+GG[9][YDIR]+GG[14][YDIR]+TT[YDIR]+GG[5][YDIR];
    VAR[mol].F[DEST][ZDIR][O]  +=
    	G110[ZDIR]+GG[11][ZDIR]+GG[12][ZDIR]+C1*G23[ZDIR];
    VAR[mol].F[DEST][ZDIR][H1] +=
    	GG[6][ZDIR]+GG[7][ZDIR]+GG[13][ZDIR]+TT[ZDIR]+GG[4][ZDIR];
    VAR[mol].F[DEST][ZDIR][H2] +=
    	GG[8][ZDIR]+GG[9][ZDIR]+GG[14][ZDIR]+TT[ZDIR]+GG[5][ZDIR];
    jia_unlock(gl->MolLock[mol]);

    jia_lock(gl->MolLock[comp]);
    VAR[comp].F[DEST][XDIR][O] +=
    	-G110[XDIR]-GG[13][XDIR]-GG[14][XDIR]-C1*G45[XDIR];
    VAR[comp].F[DEST][XDIR][H1] +=
    	-GG[6][XDIR]-GG[8][XDIR]-GG[11][XDIR]-TT2[XDIR]-GG[2][XDIR];
    VAR[comp].F[DEST][XDIR][H2] +=
    	-GG[7][XDIR]-GG[9][XDIR]-GG[12][XDIR]-TT2[XDIR]-GG[3][XDIR];
    VAR[comp].F[DEST][YDIR][O] += 
    	-G110[YDIR]-GG[13][YDIR]-GG[14][YDIR]-C1*G45[YDIR];
    VAR[comp].F[DEST][YDIR][H1] += 
    	-GG[6][YDIR]-GG[8][YDIR]-GG[11][YDIR]-TT2[YDIR]-GG[2][YDIR];
    VAR[comp].F[DEST][YDIR][H2] += 
    	-GG[7][YDIR]-GG[9][YDIR]-GG[12][YDIR]-TT2[YDIR]-GG[3][YDIR];
    VAR[comp].F[DEST][ZDIR][O] +=
    	-G110[ZDIR]-GG[13][ZDIR]-GG[14][ZDIR]-C1*G45[ZDIR];
    VAR[comp].F[DEST][ZDIR][H1] +=
    	-GG[6][ZDIR]-GG[8][ZDIR]-GG[11][ZDIR]-TT2[ZDIR]-GG[2][ZDIR];
    VAR[comp].F[DEST][ZDIR][H2] +=
    	-GG[7][ZDIR]-GG[9][ZDIR]-GG[12][ZDIR]-TT2[ZDIR]-GG[3][ZDIR];
    jia_unlock(gl->MolLock[comp]);
#endif

}           /* end of subroutine UPDATE_FORCES */
