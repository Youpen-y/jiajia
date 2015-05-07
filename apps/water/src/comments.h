/*
C.....DRIVER FOR MOLECULAR DYNAMIC SIMULATION OF FLEXIBLE WATER MOLECULE
C     WRITTEN BY GEORGE C. LIE, IBM, KINGSTON, N.Y.
C     APRIL 14, 1987 VERSION
C
C ******** INPUT TO THE PROGRAM ********
C
C   &MDINP
C
C     TEMP  : TEMPERATURE IN DEGREE K (DEFAULT=298.0).
C     RHO   : DENSITY IN G/C.C. (DEFAULT=0.998).
C     NORDER: ORDER USED TO SOLVE NEWTONIAN EQUATIONS (DEFAULT=5).
C     TSTEP : TIME STEP IN SECONDS (DEFAULT=1.0E-15).
C     NSTEP : NO. OF TIME STEPS FOR THIS RUN.
C     NPRINT: FREQUENCY OF PRINTING INTERMEDIATE DATA, SUCH AS KINETIC
C             ENERGY, POTENTIAL ENERGY, AVERAGE TEMPERATURE, ETC.
C             (ONE LINE PER PRINTING, DEFAULT=100).
C     LKT   : 1 IF RENORMALIZATION OF KINETIC ENERGY IS TO BE DONE JUST
C               BEFORE SAVING DATA FOR RESTART (NEEDED ONLY AT THE
C               BEGINNING WHERE THE ENERGY OR TEMPERATURE IS TOO HIGH);
C             0 OTHERWISE (DEFAULT=0).
C     NSAVE :-1 FOR THE VERY FIRST RUN;
C             0 DURING EQUILIBRATION STAGE;
C             N FREQUENCY OF DATA (X AND V) SAVING DURING DATA
C               COLLECTING STAGE (DEFAULT=10).
C     NRST  : FREQUENCY OF SAVING INTERMEDIATE DATA FOR RESTART.
C     CUTOFF: CUTOFF RADIUS FOR NEGLECTING FORCE AND POTENTIAL.
C             SET TO 0.0 IF HALF THE SIZE OF THE BOX IS TO BE USED
C             (DEFAULT=0.0D0)
C     NFMC  : 0 IF INITIALIZATION IS TO BE STARTED FROM REGULAR LATTICE;
C            11 IF INITIALIZATION IS TO BE STARTED FROM A RESTART FILE;
C             N IF INITIALIZATION IS TO BE STARTED FROM FT-N
C               WHICH CONTAINS THE COORDINATES OF THE WATER MOLECULES
C               IN THE FORMAT OF 5E16.8.  THE ORDERS ARE X OF H, O, H,
C               OF THE 1-ST WATER, X OF H, O, H OF THE 2-ND WATER, ....
C               FOLLOWED (STARTING FROM A NEW LINE) BY Y'S, THEN Z'S
C               (DEFAULT=12);
C            <0 TO RESET THE STATISTICAL COUNTERS.
C     NFSV  : FORTRAN FILE NO. FOR SAVED DATA (DEFAULT=10).
C     NFRST : FORTRAN FILE NO. FOR RESTART DATA (DEFAULT=11).
C
C   &END
C
C ******* FORTRAN FILES NEEDED ********
C
C     FT05F001  INPUT FILE CONTAINING NAMELIST &MDINP.
C     FT10F001  OUTPUT SEQUENTIAL FILE FOR SAVING X AND V DATA FOR
C               ANALYSIS; LOGICAL RECORD LENGTH IS 18*NMOL+10 DOUBLE
C               WORDS.
C     FT11F001  INPUT AND OUTPUT SEQUENTIAL RESTART FILE;
C               LOGICAL RECORD LENGTH IS 18*NMOL*3*(NORDER+1)+10
C               DOUBLE WORDS.
C     FT12F001  NEEDED ONLY IF NFMC=12 AND NSAVE=0;
C               CONTAINING COORDINATES OF ALL THE WATER MOLECULES.



      PARAMETER (NOMOL=343,MAXODR=7,NATOM=3,MXOD2=MAXODR+2,
     *           NDVAR=NATOM*NOMOL*3*MXOD2)
*/
