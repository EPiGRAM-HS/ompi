/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2013-2015 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2014-2017 Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2016-2017 IBM Corporation.  All rights reserved.
 * Copyright (c) 2018      FUJITSU LIMITED.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PNBC_OSC_SCHEDULE_H
#define PNBC_OSC_SCHEDULE_H

#include "opal/class/opal_object.h"
#include "ompi/request/request.h"
#include "pnbc_osc_trigger_common.h"
#include "pnbc_osc_trigger_single.h"
#include "pnbc_osc_trigger_array.h"
#include "pnbc_osc_action_common.h"

BEGIN_C_DECLS

/* struct holding the entire Schedule
   legacy NBC schedule will use heap memory pointed to by *data
   modern PNBC schedules will use the flexible rounds array
*/
struct PNBC_OSC_Schedule {
  opal_object_t super;
  long triggers_active;                  // for trigger-based schedule
  int triggers_length;                  // for trigger-based schedule
  triggerable_t *triggers;              // for trigger-based schedule
  int trigger_arrays_length;            // for trigger-based schedule
  triggerable_array *trigger_arrays;    // for trigger-based schedule
  FLAG_t *flags;                        // for trigger-based schedule
  int flags_length;                     // for tracking size of flags array
  MPI_Request *requests;               // for trigger-based schedule
  any_args_t *action_args_list;         // for trigger-based schedule
};
typedef struct PNBC_OSC_Schedule PNBC_OSC_Schedule;
OBJ_CLASS_DECLARATION(PNBC_OSC_Schedule);


END_C_DECLS

#endif /* PNBC_OSC_SCHEDULE_H */
