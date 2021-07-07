#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include "mpi-ext.h"

int main(int argc, char *argv[]) {
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  printf("provided thread level is: %d - ", provided);
  switch (provided) {
    case MPI_THREAD_SINGLE     : printf("mpi_thread_single\n");     break;
    case MPI_THREAD_FUNNELED   : printf("mpi_thread_funneled\n");   break;
    case MPI_THREAD_SERIALIZED : printf("mpi_thread_serialized\n"); break;
    case MPI_THREAD_MULTIPLE   : printf("mpi_thread_multiple\n");   break;
    default                    : printf("unknown\n");               break;
  }

  int csize;
  MPI_Comm_size(MPI_COMM_WORLD, &csize);

  MPI_Datatype sendtype = MPI_INT, recvtype = MPI_INT;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Info info = MPI_INFO_NULL;
  MPI_Request request;

  int sendcounts[csize], sdispls[csize], recvcounts[csize], rdispls[csize];
  int *sendbuf = malloc(10*csize*sizeof(int));
  int *recvbuf = malloc(10*csize*sizeof(int));
  for (int i=0;i<csize;++i) {
    sendcounts[i] = recvcounts[i] = 10;
    sdispls[i] = rdispls[i] = 10*i;
    for (int j=0;j<10;++j) {
      sendbuf[10*i+j] = j + 1000*i + 1000000;
      recvbuf[10*i+j] = 0;
    }
  }

  printf("index sendbuf recvbuf\n");
  for (int i=0;i<10*csize;++i) {
    printf("%5d %7d %7d\n", i, sendbuf[i], recvbuf[i]);
  }

  printf("About to call MPIX_Alltoallv_init\n");
  int ret = MPIX_Alltoallv_init(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts, rdispls, recvtype, comm, info, &request);
  printf("Done call to MPIX_Alltoallv_init - ret value is %d\n", ret);

  printf("request is %p\n", request);

  printf("About to call MPI_Start\n");
  MPI_Start(&request);
  printf("Done call to MPI_Start\n");

  printf("request is %p\n", request);

  printf("About to call MPI_Wait\n");
  MPI_Wait(&request, MPI_STATUS_IGNORE);
  printf("Done call to MPI_Wait\n");

  printf("index sendbuf recvbuf\n");
  for (int i=0;i<10*csize;++i) {
    printf("%5d %7d %7d\n", i, sendbuf[i], recvbuf[i]);
  }

  printf("request is %p\n", request);

  printf("About to call MPI_Request_free\n");
  MPI_Request_free(&request);
  printf("Done call to MPI_Request_free\n");

  printf("request is %p\n", request);

  MPI_Finalize();
}
