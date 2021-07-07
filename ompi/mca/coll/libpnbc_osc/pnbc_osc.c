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
#include "pnbc_osc_trigger_common.h"

/* should be moved into pnbc_osc_request.c */
int PNBC_OSC_Start(PNBC_OSC_Handle *handle) {

  // bozo case - starting a null request is no-op
  if (OPAL_UNLIKELY((ompi_request_t *)handle == &ompi_request_empty)) {
    return OMPI_SUCCESS;
  }

  // bozo case - starting an active request is erroneous
  if (OPAL_UNLIKELY(OMPI_REQUEST_ACTIVE == handle->super.req_state)) {
    return OMPI_ERR_BAD_PARAM;
  }
  //TODO: Loop over all trigger arrays and reset
  //TODO: Reset triggers 

  PNBC_OSC_DEBUG(10, "[pnbc_osc]\n ");
  for(int t=0;t<handle->schedule->triggers_length;t++) {
    PNBC_OSC_DEBUG(10, "[pnbc_osc] about to reset trigger @ %p\n",&handle->schedule->triggers[t]);
    trigger_reset(&handle->schedule->triggers[t]);
  }

  // change state of request to ACTIVE, permits progress for this request
  handle->super.req_state = OMPI_REQUEST_ACTIVE;

  // make this request visible to the progress engine
  OPAL_THREAD_LOCK(&mca_coll_libpnbc_osc_component.lock);
  opal_list_append(&mca_coll_libpnbc_osc_component.active_requests, &(handle->super.super.super));
  OPAL_THREAD_UNLOCK(&mca_coll_libpnbc_osc_component.lock);

  return OMPI_SUCCESS;
}


/* progresses a request
 *
 * to be called *only* from the progress thread !!! */
int PNBC_OSC_Progress(PNBC_OSC_Handle *handle) {
  PNBC_OSC_DEBUG(60, "Entering progress engine for single handle\n");

  enum TRIGGER_ACTION_STATE state;

  // protection against progression error - should never happen
  if (OPAL_UNLIKELY(OMPI_REQUEST_ACTIVE != handle->super.req_state)) {
    PNBC_OSC_DEBUG(10, "NULL (2)\n");
    return OMPI_ERR_BAD_PARAM;
  }
  
  
  PNBC_OSC_DEBUG(10, "Triggers_Length: %d\n",handle->schedule->triggers_length);
  // test each triggerable in the schedule
  for (int t=0;t<handle->schedule->triggers_length;++t) {
    PNBC_OSC_DEBUG(10, "About to test trigger @: %p, %d\n",&(handle->schedule->triggers[t]),t);
    state = trigger_test(&(handle->schedule->triggers[t]));
    PNBC_OSC_DEBUG(10, "TRIGGER TESTED: %p, %d\n",&(handle->schedule->triggers[t]),t);

    if(handle->schedule->triggers_active==0){
      handle->super.req_state=OMPI_REQUEST_INACTIVE;
      PNBC_OSC_DEBUG(10, "NO MORE TRIGGERS ACTIVE -- SETTING REQUEST TO INACTIVE");
    }

    PNBC_OSC_DEBUG(10, "Triggers_Active: %d\n",handle->schedule->triggers_active);
    if (OPAL_UNLIKELY(ACTION_PROBLEM == state)) {
      PNBC_OSC_DEBUG(10, "RTRN 1\n");
      return OMPI_ERR_NOT_SUPPORTED;
    }
  }

  PNBC_OSC_DEBUG(10, "Triggers_Array_Length: %d\n",handle->schedule->trigger_arrays_length);
  // test each trigger_array in the schedule
  for (int t=0;t<handle->schedule->trigger_arrays_length;++t) {
    state = trigger_test_all(&(handle->schedule->trigger_arrays[t]));
    if (OPAL_UNLIKELY(ACTION_PROBLEM == state)) {
      PNBC_OSC_DEBUG(10, "RTRN 2\n");
      return OMPI_ERR_NOT_SUPPORTED;
    }
  }

  //if(handle->schedule->triggers_active==0){
  //  handle->super.req_state=OMPI_REQUEST_INACTIVE;
  //}


  // check new state of this request (will eventually have been changed by a trigger)
  if (OMPI_REQUEST_ACTIVE != handle->super.req_state) { 
    PNBC_OSC_DEBUG(10, "RTRN 3\n");
    return PNBC_OSC_SUCCESS;
  } else {
    PNBC_OSC_DEBUG(60, "RTRN 4\n");
    return PNBC_OSC_CONTINUE;
  }
}

