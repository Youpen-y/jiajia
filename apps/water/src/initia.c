#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mdvar.h"
#include "water.h"
#include "cnst.h"
#include "fileio.h"
#include "parameters.h"
#include "mddata.h"
#include "split.h"
void INITIA(FILE *nfmc)   /* nfmc points to LWI12, the input file with the initial displacements  */
{
/*   THIS ROUTINE INITIALIZES POSITIONS IN A CUBE AND RANDOMIZES 
     VELOCITY OF EACH ATOM */

    static double XMIN = 0;
    static double YMIN = 0;
    static double ZMIN = 0;
    FILE *random_numbers;       /* points to input file containing
                                pseudo-random numbers for initializing
                                velocities */
    double XMAS[4], XS, ZERO, WCOS, WSIN, XT[4], YT[4], Z;
    double SUX, SUY, SUZ, SUMX, SUMY, SUMZ, FAC;
    int mol=0;
    int atom=0;
    
    if (!(random_numbers = fopen("waterfiles/random.in","r"))) {
	    fprintf(stderr,"Can't open 'random.in'");
    }
    XMAS[1]=sqrt(OMAS*HMAS);
    XMAS[0]=HMAS;
    XMAS[2]=HMAS;

/* .....ASSIGN POSITIONS */

    if (nfmc != NULL) {  /* i.e. if we are to read displacements from
                            an input file (LWI12) */
        rewind(nfmc);
        fprintf(six, "***** NEW RUN STARTING FROM COORDINATES OF WATERS\n");

        /* read in the displacements for the x-direction */
        for (mol = 0; mol < NMOL; mol++)
	    for ( atom = 0; atom < NATOM; atom++)
	        fscanf(nfmc,"%lf",&VAR[mol].F[DISP][XDIR][atom]);

	/* skip numbers in input file until y-direction numbers start */

        for (mol = NMOL; mol < NOMOL; mol++)
	    for ( atom = 0; atom < NATOM; atom++)
	        fscanf(nfmc,"%*lf");

        /* read in the displacements for the y-direction */
        for (mol = 0; mol < NMOL; mol++)
	    for ( atom = 0; atom < NATOM; atom++)
	        fscanf(nfmc,"%lf",&VAR[mol].F[DISP][YDIR][atom]);

	/* skip numbers in input file until y-direction numbers start */

        for (mol = NMOL; mol < NOMOL; mol++)
	    for ( atom = 0; atom < NATOM; atom++)
	        fscanf(nfmc,"%*lf");

        /* read in the displacements for the z-direction */
        for (mol = 0; mol < NMOL; mol++)
	    for ( atom = 0; atom < NATOM; atom++)
	        fscanf(nfmc,"%lf",&VAR[mol].F[DISP][ZDIR][atom]);

        rewind(nfmc);

        /* loop through the values we just read in and determine the
            min. for each direction */
        for (mol = 0; mol < NMOL; mol++) {
     	    for (atom = 0; atom < NATOM; atom++) {
                XMIN=min(XMIN,VAR[mol].F[DISP][XDIR][atom]);
                YMIN=min(YMIN,VAR[mol].F[DISP][YDIR][atom]);
                ZMIN=min(ZMIN,VAR[mol].F[DISP][ZDIR][atom]);
            }
        }
    
        /* subtract the MINs from every atom's displacements; 
           shifts the origin of the system to a corner of the 
           computational box */

        for (mol = 0; mol < NMOL; mol++) {
 	    for (atom = 0; atom < NATOM; atom++) {
                VAR[mol].F[DISP][XDIR][atom] -= XMIN;
                VAR[mol].F[DISP][YDIR][atom] -= YMIN;
                VAR[mol].F[DISP][ZDIR][atom] -= ZMIN;
            }
        }
    } else {  /* do not use input file for displacements; use a
                regular lattice; only the case if the NFMC input in
                the file LWI5 is 0               */ 
	double NS = pow((double) NMOL, 1.0/3.0) - 0.00001;
	double XS = BOXL/NS;
	double ZERO = XS * 0.50;
	double WCOS = ROH * cos(ANGLE * 0.5);
	double WSIN = ROH * sin(ANGLE * 0.5);
	int i,j,k;
        fprintf(six, "***** NEW RUN STARTING FROM REGULAR LATTICE *****\n");
	fflush(six);
	XT[2] = ZERO;
	mol = 0;
        for (i=0; i < NS; i+=1) {
	    XT[1]=XT[2]+WCOS;
	    XT[3]=XT[1];
	    YT[2]=ZERO;
	    for (j=0; j < NS; j+=1) {
		YT[1]=YT[2]+WSIN;
		YT[3]=YT[2]-WSIN;
		Z=ZERO;
		for (k = 0; k < NS; k++) {
		    for (atom = 0; atom < NATOMS; atom +=1) {
	        	VAR[mol].F[DISP][XDIR][atom] = XT[atom+1];
	        	VAR[mol].F[DISP][YDIR][atom] = YT[atom+1];
	        	VAR[mol].F[DISP][ZDIR][atom] = Z;
		    }
		    mol += 1;
      		    Z=Z+XS;
	         }
	         YT[2]=YT[2]+XS;
 	    }
            XT[2]=XT[2]+XS;
       }
       if (NMOL != mol) {
	printf("Lattice init error: total mol %d != NMOL %d\n", mol, NMOL);
	exit(-1);
       }
    }
     
    /* ASSIGN RANDOM MOMENTA */
    fscanf(random_numbers,"%lf",&SUX);

    SUMX=0.0;
    SUMY=0.0;
    SUMZ=0.0;
    /*   read pseudo-random numbers from input file random.in */
    for (mol = 0; mol < NMOL; mol++) {
   	for (atom = 0; atom < NATOMS; atom++) {
            fscanf(random_numbers,"%lf",&VAR[mol].F[FORCES][XDIR][atom]);
            fscanf(random_numbers,"%lf",&VAR[mol].F[FORCES][YDIR][atom]);
            fscanf(random_numbers,"%lf",&VAR[mol].F[FORCES][ZDIR][atom]);
     	    SUMX = SUMX + VAR[mol].F[FORCES][XDIR][atom];
            SUMY = SUMY + VAR[mol].F[FORCES][YDIR][atom];
     	    SUMZ = SUMZ + VAR[mol].F[FORCES][ZDIR][atom];
        } /* atoms */
    } /* molecules */

    /* find average momenta per atom */
    SUMX=SUMX/(NATOMS*NMOL);
    SUMY=SUMY/(NATOMS*NMOL);
    SUMZ=SUMZ/(NATOMS*NMOL);

    /*  find normalization factor so that <k.e.>=KT/2  */
    SUX=0.0;
    SUY=0.0;
    SUZ=0.0;
    for (mol = 0; mol < NMOL; mol++) {
    	SUX = SUX + (pow( (VAR[mol].F[FORCES][XDIR][H1] - SUMX),2.0)
	       +pow( (VAR[mol].F[FORCES][XDIR][H2] - SUMX),2.0))/HMAS
	       +pow( (VAR[mol].F[FORCES][XDIR][O]  - SUMX),2.0)/OMAS;

    	SUY = SUY + (pow( (VAR[mol].F[FORCES][YDIR][H1] - SUMY),2.0)
	       +pow( (VAR[mol].F[FORCES][YDIR][H2] - SUMY),2.0))/HMAS
	       +pow( (VAR[mol].F[FORCES][YDIR][O]  - SUMY),2.0)/OMAS;

    	SUZ = SUZ + (pow( (VAR[mol].F[FORCES][ZDIR][H1] - SUMZ),2.0)
	       +pow( (VAR[mol].F[FORCES][ZDIR][H2] - SUMZ),2.0))/HMAS
	       +pow( (VAR[mol].F[FORCES][ZDIR][O]  - SUMZ),2.0)/OMAS;
    }
    FAC=BOLTZ*TEMP*NATMO/UNITM * pow((UNITT*TSTEP/UNITL),2.0);
    SUX=sqrt(FAC/SUX);
    SUY=sqrt(FAC/SUY);
    SUZ=sqrt(FAC/SUZ);

    /* normalize individual velocities so that there are no bulk
        momenta  */
    XMAS[1]=OMAS;
    for (mol = 0; mol < NMOL; mol++) {
      	for (atom = 0; atom < NATOMS; atom++) {
	    VAR[mol].F[FORCES][XDIR][atom] = ( VAR[mol].F[FORCES][XDIR][atom] -
		SUMX) * SUX/XMAS[atom];
	    VAR[mol].F[FORCES][YDIR][atom] = ( VAR[mol].F[FORCES][YDIR][atom] -
		SUMY) * SUY/XMAS[atom];
	    VAR[mol].F[FORCES][ZDIR][atom] = ( VAR[mol].F[FORCES][ZDIR][atom] -
		SUMZ) * SUZ/XMAS[atom];
        } /* for atom */
    } /* for mol */
    if (nfmc != NULL)
	fclose(nfmc);   /* close input file LWI12 */
} /* end of subroutine INITIA */
