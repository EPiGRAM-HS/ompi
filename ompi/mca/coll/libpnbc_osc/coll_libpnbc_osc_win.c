/* -*- Mode: C; c-basic-offset:2 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2006      The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2006      The Technical University of Chemnitz. All
 *                         rights reserved.
 * Copyright (c) 2013-2017 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2014-2018 Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2017      IBM Corporation.  All rights reserved.
 * Copyright (c) 2018      FUJITSU LIMITED.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * Author(s): Luis Cebamanos <l.cebamanos@epcc.ed.ac.uk>, Daniel Holmes <d.homes@epcc.ed.ac.uk>
 *
 */

#include "ompi/mca/osc/rdma/osc_rdma.h"
#include "coll_libpnbc_osc.h"

#include "mpi.h"
#include "ompi/mca/coll/coll.h"
#include "ompi/communicator/communicator.h"
#include "pnbc_osc_internal.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/op/op.h"
#include "opal/util/bit_ops.h"
#include "ompi/win/win.h"
#include "ompi/mca/osc/osc.h"

#include <assert.h>

/* No schedule assigned to win_icreate */
static int NBC_Win_icreate_dynamic (opal_info_t *info, ompi_communicator_t *comm,
                                    ompi_communicator_t **newcomm, ompi_win_t **win,
                                    ompi_request_t **request ){
  
  int res;
  
  res = ompi_win_icreate_dynamic(info, comm, newcomm, win, request);
  if (OMPI_SUCCESS != res) {
    NBC_Error ("MPI Error in ompi_win_icreate_dynamic (%i)", res);
    return res;
  }
  
  return OMPI_SUCCESS;
}



