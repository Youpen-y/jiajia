MPICC := mpicc

TARGET := openmpi_test

all: $(TARGET)

openmpi_test: mpi_transfer_test.c
	$(MPICC) -o $@ $^


run:
	# mpirun -np 2 ./openmpi_test 
	mpirun --display-map -np 2 --mca plm_base_verbose 30 --mca oob_base_verbose 10 -hostfile mpi_hosts ./openmpi_test

clean:
	rm -f $(TARGET)
