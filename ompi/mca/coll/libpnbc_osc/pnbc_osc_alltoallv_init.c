/* -*- Mode: C; c-basic-offset:2 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2006      The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2006      The Technical University of Chemnitz. All
 *                         rights reserved.
 * Copyright (c) 2014-2018 Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2015-2017 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2017      IBM Corporation.  All rights reserved.
 * Copyright (c) 2018      FUJITSU LIMITED.  All rights reserved.
 * Copyright (c) 2020      EPCC, The University of Edinburgh. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * Author(s): Daniel Holmes  EPCC, The University of Edinburgh
 *
 */
#include "pnbc_osc_debug.h"
#include "pnbc_osc_internal.h"
#include "pnbc_osc_action_common.h"
#include "pnbc_osc_helper_info.h"

static inline int pnbc_osc_alltoallv_init(const void* sendbuf, const int *sendcounts, const int *sdispls,
                              MPI_Datatype sendtype, void* recvbuf, const int *recvcounts, const int *rdispls,
                              MPI_Datatype recvtype, struct ompi_communicator_t *comm, MPI_Info info,
                              ompi_request_t ** request, struct mca_coll_base_module_2_3_0_t *module, bool persistent);

int ompi_coll_libpnbc_osc_alltoallv_init(const void* sendbuf, const int *sendcounts, const int *sdispls,
                        MPI_Datatype sendtype, void* recvbuf, const int *recvcounts, const int *rdispls,
                        MPI_Datatype recvtype, struct ompi_communicator_t *comm, MPI_Info info,
                        ompi_request_t ** request, struct mca_coll_base_module_2_3_0_t *module) {

    int res = pnbc_osc_alltoallv_init(sendbuf, sendcounts, sdispls, sendtype,
                                      recvbuf, recvcounts, rdispls, recvtype,
                                      comm, info, request, module, true);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
        return res;
    }

    return OMPI_SUCCESS;
}

typedef enum {
  algo_trigger_pull,
  algo_trigger_push,
} a2av_sched_algo;

// pull implies move means get and FLAG means RTS (ready to send)
static inline int a2av_sched_trigger_pull(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                          MPI_Win win, MPI_Comm comm,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_sdispls_other);

// push implies move means put and FLAG means CTS (clear to send)
static inline int a2av_sched_trigger_push(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                          MPI_Win win, MPI_Comm comm,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_rdispls_other);

static int pnbc_osc_alltoallv_init(const void* sendbuf, const int *sendcounts, const int *sdispls,
                  MPI_Datatype sendtype, void* recvbuf, const int *recvcounts, const int *rdispls,
                  MPI_Datatype recvtype, struct ompi_communicator_t *comm, MPI_Info info,
                  ompi_request_t ** request, struct mca_coll_base_module_2_3_0_t *module, bool persistent)
{
  int res;
  char inplace;
  MPI_Aint sendext, recvext;
  PNBC_OSC_Schedule *schedule;
  int crank, csize;
  MPI_Aint base_sendbuf;
  MPI_Aint *abs_sdispls_other, *abs_sdispls_local;
  MPI_Aint base_recvbuf;
  MPI_Aint *abs_rdispls_other, *abs_rdispls_local;
  MPI_Win win;
  ompi_coll_libpnbc_osc_module_t *libpnbc_osc_module = (ompi_coll_libpnbc_osc_module_t*) module;

  PNBC_OSC_IN_PLACE(sendbuf, recvbuf, inplace);
  if (inplace) {
    PNBC_OSC_Error("Error: inplace not implemented for PNBC_OSC AlltoallV\n");
    return OMPI_ERROR;
  }

  res = ompi_datatype_type_extent (recvtype, &recvext);
  if (MPI_SUCCESS != res) {
    PNBC_OSC_Error("MPI Error in ompi_datatype_type_extent() (%i)\n", res);
    return res;
  }

  res = ompi_datatype_type_extent (sendtype, &sendext);
  if (MPI_SUCCESS != res) {
    PNBC_OSC_Error("MPI Error in ompi_datatype_type_extent() (%i)\n", res);
    return res;
  }

  schedule = OBJ_NEW(PNBC_OSC_Schedule);
  if (OPAL_UNLIKELY(NULL == schedule)) {
    return OMPI_ERR_OUT_OF_RESOURCE;
  }

  crank = ompi_comm_rank(comm);
  csize = ompi_comm_size(comm);

  a2av_sched_algo algo = algo_trigger_pull;
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d default config linear_trigger_pull for algo choice\n",
                 crank);

  if ( check_config_value_equal("a2av_algo_requested", info, "linear_trigger_pull") ) {
    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d config value requires linear_trigger_pull for algo choice\n",
                   crank);
    algo = algo_trigger_pull;
  }
  if ( check_config_value_equal("a2av_algo_requested", info, "linear_trigger_push") ) {
    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d config value requires linear_trigger_push for algo choice\n",
                   crank);
    algo = algo_trigger_push;
  }

  // create a dynamic window - data here will be accessed by remote processes
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d creating dynamic window...\n",
                 crank);
  res = ompi_win_create_dynamic(&info->super, comm, &win);
  if (OMPI_SUCCESS != res) {
    PNBC_OSC_Error ("MPI Error in win_create_dynamic (%i)\n", res);
    return res;
  }
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d created dynamic window\n",
                 crank);

  switch (algo) {

    case algo_trigger_pull:
      // uses get to move data from the remote sendbuf - needs sdispls to be exchanged

      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d chosing linear_trigger_pull for algo choice\n",
                     crank);

      // ******************************
      // GET-BASED WINDOW SETUP - BEGIN
      // ******************************

      // compute absolute displacement as MPI_AINT for the sendbuf pointer
      res = MPI_Get_address(sendbuf, &base_sendbuf);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in MPI_Get_address (%i)\n", res);
        MPI_Win_free(&win);
        return res;
      }

      // create an array of displacements where all ranks will gather their window memory base address
      abs_sdispls_other = (MPI_Aint*)malloc(csize * sizeof(MPI_Aint));
      if (OPAL_UNLIKELY(NULL == abs_sdispls_other)) {
        MPI_Win_free(&win);
        return OMPI_ERR_OUT_OF_RESOURCE;
      }
      abs_sdispls_local = (MPI_Aint*)malloc(csize * sizeof(MPI_Aint));
      if (OPAL_UNLIKELY(NULL == abs_sdispls_local)) {
        MPI_Win_free(&win);
        free(abs_sdispls_other);
        return OMPI_ERR_OUT_OF_RESOURCE;
      }

      // compute the total size of all pieces of local sendbuf and record their absolute displacements
      int end_of_sendbuf = 0;
      for (int r=0;r<csize;++r) {
        int end_of_this_bit = sendext * (sdispls[r] + sendcounts[r]);
        if (end_of_this_bit > end_of_sendbuf) end_of_sendbuf = end_of_this_bit;
       	PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d gets new end_of_sendbuf %d (end_of_this_bit %d)\n",
                       crank, end_of_sendbuf, end_of_this_bit);
        MPI_Get_address(((char*)sendbuf) + (sdispls[r]*sendext), &(abs_sdispls_local[r]));
       	PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d (base %lu) gets address at %lu (from sdispl %d giving pointer %p)\n",
                       crank, sendbuf, abs_sdispls_local[r], sdispls[r], ((char*)sendbuf)+(sdispls[r]*sendext));
	PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d local displacement %lu\n",crank,abs_sdispls_local[r]);
      }

      // attach all of the local sendbuf to local window as one large chunk of memory

      res = win->w_osc_module->osc_win_attach(win, (char*)sendbuf, end_of_sendbuf);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in sendbuf win_attach (%i)\n", res);
        free(abs_sdispls_other);
        free(abs_sdispls_local);
        MPI_Win_free(&win);
        return res;
      }
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d attached sendbuf to dynamic window memory for all ranks with address %p of size %d bytes\n",
                     crank,
                     (char*)sendbuf,
                     end_of_sendbuf);

      // swap local sdispls for remote sdispls
      // put the displacements for all local portions on the window
      // get the displacements for all other portions on the window
      res = comm->c_coll->coll_alltoall(abs_sdispls_local, 1, MPI_AINT,
                                        abs_sdispls_other, 1, MPI_AINT,
                                        comm, comm->c_coll->coll_alltoall_module);

      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d My base sendbuf %lu \n", crank, &base_sendbuf);
      for(int i=0;i<csize;i++){
        PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d My send dspl (local,other) are %d, %lu, %lu \n", crank, i, abs_sdispls_local[i], abs_sdispls_other[i]);
      }
      
      if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
        PNBC_OSC_Error ("MPI Error in alltoall for sdispls (%i)\n", res);
        free(abs_sdispls_other);
        free(abs_sdispls_local);
        MPI_Win_free(&win);
        return res;
      }
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d exchanged local and other send displacements using alltoall\n",
                     crank);

      // the local absolute displacement values for portions of sendbuf are only needed remotely
      //free(abs_sdispls_local);

      // ****************************
      // GET_BASED WINDOW SETUP - END
      // ****************************

      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d about to call a2av_sched_trigger_pull\n",
                     crank);
      res = a2av_sched_trigger_pull(crank, csize, schedule, win, comm,
                                    sendbuf, sendcounts, sdispls, sendext, sendtype,
                                    recvbuf, recvcounts, rdispls, recvext, recvtype,
                                    abs_sdispls_other);

      free(abs_sdispls_other);

      if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
        PNBC_OSC_Error ("MPI Error in a2av_sched_linear (%i)\n", res);
        MPI_Win_free(&win);
        OBJ_RELEASE(schedule);
        return res;
      }


      break;

    case algo_trigger_push:
      // uses put to move data into the remote recvbuf - needs rdispls to be exchanged

      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d chosing linear_trigger_push for algo choice\n",
                     crank);

      // ******************************
      // PUT-BASED WINDOW SETUP - BEGIN
      // ******************************
    
      // compute absolute displacement as MPI_AINT for the recvbuf pointer
      res = MPI_Get_address(recvbuf, &base_recvbuf);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in MPI_Get_address (%i)\n", res);
        MPI_Win_free(&win);
        return res;
      }
    
      // create an array of displacements where all ranks will gather their window memory base address
      abs_rdispls_other = (MPI_Aint*)malloc(csize * sizeof(MPI_Aint));
      if (OPAL_UNLIKELY(NULL == abs_rdispls_other)) {
        MPI_Win_free(&win);
        return OMPI_ERR_OUT_OF_RESOURCE;
      }
      abs_rdispls_local = (MPI_Aint*)malloc(csize * sizeof(MPI_Aint));
      if (OPAL_UNLIKELY(NULL == abs_rdispls_local)) {
        MPI_Win_free(&win);
        free(abs_rdispls_other);
        return OMPI_ERR_OUT_OF_RESOURCE;
      }
    
      // attach all pieces of local recvbuf to local window and record their absolute displacements
      for (int r=0;r<csize;++r) {
        res = win->w_osc_module->osc_win_attach(win, (char*)recvbuf+(recvext*rdispls[r]), recvext*recvcounts[r]);
        if (OMPI_SUCCESS != res) {
          PNBC_OSC_Error ("MPI Error in recvbuf win_attach (%i)\n", res);
          free(abs_rdispls_other);
          free(abs_rdispls_local);
          MPI_Win_free(&win);
          return res;
        }
        PNBC_OSC_DEBUG(1, "[pnbc_alltoallv_init] %d attached recv buf to dynamic window memory for rank %d with address %p (computed from rdispl value %d) of size %d bytes (computed from recvcount value %d)\n",
                       crank, r,
                       (char*)recvbuf+(recvext*rdispls[r]),
                       rdispls[r],
                       recvext*recvcounts[r],
                       recvcounts[r]);
    
        // compute displacement of local window memory portion
        MPI_Get_address(((char*)recvbuf) + (rdispls[r]*recvext), &(abs_rdispls_local[r]));
       	PNBC_OSC_DEBUG(1, "[pnbc_alltoallv_init] %d gets address at %ld (from rdispl %d giving pointer %p)\n",
                       crank, abs_rdispls_local[r], rdispls[r], ((char*)recvbuf)+(rdispls[r]*recvext));
        //abs_rdispls_local[r] = MPI_Aint_add(base_recvbuf, (MPI_Aint)rdispls[r]);
        //PNBC_OSC_DEBUG(1, "[nbc_allreduce_init] %d gets address at disp %ld\n",
        //               crank, abs_rdispls_local[r]);
      }
    
      // swap local rdispls for remote rdispls
      // put the displacements for all local portions on the window
      // get the displacements for all other portions on the window
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d about to exchange local and other recv displacements using alltoall\n",
                     crank);
      res = comm->c_coll->coll_alltoall(abs_rdispls_local, 1, MPI_AINT,
                                        abs_rdispls_other, 1, MPI_AINT,
                                        comm, comm->c_coll->coll_alltoall_module);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in alltoall for rdispls (%i)\n", res);
        free(abs_rdispls_other);
        free(abs_rdispls_local);
        MPI_Win_free(&win);
        return res;
      }
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d exchanged local and other recv displacements using alltoall\n",
                     crank);
    
      // the local absolute displacement values for portions of recvbuf are only needed remotely
      free(abs_rdispls_local);
    
      // ****************************
      // PUT_BASED WINDOW SETUP - END
      // ****************************

      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d about to call a2av_sched_trigger_push\n",
                     crank);
      res = a2av_sched_trigger_push(crank, csize, schedule, win, comm,
                                    sendbuf, sendcounts, sdispls, sendext, sendtype,
                                    recvbuf, recvcounts, rdispls, recvext, recvtype,
                                    abs_rdispls_other);

      free(abs_rdispls_other);

      if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
        PNBC_OSC_Error ("MPI Error in a2av_sched_linear (%i)\n", res);
        MPI_Win_free(&win);
        OBJ_RELEASE(schedule);
        return res;
      }

      break;

  } // end switch (algo)

  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d generated schedule (end of algo switch, flags_length is %d)\n",
                     crank, schedule->flags_length);

  // attach the flags memory to the win window (details provided by the schedule)
  if (OPAL_UNLIKELY(0 == schedule->flags_length)) {
    return OMPI_ERROR;
  } else {
    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Flags are at location %p with length %d (attached)\n", crank, schedule->flags, schedule->flags_length);
    res = win->w_osc_module->osc_win_attach(win, schedule->flags, schedule->flags_length);
    if (OMPI_SUCCESS != res) {
      PNBC_OSC_Error ("MPI Error in flag win_attach (%i)\n", res);
      MPI_Win_free(&win);
      OBJ_RELEASE(schedule);
      return res;
    }
  }
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d attached memory for flags\n",
                     crank);

  // lock the flags window at all other processes
  res = MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
  if (OMPI_SUCCESS != res) {
    PNBC_OSC_Error ("MPI Error in MPI_Win_lock_all (%i)\n", res);
    MPI_Win_free(&win);
    return res;
  }
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d locked window using MPI_WIN_LOCK_ALL\n",
                     crank);

//  res = PNBC_OSC_Sched_commit(schedule);
//  if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
//    PNBC_OSC_Error ("MPI Error in PNBC_OSC_Sched_commit (%i)", res);
//    free(abs_rdispls_other);
//    free(abs_rdispls_local);
//    MPI_Win_free(&win);
//    OBJ_RELEASE(schedule);
//    return res;
//  }
//  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d committed schedule\n",
//                     crank);

  res = PNBC_OSC_Schedule_request_win(schedule, comm, win, libpnbc_osc_module, persistent, request);
  if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
    PNBC_OSC_Error ("MPI Error in PNBC_OSC_Schedule_request_win (%i)\n", res);
    MPI_Win_free(&win);
    OBJ_RELEASE(schedule);
    return res;
  }
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d created top-level request\n",
                     crank);
  if(((ompi_coll_libpnbc_osc_request_t*)(*request))->win!=MPI_WIN_NULL){
    PNBC_OSC_DEBUG(10, "Window has not been free'd\n");
  }

  return OMPI_SUCCESS;
}

FLAG_t *FLAG_TRUE;
//static const int FLAG_TRUE = !0;

static inline int a2av_sched_trigger_pull(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                          MPI_Win win, MPI_Comm comm,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_sdispls_other) {
  // pull implies move means get and FLAG means RTS (ready to send)
  int res = OMPI_SUCCESS;
  FLAG_TRUE = malloc(sizeof(FLAG_t));
  *(long*)FLAG_TRUE = !(long)0;

  //schedule = OBJ_NEW(PNBC_OSC_Schedule);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 0\n", crank);

  schedule->triggers_length = 6 * csize;
  schedule->triggers = malloc(schedule->triggers_length * sizeof(triggerable_t));
  triggerable_t *triggers_phase0 = &(schedule->triggers[0 * csize]);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 0 starts at %p\n", crank, triggers_phase0);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 0 first starts at  %p\n", crank, triggers_phase0+0);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 0 second starts at  %p\n", crank, triggers_phase0+1);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 0 third starts at  %p\n", crank, triggers_phase0+2);

  triggerable_t *triggers_phase1 = &(schedule->triggers[1 * csize]);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 1 starts at %p\n", crank, triggers_phase1);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 1 first starts at  %p\n", crank, triggers_phase1+0);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 1 second starts at  %p\n", crank, triggers_phase1+1);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 1 third starts at  %p\n", crank, triggers_phase1+2);

  triggerable_t *triggers_phase2 = &(schedule->triggers[2 * csize]);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 2 starts at %p\n", crank, triggers_phase2);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 2 first starts at  %p\n", crank, triggers_phase2+0);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 2 second starts at  %p\n", crank, triggers_phase2+1);
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Trigger phase 2 third starts at  %p\n", crank, triggers_phase2+2);



  triggerable_t *triggers_phase3 = &(schedule->triggers[3 * csize]);
  triggerable_t *triggers_phase4 = &(schedule->triggers[4 * csize]);
  triggerable_t *triggers_phase5 = &(schedule->triggers[5 * csize]);

  schedule->flags = (FLAG_t*) malloc(5 * csize * sizeof(FLAG_t));
  FLAG_t *flags_rma_put_FLAG = &(schedule->flags[0 * csize]); // needs exposure via MPI window
  FLAG_t *flags_rma_put_DONE = &(schedule->flags[1 * csize]); // needs exposure via MPI window
  FLAG_t *flags_request_FLAG = &(schedule->flags[2 * csize]); // local usage only
  FLAG_t *flags_request_DATA = &(schedule->flags[3 * csize]); // local usage only
  FLAG_t *flags_request_DONE = &(schedule->flags[4 * csize]); // local usage only

  MPI_Aint *flag_displs = malloc(4 * csize * sizeof(MPI_Aint)); // used temporarily in this procedure only
  MPI_Aint *flag_displs_local = &flag_displs[0 * csize]; // used as input for MPI_Alltoall
  MPI_Aint *flag_displs_other = &flag_displs[2 * csize]; // used as output for MPI_Alltoall
  MPI_Aint *FLAG_displs_local = &flag_displs_local[0 * csize]; // set locally in MPI_Get_address
  MPI_Aint *DONE_displs_local = &flag_displs_local[1 * csize]; // set locally in MPI_Get_address
  MPI_Aint *FLAG_displs_other = &flag_displs_other[0 * csize]; // set remotely, copied into put_args
  MPI_Aint *DONE_displs_other = &flag_displs_other[1 * csize]; // set remotely, copied into put_args

  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 1\n", crank);

  for (int i=0;i<csize;++i) {
    MPI_Get_address(&flags_rma_put_FLAG[i], &FLAG_displs_local[i]);
    MPI_Get_address(&flags_rma_put_DONE[i], &DONE_displs_local[i]);
    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d My local displs (flag,done) are %d, %lu, %lu \n", crank, i, FLAG_displs_local[i], DONE_displs_local[i]);

  }

  MPI_Alltoall(FLAG_displs_local, 1, MPI_AINT, FLAG_displs_other, 1, MPI_AINT, comm);
  MPI_Alltoall(DONE_displs_local, 1, MPI_AINT, DONE_displs_other, 1, MPI_AINT, comm);

  //TODO: Change the name of flag_length (this is actually the size of memory)
  schedule->flags_length = 2*csize*sizeof(FLAG_t); // size of memory to expose to other ranks via window

  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Flags are at location %p with length %d (displacment %lu)\n", crank, schedule->flags, schedule->flags_length,FLAG_displs_local[0]);

  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 2\n", crank);

  schedule->requests = malloc(3 * csize * sizeof(MPI_Request*));
  MPI_Request *requests_rputFLAG = &(schedule->requests[0 * csize]); // circumvent the request?
  MPI_Request *requests_moveData = &(schedule->requests[1 * csize]); // combine into PUT_NOTIFY?
  MPI_Request *requests_rputDONE = &(schedule->requests[2 * csize]); // combine into PUT_NOTIFY?
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %lu test request address %p\n",&requests_rputFLAG[0],(char*)requests_rputFLAG[0]);

  schedule->action_args_list = malloc(3 * csize * sizeof(any_args_t));
  any_args_t *action_args_FLAG = &(schedule->action_args_list[0 * csize]);
  any_args_t *action_args_DATA = &(schedule->action_args_list[1 * csize]);
  any_args_t *action_args_DONE = &(schedule->action_args_list[2 * csize]);

  // schedule->trigger_arrays = malloc(6 * sizeof(triggerable_array)); // TODO:replace csize*triggerable_single with triggerable_array
  
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 3\n", crank);


  for (int p=0;p<csize;++p) {
    int orank = (crank+p)%csize;
    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 3.1\n", crank);

    // set 0 - triggered by: local start (responds to action from local user)
    //         trigger: reset all triggers, including schedule->triggers_active = 3*csize
    //         action: put FLAG signalling RTS (ready-to-send, i.e. ready for remote to get local data)
    triggers_phase0[orank].trigger = &(schedule->triggers_active);
    triggers_phase0[orank].test = &triggered_all_bynonzero_int;
    triggers_phase0[orank].action = action_all_put_p;
    triggers_phase0[orank].action_cbstate = &action_args_FLAG[orank];
    triggers_phase0[orank].reset = &reset_all_to_resetvalue_int;
    triggers_phase0[orank].reset_value = 3*csize;

    {
      put_args_t *args = &(action_args_FLAG[orank].put_args);
      args->buf = FLAG_TRUE;
      args->origin_count = 1;
      args->origin_datatype = MPI_LONG;
      args->target = orank;
      args->target_displ = FLAG_displs_other[orank];
      args-> target_count = 1;
      args->target_datatype = MPI_LONG;
      args->win = win;
      args->request = &requests_rputFLAG[orank];
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %lu request address %p\n",&args->request,(char*)args->request);
    }

    // set 1 - triggered by: remote rma put (responds to action from remote set 0)
    //         trigger: set local FLAG integer to non-zero value
    //         action: get DATA from remote into local output buffer

    triggers_phase1[orank].trigger = &flags_rma_put_FLAG[orank];

    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d: Setting trigger value to %p\n", crank, &flags_rma_put_FLAG[orank]);


    triggers_phase1[orank].test = &triggered_all_bynonzero_int;
    triggers_phase1[orank].action = action_all_get_p;
    triggers_phase1[orank].action_cbstate = &action_args_DATA[orank];

    triggers_phase1[orank].reset = &reset_all_to_zero_int;

    {
      get_args_t *args = &(action_args_DATA[orank].get_args);
      //TODO: verify (501) arithmetic
      //MPI_Get_address(((char*)recvbuf) + (rdispls[r]*recvext), &(abs_rdispls_local[r]));
      args->buf = (void*)&(((char*)recvbuf)[rdispls[orank]*recvext]);
      //args->buf = (void*)&(((char*)recvbuf)[recvext*orank]);
      args->origin_count = args->target_count = recvcounts[orank];
      args->origin_datatype = args->target_datatype = recvtype;
      args->target = orank;
      args->target_displ = abs_sdispls_other[orank];
      args->win = win;
      args->request = &requests_moveData[orank];
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %lu request address %p\n",&args->request,(char*)args->request);
    }

    // set 2 - triggered by: local test (completes the action from local set 1)
    //         trigger: MPI_Test(requests_moveData[orank], &flags_request_DATA[orank]);
    //         action: put DONE signalling data movement is complete; a portion of buffers are usable
    triggers_phase2[orank].trigger = &flags_request_DATA[orank];
    triggers_phase2[orank].test = &triggered_all_byrequest_flag;
    triggers_phase2[orank].test_cbstate = &requests_moveData[orank];
    triggers_phase2[orank].action = action_all_put_p;
    triggers_phase2[orank].action_cbstate = &action_args_DONE[orank];
    
    triggers_phase2[orank].reset = &reset_all_to_zero_int;

    {
      put_args_t *args = &(action_args_DONE[orank].put_args);
      args->buf = &FLAG_TRUE;
      args->origin_count = 1;
      args->origin_datatype = MPI_LONG;
      args->target = orank;
      args->target_displ = DONE_displs_other[orank];
      args-> target_count = 1;
      args->target_datatype = MPI_LONG;
      args->win = win;
      args->request = &requests_rputDONE[orank];
      PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %lu request address %p\n",&args->request,(char*)args->request);

    }

    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 3.2\n", crank);


    // set 3 - triggered by: local test (completes the action from local set 0)
    //         trigger: MPI_Test(requests_rputFLAG[orank], &flags_request_FLAG[orank]);
    //         action: update local progress counter
    triggers_phase3[orank].trigger = &flags_request_FLAG[orank];
    triggers_phase3[orank].test = &triggered_all_byrequest_flag;
    triggers_phase3[orank].test_cbstate = &requests_rputFLAG[orank];
    triggers_phase3[orank].action = action_all_decrement_int_p;
    triggers_phase3[orank].action_cbstate = &(schedule->triggers_active);

    triggers_phase3[orank].reset = &reset_all_to_zero_int;

    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 3.3\n", crank);

    // set 4 - triggered by: local test (completes the action from local set 2)
    //         trigger: MPI_Test(requests_rputDONE[orank], &flags_request_DONE[orank]);
    //         action: update local progress counter
    triggers_phase4[orank].trigger = &flags_request_DONE[orank];
    triggers_phase4[orank].test = &triggered_all_byrequest_flag;
    triggers_phase4[orank].test_cbstate = &requests_rputDONE[orank];
    triggers_phase4[orank].action = action_all_decrement_int_p;
    triggers_phase4[orank].action_cbstate = &(schedule->triggers_active);

    triggers_phase4[orank].reset = &reset_all_to_zero_int;

    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 3.4\n", crank);

    // set 5 - triggered by: remote rma put (responds to action from remote set 2)
    //         trigger: set local DONE integer to non-zero value
    //         action: update local progress counter
    triggers_phase5[orank].trigger = &flags_rma_put_DONE[orank];
    triggers_phase5[orank].test = &triggered_all_bynonzero_int;
    triggers_phase5[orank].action = action_all_decrement_int_p;
    triggers_phase5[orank].action_cbstate = &(schedule->triggers_active);

    triggers_phase5[orank].reset = &reset_all_to_zero_int;

    PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule 3.5\n", crank);

  }
  PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %d Generating pull schedule (end)\n", crank);


  free(flag_displs); // all remote values are now stored in put_args structs, we can get rid of this temp space

  return res;
}

static inline int a2av_sched_trigger_push(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                          MPI_Win win, MPI_Comm comm,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_rdispls_other) {
  // push implies move means put and FLAG means CTS (clear to send)
  int res = OMPI_SUCCESS;

  // schedule a copy for the local MPI process, if needed
  if (sendcounts[crank] != 0) {
  }

  return res;
}

