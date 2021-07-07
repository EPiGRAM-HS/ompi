/* -*- Mode: C; c-basic-offset:2 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2006      The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2013-2018 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2006      The Technical University of Chemnitz. All
 *                         rights reserved.
 * Copyright (c) 2015      Los Alamos National Security, LLC.  All rights
 *                         reserved.
 * Copyright (c) 2015-2018 Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 *
 * Author(s): Torsten Hoefler <htor@cs.indiana.edu>
 *
 * Copyright (c) 2012      Oracle and/or its affiliates.  All rights reserved.
 * Copyright (c) 2016      IBM Corporation.  All rights reserved.
 * Copyright (c) 2017      Ian Bradley Morgan and Anthony Skjellum. All
 *                         rights reserved.
 * Copyright (c) 2018      FUJITSU LIMITED.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 */
#include "pnbc_osc_debug.h"
#include "pnbc_osc_internal.h"
#include "pnbc_osc_schedule.h"

static void PNBC_OSC_Schedule_constructor (PNBC_OSC_Schedule *schedule) {
  // nothing needed here
}

static void PNBC_OSC_Schedule_destructor (PNBC_OSC_Schedule *schedule) {
  // TODO make sure referenced objects are destroyed and referenced memory is freed
}

OBJ_CLASS_INSTANCE(PNBC_OSC_Schedule, opal_object_t,
                   PNBC_OSC_Schedule_constructor,
                   PNBC_OSC_Schedule_destructor);


int PNBC_OSC_Schedule_request_win(PNBC_OSC_Schedule *schedule, ompi_communicator_t *comm,
                                  ompi_win_t *win,
                                  ompi_coll_libpnbc_osc_module_t *module,
                                  bool persistent, ompi_request_t **request) {
  bool need_register = false;
  ompi_coll_libpnbc_osc_request_t *handle;
  int ret;

  PNBC_OSC_DEBUG(10, "Schedule Request (0)\n");


  OMPI_COLL_LIBPNBC_OSC_REQUEST_ALLOC(comm, persistent, handle);
  if (NULL == handle) return OMPI_ERR_OUT_OF_RESOURCE;

  handle->nbc_complete = persistent ? true : false;
  handle->schedule = schedule;
  handle->comm = comm;
  handle->win = win;

  /******************** Do the shadow comm administration ...  ***************/

  OPAL_THREAD_LOCK(&module->mutex);
  if (true != module->comm_registered) {
    module->comm_registered = true;
    need_register = true;
  }
  OPAL_THREAD_UNLOCK(&module->mutex);

  /* register progress */
  if (need_register) {
    int32_t tmp =
      OPAL_THREAD_ADD_FETCH32(&mca_coll_libpnbc_osc_component.active_comms, 1);
    if (tmp == 1) {
      PNBC_OSC_DEBUG(10, "Schedule Request (1)\n");
      ret = opal_progress_register(ompi_coll_libpnbc_osc_progress);
      if (OMPI_SUCCESS != ret) {
        PNBC_OSC_DEBUG(10, "FAiled TO REGISTER\n");
      }
      else{
	PNBC_OSC_DEBUG(10, "Register Succeeded\n");
      }
    }
  }

  /******************** end of shadow comm administration ...  ***************/

  *request = (ompi_request_t *) handle;

  return OMPI_SUCCESS;
}
