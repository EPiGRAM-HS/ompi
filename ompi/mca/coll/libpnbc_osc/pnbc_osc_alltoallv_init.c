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
  algo_linear_rput,
  algo_linear_rget,
  algo_trigger_pull,
  algo_trigger_push,
} a2av_sched_algo;

// pull implies move means get and FLAG means RTS (ready to send)
static inline int a2av_sched_trigger_pull(int crank, int csize, PNBC_OSC_Schedule *schedule,
//                                          FLAG_t **flags, int *fsize, int *req_count,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_sdispls_other);

// push implies move means put and FLAG means CTS (clear to send)
static inline int a2av_sched_trigger_push(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                          void *flags, int *fsize, int *req_count,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_rdispls_other);

static inline int a2av_sched_linear_rget(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                         void *flags, int *fsize, int *req_count,
                                         const void *sendbuf, const int *sendcounts, const int *sdispls,
                                         MPI_Aint sendext, MPI_Datatype sendtype,
                                               void *recvbuf, const int *recvcounts, const int *rdispls,
                                         MPI_Aint recvext, MPI_Datatype recvtype,
                                         MPI_Aint *abs_sdispls_other);

static inline int a2av_sched_linear_rput(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                         void *flags, int *fsize, int *req_count,
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
  MPI_Aint base_sendbuf, abs_sendbuf;
  MPI_Aint *abs_sdispls_other, *abs_sdispls_local;
  MPI_Aint base_recvbuf, abs_recvbuf;
  MPI_Aint *abs_rdispls_other, *abs_rdispls_local;
  MPI_Win win, winflag;
  void *flags = NULL;
  int fsize = 0;
  int req_count;
  ompi_coll_libpnbc_osc_module_t *libpnbc_osc_module = (ompi_coll_libpnbc_osc_module_t*) module;

  PNBC_OSC_IN_PLACE(sendbuf, recvbuf, inplace);
  if (inplace) {
    PNBC_OSC_Error("Error: inplace not implemented for PNBC_OSC AlltoallV");
    return OMPI_ERROR;
  }

  res = ompi_datatype_type_extent (recvtype, &recvext);
  if (MPI_SUCCESS != res) {
    PNBC_OSC_Error("MPI Error in ompi_datatype_type_extent() (%i)", res);
    return res;
  }

  res = ompi_datatype_type_extent (sendtype, &sendext);
  if (MPI_SUCCESS != res) {
    PNBC_OSC_Error("MPI Error in ompi_datatype_type_extent() (%i)", res);
    return res;
  }

  schedule = OBJ_NEW(PNBC_OSC_Schedule);
  if (OPAL_UNLIKELY(NULL == schedule)) {
    return OMPI_ERR_OUT_OF_RESOURCE;
  }

  crank = ompi_comm_rank(comm);
  csize = ompi_comm_size(comm);

  // TODO [DanH] add code to examine info and decide which algo to use

  a2av_sched_algo algo = algo_trigger_push;

  if ( check_config_value_equal("a2av_algo_requested", info, "linear_trigger_pull") )
    algo = algo_trigger_pull;
  if ( check_config_value_equal("a2av_algo_requested", info, "linear_trigger_push") )
    algo = algo_trigger_push;

  // END OF DanH changes for info

  // create a dynamic window - data here will be accessed by remote processes
  res = ompi_win_create_dynamic(&info->super, comm, &win);
  if (OMPI_SUCCESS != res) {
    PNBC_OSC_Error ("MPI Error in win_create_dynamic (%i)", res);
    return res;
  }


  switch (algo) {
    case algo_linear_rput: // ignore
      break;
    case algo_linear_rget: // ignore
      break;
    case algo_trigger_pull:
      // uses get to move data from the remote sendbuf - needs sdispls to be exchanged

      // ******************************
      // GET-BASED WINDOW SETUP - BEGIN
      // ******************************

      // compute absolute displacement as MPI_AINT for the sendbuf pointer
      res = MPI_Get_address(sendbuf, &base_sendbuf);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in MPI_Get_address (%i)", res);
        MPI_Win_free(&win);
        return res;
      }
      abs_sendbuf = MPI_Aint_add(MPI_BOTTOM, base_sendbuf);

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

      // attach all pieces of local sendbuf to local window and record their absolute displacements
      for (int r=0;r<csize;++r) {
        res = win->w_osc_module->osc_win_attach(win, (char*)sendbuf+sdispls[r], sendext*sendcounts[r]);
        if (OMPI_SUCCESS != res) {
          PNBC_OSC_Error ("MPI Error in win_create_dynamic (%i)", res);
          free(abs_sdispls_other);
          free(abs_sdispls_local);
          MPI_Win_free(&win);
          return res;
        }
        PNBC_OSC_DEBUG(1, "[pnbc_alltoallv_init] %d attaches to dynamic window memory for rank %d with address %p (computed from sdispl value %d) of size %d bytes (computed from sendcount value %d)\n",
                       crank, r,
                       (char*)sendbuf+sdispls[r],
                       sdispls[r],
                       sendext*sendcounts[r],
                       sendcounts[r]);

        // compute displacement of local window memory portion
        abs_sdispls_local[r] = MPI_Aint_add(abs_sendbuf, (MPI_Aint)sdispls[r]);
        PNBC_OSC_DEBUG(1, "[nbc_allreduce_init] %d gets address at disp %ld\n",
                       crank, abs_sdispls_local[r]);
      }

      // swap local rdispls for remote rdispls
      // put the displacements for all local portions on the window
      // get the displacements for all other portions on the window
      res = comm->c_coll->coll_alltoall(abs_sdispls_local, csize, MPI_AINT,
                                        abs_sdispls_other, csize, MPI_AINT,
                                        comm, comm->c_coll->coll_alltoall_module);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in alltoall for sdispls (%i)", res);
        free(abs_sdispls_other);
        free(abs_sdispls_local);
        MPI_Win_free(&win);
        return res;
      }

      // the local absolute displacement values for portions of sendbuf are only needed remotely
      free(abs_sdispls_local);

      // ****************************
      // GET_BASED WINDOW SETUP - END
      // ****************************


    case algo_trigger_push:
      // uses put to move data into the remote recvbuf - needs rdispls to be exchanged

      // ******************************
      // PUT-BASED WINDOW SETUP - BEGIN
      // ******************************
    
      // compute absolute displacement as MPI_AINT for the recvbuf pointer
      res = MPI_Get_address(recvbuf, &base_recvbuf);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in MPI_Get_address (%i)", res);
        MPI_Win_free(&win);
        return res;
      }
      abs_recvbuf = MPI_Aint_add(MPI_BOTTOM, base_recvbuf);
    
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
        res = win->w_osc_module->osc_win_attach(win, (char*)recvbuf+rdispls[r], recvext*recvcounts[r]);
        if (OMPI_SUCCESS != res) {
          PNBC_OSC_Error ("MPI Error in win_create_dynamic (%i)", res);
          free(abs_rdispls_other);
          free(abs_rdispls_local);
          MPI_Win_free(&win);
          return res;
        }
        PNBC_OSC_DEBUG(1, "[pnbc_alltoallv_init] %d attaches to dynamic window memory for rank %d with address %p (computed from rdispl value %d) of size %d bytes (computed from recvcount value %d)\n",
                       crank, r,
                       (char*)recvbuf+rdispls[r],
                       rdispls[r],
                       recvext*recvcounts[r],
                       recvcounts[r]);
    
        // compute displacement of local window memory portion
        abs_rdispls_local[r] = MPI_Aint_add(abs_recvbuf, (MPI_Aint)rdispls[r]);
        PNBC_OSC_DEBUG(1, "[nbc_allreduce_init] %d gets address at disp %ld\n",
                       crank, abs_rdispls_local[r]);
      }
    
      // swap local rdispls for remote rdispls
      // put the displacements for all local portions on the window
      // get the displacements for all other portions on the window
      res = comm->c_coll->coll_alltoall(abs_rdispls_local, csize, MPI_AINT,
                                        abs_rdispls_other, csize, MPI_AINT,
                                        comm, comm->c_coll->coll_alltoall_module);
      if (OMPI_SUCCESS != res) {
        PNBC_OSC_Error ("MPI Error in alltoall for rdispls (%i)", res);
        free(abs_rdispls_other);
        free(abs_rdispls_local);
        MPI_Win_free(&win);
        return res;
      }
    
      // the local absolute displacement values for portions of recvbuf are only needed remotely
      free(abs_rdispls_local);
    
      // ****************************
      // PUT_BASED WINDOW SETUP - END
      // ****************************

  } // end switch (algo)

  res = MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
  if (OMPI_SUCCESS != res) {
    PNBC_OSC_Error ("MPI Error in MPI_Win_lock_all (%i)", res);
    free(abs_rdispls_other);
    free(abs_rdispls_local);
    MPI_Win_free(&win);
    return res;
  }

  switch (algo) {
    case algo_linear_rput:
  res = a2av_sched_linear_rput(crank, csize, schedule, flags, &fsize, &req_count,
                               sendbuf, sendcounts, sdispls, sendext, sendtype,
                               recvbuf, recvcounts, rdispls, recvext, recvtype,
                               abs_rdispls_other);
    case algo_linear_rget:
  res = a2av_sched_linear_rget(crank, csize, schedule, flags, &fsize, &req_count,
                               sendbuf, sendcounts, sdispls, sendext, sendtype,
                               recvbuf, recvcounts, rdispls, recvext, recvtype,
                               abs_rdispls_other);
                             //abs_sdispls_other);
    case algo_trigger_pull:
  res = a2av_sched_trigger_pull(crank, csize, schedule, //flags, &fsize, &req_count,
                               sendbuf, sendcounts, sdispls, sendext, sendtype,
                               recvbuf, recvcounts, rdispls, recvext, recvtype,
                               abs_rdispls_other);
    case algo_trigger_push:
  res = a2av_sched_trigger_push(crank, csize, schedule, flags, &fsize, &req_count,
                               sendbuf, sendcounts, sdispls, sendext, sendtype,
                               recvbuf, recvcounts, rdispls, recvext, recvtype,
                               abs_rdispls_other);
                             //abs_sdispls_other);
  }
  if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
    PNBC_OSC_Error ("MPI Error in a2av_sched_linear (%i)", res);
    free(abs_rdispls_other);
    free(abs_rdispls_local);
    MPI_Win_free(&win);
    OBJ_RELEASE(schedule);
    return res;
  }

  res = PNBC_OSC_Sched_commit(schedule);
  if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
    PNBC_OSC_Error ("MPI Error in PNBC_OSC_Sched_commit (%i)", res);
    free(abs_rdispls_other);
    free(abs_rdispls_local);
    MPI_Win_free(&win);
    OBJ_RELEASE(schedule);
    return res;
  }

  if (0 == fsize) {
    winflag = MPI_WIN_NULL;
  } else {
    // create a dynamic window - flags will be signalled here
    res = ompi_win_create_dynamic(&info->super, comm, &winflag);
    if (OMPI_SUCCESS != res) {
      PNBC_OSC_Error ("MPI Error in win_create_dynamic (%i)", res);
      free(abs_rdispls_other);
      free(abs_rdispls_local);
      MPI_Win_free(&win);
      OBJ_RELEASE(schedule);
      return res;
    }

    // attach the flags memory to the winflag window (fsize provided by the schedule)
    res = win->w_osc_module->osc_win_attach(winflag, flags, fsize);
    if (OMPI_SUCCESS != res) {
      PNBC_OSC_Error ("MPI Error in win_create_dynamic (%i)", res);
      free(abs_rdispls_other);
      free(abs_rdispls_local);
      MPI_Win_free(&win);
      OBJ_RELEASE(schedule);
      MPI_Win_free(&winflag);
      return res;
    }

    // lock the flags window at all other processes
    res = MPI_Win_lock_all(MPI_MODE_NOCHECK, winflag);
    if (OMPI_SUCCESS != res) {
      PNBC_OSC_Error ("MPI Error in MPI_Win_lock_all for winflag (%i)", res);
      free(abs_rdispls_other);
      free(abs_rdispls_local);
      MPI_Win_free(&win);
      return res;
    }

  }

  res = PNBC_OSC_Schedule_request_win(schedule, comm, win, winflag, req_count, libpnbc_osc_module, persistent, request, flags);
  if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
    PNBC_OSC_Error ("MPI Error in PNBC_OSC_Schedule_request_win (%i)", res);
    free(abs_rdispls_other);
    free(abs_rdispls_local);
    MPI_Win_free(&win);
    OBJ_RELEASE(schedule);
    MPI_Win_free(&winflag);
    return res;
  }

  return OMPI_SUCCESS;
}

static inline int a2av_sched_trigger_pull(int crank, int csize, PNBC_OSC_Schedule *schedule,
//                                          FLAG_t **flags, int *fsize, int *req_count,
                                          const void *sendbuf, const int *sendcounts, const int *sdispls,
                                          MPI_Aint sendext, MPI_Datatype sendtype,
                                                void *recvbuf, const int *recvcounts, const int *rdispls,
                                          MPI_Aint recvext, MPI_Datatype recvtype,
                                          MPI_Aint *abs_sdispls_other) {
  // pull implies move means get and FLAG means RTS (ready to send)
  int res = OMPI_SUCCESS;

  schedule = OBJ_NEW(PNBC_OSC_Schedule);

  schedule->triggers = malloc(6 * csize * sizeof(triggerable_t));
  triggerable_t *triggers_phase0 = &(schedule->triggers[0 * csize * sizeof(triggerable_t)]);
  triggerable_t *triggers_phase1 = &(schedule->triggers[1 * csize * sizeof(triggerable_t)]);
  triggerable_t *triggers_phase2 = &(schedule->triggers[2 * csize * sizeof(triggerable_t)]);
  triggerable_t *triggers_phase3 = &(schedule->triggers[3 * csize * sizeof(triggerable_t)]);
  triggerable_t *triggers_phase4 = &(schedule->triggers[4 * csize * sizeof(triggerable_t)]);
  triggerable_t *triggers_phase5 = &(schedule->triggers[5 * csize * sizeof(triggerable_t)]);

  schedule->flags = malloc(5 * csize * sizeof(FLAG_t));
  FLAG_t *flags_rma_put_FLAG = &(schedule->flags[0 * csize * sizeof(FLAG_t)]); // needs exposure via MPI window
  FLAG_t *flags_rma_put_DONE = &(schedule->flags[1 * csize * sizeof(FLAG_t)]); // needs exposure via MPI window
  FLAG_t *flags_request_FLAG = &(schedule->flags[2 * csize * sizeof(FLAG_t)]); // local usage only
  FLAG_t *flags_request_DATA = &(schedule->flags[3 * csize * sizeof(FLAG_t)]); // local usage only
  FLAG_t *flags_request_DONE = &(schedule->flags[4 * csize * sizeof(FLAG_t)]); // local usage only

  schedule->requests = malloc(3 * csize * sizeof(MPI_Request*));
  MPI_Request **requests_rputFLAG = &(schedule->requests[0 * csize * sizeof(MPI_Request*)]); // circumvent the request?
  MPI_Request **requests_moveData = &(schedule->requests[1 * csize * sizeof(MPI_Request*)]); // combine into PUT_NOTIFY?
  MPI_Request **requests_rputDONE = &(schedule->requests[2 * csize * sizeof(MPI_Request*)]); // combine into PUT_NOTIFY?

  schedule->action_args_list = malloc(3 * csize * sizeof(any_args_t));
  any_args_t *action_args_FLAG = &(schedule->action_args_list[0 * csize * sizeof(any_args_t)]);
  any_args_t *action_args_DATA = &(schedule->action_args_list[1 * csize * sizeof(any_args_t)]);
  any_args_t *action_args_DONE = &(schedule->action_args_list[2 * csize * sizeof(any_args_t)]);

  schedule->trigger_arrays = malloc(6 * sizeof(triggerable_array)); // TODO:replace csize*triggerable_single with triggerable_array

  for (int p=0;p<csize;++p) {
    int orank = (crank+p)%csize;

    // set 0 - triggered by: local start (responds to action from local user)
    //         trigger: reset all triggers, including schedule->triggers_active = 3*csize
    //         action: put FLAG signalling RTS (ready-to-send, i.e. ready for remote to get local data)
    triggers_phase0[orank].trigger = &(schedule->triggers_active);
    triggers_phase0[orank].test = &triggered_all_bynonzero_int;
    triggers_phase0[orank].action = action_all_put_p;
    triggers_phase0[orank].action_cbstate = &action_args_FLAG[orank];

    // set 1 - triggered by: remote rma put (responds to action from remote set 0)
    //         trigger: set local FLAG integer to non-zero value
    //         action: get DATA from remote into local output buffer
    triggers_phase1[orank].trigger = &flags_rma_put_FLAG[orank];
    triggers_phase1[orank].test = &triggered_all_bynonzero_int;
    triggers_phase1[orank].action = action_all_get_p;
    triggers_phase1[orank].action_cbstate = &action_args_DATA[orank];

    // set 2 - triggered by: local test (completes the action from local set 1)
    //         trigger: MPI_Test(requests_moveData[orank], &flags_request_DATA[orank]);
    //         action: put DONE signalling data movement is complete; a portion of buffers are usable
    triggers_phase2[orank].trigger = &flags_request_DATA[orank];
    triggers_phase2[orank].test = &triggered_all_byrequest_flag;
    triggers_phase2[orank].test_cbstate = requests_moveData[orank];
    triggers_phase2[orank].action = action_all_put_p;
    triggers_phase2[orank].action_cbstate = &action_args_DONE[orank];

    // set 3 - triggered by: local test (completes the action from local set 0)
    //         trigger: MPI_Test(requests_rputFLAG[orank], &flags_request_FLAG[orank]);
    //         action: update local progress counter
    triggers_phase3[orank].trigger = &flags_request_FLAG[orank];
    triggers_phase3[orank].test = &triggered_all_byrequest_flag;
    triggers_phase3[orank].test_cbstate = requests_rputFLAG[orank];
    triggers_phase3[orank].action = action_all_decrement_int_p;
    triggers_phase3[orank].action_cbstate = &(schedule->triggers_active);

    // set 4 - triggered by: local test (completes the action from local set 2)
    //         trigger: MPI_Test(requests_rputDONE[orank], &flags_request_DONE[orank]);
    //         action: update local progress counter
    triggers_phase4[orank].trigger = &flags_request_DONE[orank];
    triggers_phase4[orank].test = &triggered_all_byrequest_flag;
    triggers_phase4[orank].test_cbstate = requests_rputDONE[orank];
    triggers_phase4[orank].action = action_all_decrement_int_p;
    triggers_phase4[orank].action_cbstate = &(schedule->triggers_active);

    // set 5 - triggered by: remote rma put (responds to action from remote set 2)
    //         trigger: set local DONE integer to non-zero value
    //         action: update local progress counter
    triggers_phase5[orank].trigger = &flags_rma_put_DONE[orank];
    triggers_phase5[orank].test = &triggered_all_bynonzero_int;
    triggers_phase5[orank].action = action_all_decrement_int_p;
    triggers_phase5[orank].action_cbstate = &(schedule->triggers_active);

  }

  // schedule a copy for the local MPI process, if needed
  if (recvcounts[crank] != 0) {
  }

  return res;
}

static inline int a2av_sched_trigger_push(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                          void *flags, int *fsize, int *req_count,
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

static inline int a2av_sched_linear_rget(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                         void *flags, int *fsize, int *req_count,
                                         const void *sendbuf, const int *sendcounts, const int *sdispls,
                                         MPI_Aint sendext, MPI_Datatype sendtype,
                                               void *recvbuf, const int *recvcounts, const int *rdispls,
                                         MPI_Aint recvext, MPI_Datatype recvtype,
                                         MPI_Aint *abs_sdispls_other) {
  int res;
  char *rbuf, *sbuf;

  // schedule a copy for the local MPI process, if needed
  if (recvcounts[crank] != 0) {
    sbuf = (char *) sendbuf + sdispls[crank] * sendext;
    rbuf = (char *) recvbuf + rdispls[crank] * recvext;
    res = PNBC_OSC_Sched_copy(sbuf, false, sendcounts[crank], sendtype,
                              rbuf, false, recvcounts[crank], recvtype, schedule, false);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
      OBJ_RELEASE(schedule);
      return res;
    }
  }

  *req_count = csize - 1;

  // schedule a get for each remote target, if needed
  for (int dist = 1 ; dist < csize ; ++dist) {
    // start from dist==1 to exclude local rank, local is a copy not a get
    int orank = (crank + dist) % csize;
    if (recvcounts[orank] != 0) {
      rbuf = (char *) recvbuf + rdispls[orank] * recvext;

      res = PNBC_OSC_Sched_rget(rbuf, orank,
                                sendcounts[orank], sendtype,
                                recvcounts[orank], recvtype,
                                abs_sdispls_other[orank],
                                schedule, false);

      if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
        OBJ_RELEASE(schedule);
        return res;
      }

    }
  }

  return OMPI_SUCCESS;
}

static inline int a2av_sched_linear_rput(int crank, int csize, PNBC_OSC_Schedule *schedule,
                                         void *flags, int *fsize, int *req_count,
                                         const void *sendbuf, const int *sendcounts, const int *sdispls,
                                         MPI_Aint sendext, MPI_Datatype sendtype,
                                               void *recvbuf, const int *recvcounts, const int *rdispls,
                                         MPI_Aint recvext, MPI_Datatype recvtype,
                                         MPI_Aint *abs_rdispls_other) {
  int res;
  char *rbuf, *sbuf;

  // schedule a copy for the local MPI process, if needed
  if (sendcounts[crank] != 0) {
    sbuf = (char *) sendbuf + sdispls[crank] * sendext;
    rbuf = (char *) recvbuf + rdispls[crank] * recvext;
    res = PNBC_OSC_Sched_copy(sbuf, false, sendcounts[crank], sendtype,
                              rbuf, false, recvcounts[crank], recvtype, schedule, false);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
      OBJ_RELEASE(schedule);
      return res;
    }
  }

  *req_count = csize - 1;

  // schedule a put for each remote target, if needed
  for (int dist = 1 ; dist < csize ; ++dist) {
    // start from dist==1 to exclude local rank, local is a copy not a put
    int orank = (crank + dist) % csize;
    if (sendcounts[orank] != 0) {
      sbuf = (char *) sendbuf + sdispls[orank] * sendext;

      res = PNBC_OSC_Sched_rput(sbuf, orank,
                                sendcounts[orank], sendtype,
                                recvcounts[orank], recvtype,
                                abs_rdispls_other[orank],
                                schedule, false);

      if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
        OBJ_RELEASE(schedule);
        return res;
      }

    }
  }

  return OMPI_SUCCESS;
}

