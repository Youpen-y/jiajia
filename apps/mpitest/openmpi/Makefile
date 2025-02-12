MPICC := mpicc

TARGET := openmpi_test

all: $(TARGET)

openmpi_test: mpi_transfer_test.c
	$(MPICC) -o $@ $^


run:
	# mpirun -np 2 ./openmpi_test 
	mpirun --hostfile mpi_hosts --mca pml ob1 ./openmpi_test 

clean:
	rm -f $(TARGET)
