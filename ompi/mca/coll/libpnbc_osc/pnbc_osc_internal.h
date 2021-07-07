/* -*- Mode: C; c-basic-offset:2 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2006 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 * Copyright (c) 2006 The Technical University of Chemnitz. All
 *                    rights reserved.
 *
 * Author(s): Torsten Hoefler <htor@cs.indiana.edu>
 *
 * Copyright (c) 2012      Oracle and/or its affiliates.  All rights reserved.
 * Copyright (c) 2014      NVIDIA Corporation.  All rights reserved.
 * Copyright (c) 2015-2018 Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2015      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2018      FUJITSU LIMITED.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 */
#ifndef __PNBC_OSC_INTERNAL_H__
#define __PNBC_OSC_INTERNAL_H__
#include "ompi_config.h"

/* correct fortran bindings */
#define PNBC_OSC_F77_FUNC_ F77_FUNC_

//#include "mpi.h"

#if OPAL_CUDA_SUPPORT
#include "opal/datatype/opal_convertor.h"
#include "opal/datatype/opal_datatype_cuda.h"
#endif /* OPAL_CUDA_SUPPORT */
#include "ompi/include/ompi/constants.h"
#include "ompi/request/request.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/communicator/communicator.h"
#include "ompi/win/win.h"

#include "pnbc_osc_debug.h"
#include "coll_libpnbc_osc.h"
#include "pnbc_osc_request.h"

//#include <stdlib.h>
//#include <assert.h>
//#include <math.h>
//#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* log(2) */
#define LOG2 0.69314718055994530941

/* true/false */
#define true 1
#define false 0

/* Function return codes  */
#define PNBC_OSC_OK 0 /* everything went fine */
#define PNBC_OSC_SUCCESS 0 /* everything went fine (MPI compliant :) */
#define PNBC_OSC_OOR 1 /* out of resources */
#define PNBC_OSC_BAD_SCHED 2 /* bad schedule */
#define PNBC_OSC_CONTINUE 3 /* progress not done */
#define PNBC_OSC_DATATYPE_NOT_SUPPORTED 4 /* datatype not supported or not valid */
#define PNBC_OSC_OP_NOT_SUPPORTED 5 /* operation not supported or not valid */
#define PNBC_OSC_NOT_IMPLEMENTED 6
#define PNBC_OSC_INVALID_PARAM 7 /* invalid parameters */
#define PNBC_OSC_INVALID_TOPOLOGY_COMM 8 /* invalid topology attached to communicator */

typedef ompi_coll_libpnbc_osc_request_t PNBC_OSC_Handle;

typedef enum {
  LOCKED,
  UNLOCKED
} PNBC_OSC_Lock_status;

typedef enum {
  PUT,
  GET,
  TRY_GET,
  WIN_FREE,
  OP,
  COPY,
  UNPACK
} PNBC_OSC_Fn_type;

/* the put argument struct */
typedef struct {
  PNBC_OSC_Fn_type type;
  int origin_count;
  int target_count;
  const void *buf;
  MPI_Datatype origin_datatype;
  MPI_Datatype target_datatype;
  int target;
  MPI_Aint target_displ;
  bool local;
} PNBC_OSC_Args_put;

/* the get argument struct */
typedef struct {
  PNBC_OSC_Fn_type type;
  int origin_count;
  int target_count;
  void *buf;
  MPI_Datatype origin_datatype;
  MPI_Datatype target_datatype;
  MPI_Aint target_displ;
  int target;
  char tmpbuf;
  bool local;
  int lock_type;
  PNBC_OSC_Lock_status lock_status;
  int assert;
  bool notify;
} PNBC_OSC_Args_get;

/* the win_free argument struct */
typedef struct {
  PNBC_OSC_Fn_type type;
} PNBC_OSC_Args_win_free;

/* the operation argument struct */
typedef struct {
  PNBC_OSC_Fn_type type;
  char tmpbuf1;
  char tmpbuf2;
  const void *buf1;
  void *buf2;
  MPI_Op op;
  MPI_Datatype datatype;
  int count;
} PNBC_OSC_Args_op;

/* the copy argument struct */
typedef struct {
  PNBC_OSC_Fn_type type;
  int srccount;
  void *src;
  void *tgt;
  MPI_Datatype srctype;
  MPI_Datatype tgttype;
  int tgtcount;
  char tmpsrc;
  char tmptgt;
} PNBC_OSC_Args_copy;

/* unpack operation arguments */
typedef struct {
  PNBC_OSC_Fn_type type;
  int count;
  void *inbuf;
  void *outbuf;
  MPI_Datatype datatype;
  char tmpinbuf;
  char tmpoutbuf;
} PNBC_OSC_Args_unpack;

/* internal function prototypes */

/* add a put to a schedule */
int PNBC_OSC_Sched_rput(const void* buf, int target,
                        int origin_count, MPI_Datatype origin_datatype,
                        int target_count, MPI_Datatype target_datatype,
                        MPI_Aint target_displ,
                        PNBC_OSC_Schedule *schedule, bool barrier);

/* add a get to a schedule */
int PNBC_OSC_Sched_rget(      void* buf, int target,
                        int origin_count, MPI_Datatype origin_datatype,
                        int target_count, MPI_Datatype target_datatype,
                        MPI_Aint target_displ,
                        PNBC_OSC_Schedule *schedule, bool barrier);

int PNBC_OSC_Sched_tryget(      void* buf, int target,
                          int origin_count, MPI_Datatype origin_datatype,
                          int target_count, MPI_Datatype target_datatype,
                          MPI_Aint target_disp,
                          int lock_type, int assert, bool notify,
                          PNBC_OSC_Schedule *schedule, bool barrier);

/* schedule win_free */
int PNBC_OSC_Sched_win_free(PNBC_OSC_Schedule *schedule, bool barrier);

int PNBC_OSC_Sched_op (const void* buf1, char tmpbuf1, void* buf2, char tmpbuf2, int count,
                       MPI_Datatype datatype, MPI_Op op, PNBC_OSC_Schedule *schedule,
                       bool barrier);

int PNBC_OSC_Sched_copy (void *src, char tmpsrc, int srccount, MPI_Datatype srctype,
                         void *tgt, char tmptgt, int tgtcount,
                         MPI_Datatype tgttype, PNBC_OSC_Schedule *schedule, bool barrier);

int PNBC_OSC_Sched_unpack (void *inbuf, char tmpinbuf, int count, MPI_Datatype datatype,
                           void *outbuf, char tmpoutbuf,
                           PNBC_OSC_Schedule *schedule, bool barrier);
  
int PNBC_OSC_Sched_barrier (PNBC_OSC_Schedule *schedule);
int PNBC_OSC_Sched_commit (PNBC_OSC_Schedule *schedule);


int PNBC_OSC_Schedule_request_win(PNBC_OSC_Schedule *schedule, ompi_communicator_t *comm,
                                  ompi_win_t *win,
                                  ompi_coll_libpnbc_osc_module_t *module,
                                  bool persistent, ompi_request_t **request);
  
int PNBC_OSC_Start(PNBC_OSC_Handle *handle);
int PNBC_OSC_Progress(PNBC_OSC_Handle *handle);
void PNBC_OSC_Free (PNBC_OSC_Handle* handle);

static inline void PNBC_OSC_Reset(PNBC_OSC_Handle *handle) {
  //handle->schedule->row_offset = 0;
}

static inline int PNBC_OSC_Type_intrinsic(MPI_Datatype type);
int PNBC_OSC_Create_fortran_handle(int *fhandle, PNBC_OSC_Handle **handle);
  
  /* some macros */

#define PNBC_OSC_IN_PLACE(sendbuf, recvbuf, inplace) \
    {                                           \
      inplace = 0;                              \
      if(recvbuf == sendbuf) {                  \
        inplace = 1;                            \
      } else                                    \
        if(sendbuf == MPI_IN_PLACE) {           \
          sendbuf = recvbuf;                    \
          inplace = 1;                          \
        } else                                  \
          if(recvbuf == MPI_IN_PLACE) {         \
            recvbuf = (void *)sendbuf;          \
            inplace = 1;                        \
          }                                     \
    }

    int PNBC_OSC_Comm_neighbors_count (ompi_communicator_t *comm, int *indegree,
                                       int *outdegree);
    int PNBC_OSC_Comm_neighbors (ompi_communicator_t *comm, int **sources,
                                 int *source_count, int **destinations,
                                 int *dest_count);

#ifdef __cplusplus
  }
#endif

#endif
