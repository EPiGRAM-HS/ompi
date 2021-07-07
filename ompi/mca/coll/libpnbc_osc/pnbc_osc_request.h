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

#ifndef LIBPNBC_OSC_REQUEST_H
#define LIBPNBC_OSC_REQUEST_H

#include "ompi/request/request.h"
#include "coll_libpnbc_osc.h"
#include "pnbc_osc_schedule.h"

BEGIN_C_DECLS

struct ompi_coll_libpnbc_osc_request_t {
    ompi_request_t super;
    PNBC_OSC_Schedule *schedule;
    MPI_Comm comm;
    bool nbc_complete; /* status in libpnbc_osc level */
    MPI_Win win;
};
typedef struct ompi_coll_libpnbc_osc_request_t ompi_coll_libpnbc_osc_request_t;
OBJ_CLASS_DECLARATION(ompi_coll_libpnbc_osc_request_t);



#define OMPI_COLL_LIBPNBC_OSC_REQUEST_ALLOC(comm, persistent, req)           \
    do {                                                                \
        opal_free_list_item_t *item;                                    \
        item = opal_free_list_wait (&mca_coll_libpnbc_osc_component.requests); \
        req = (ompi_coll_libpnbc_osc_request_t*) item;                       \
        OMPI_REQUEST_INIT(&req->super, persistent);                     \
        req->super.req_mpi_object.comm = comm;                          \
    } while (0)

#define OMPI_COLL_LIBPNBC_OSC_REQUEST_RETURN(req)                            \
    do {                                                                \
        OMPI_REQUEST_FINI(&(req)->super);                               \
        opal_free_list_return (&mca_coll_libpnbc_osc_component.requests,     \
                               (opal_free_list_item_t*) (req));         \
    } while (0)



END_C_DECLS

#endif /* LIBPNBC_OSC_REQUEST_H */
