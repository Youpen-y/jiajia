#include <stdio.h>
#include <math.h>
#include <jia.h>
 
// pointers to shared variables
int *Prime;  // Prime[I] = 1 means prime

int N,  // range to check for primeness
    Debug,  // 1 for debugging, 0 else
    ChunkSize;  // = N/(jiahosts+1); assumes N%(jiahosts+1) is 0, 
                // and ChunkSize >= sqrt(N)

float Lim;

// cross out chunk starting at Start
CrossOut(int Start)

{  int I,SCS,K;

   SCS = Start + ChunkSize - 1;
   for (I = 2; I <= Lim ; I++)  {
      for (K = ceil(Start/I); K <= floor(SCS/I); K++)  {
         if (K > 1)
            Prime[K*I] = 0;
            // note that this 0 won't propagate until barrier
      }
   }
}

main(int ArgC, char **ArgV)

{  int NPrimes,  // number of primes found 
       MyWait = 0,  // kind of a debugger "barrier"
       I;

   jia_init(ArgC,ArgV);  // this must be called first

   // jiapid is the JIAJIA ID number for this node; note that
   // command-line arguments are shifted for nodes other than 0 
   if (jiapid == 0)  {
      N = atoi(ArgV[1]);
      Debug = atoi(ArgV[2]);   
   }
   else  { 
      N = atoi(ArgV[2]);
      Debug = atoi(ArgV[3]);   
   }

   // no need to cross out multiples of K > sqrt(N)
   Lim = sqrt(N);

   ChunkSize = N / (jiahosts+1);

   // set up shared variables
   jia_barrier();
   Prime = (int *) jia_alloc(N*sizeof(int)); 
   jia_barrier();

   // make them all prime until shown otherwise
   if (jiapid == 0)  {
      for (I = 0; I < N; I++) 
            Prime[I] = 1;
   }
   jia_barrier();

   // jia_config(HMIG,ON);  commented out, since it didn't seem to help

   // if debugging, have everyone wait here and then set MyWait to 1 by
   // hand 
   if (Debug)
      while (MyWait == 0)  { ; }
  
   // wait for node 0 to find all the primes up to Lim (actually up
   // to ChunkSize for convenience, though Lim is all we would need)
   if (jiapid == 0)  {
      CrossOut(0);
   }
   jia_barrier();

   // now, do my chunk
   CrossOut((jiapid+1)*ChunkSize+1);

   jia_barrier();
   if (jiapid == 0)  {
      NPrimes = 0;
      for (I = 2; I <= N; I++)
         if (Prime[I]) NPrimes++;
      printf("the number of primes found was %d\n",NPrimes);
   }

   jia_exit();
}