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

#include "ompi_config.h"

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


/* int NBC_Sched_complete_iwin_dynamic() */
/* { */
/*   ompi_osc_rdma_module_t *module; */
/*   return NBC_Sched_complete_iwin_dynamic_internal (module); */
/* } */

/* static int NBC_Sched_complete_iwin_dynamic_internal ( ompi_osc_rdma_module_t *module ) */
/* { */
/*   ompi_osc_rdma_region_t *my_data; */
/*   int ret, global_result; */
/*   int my_rank = ompi_comm_rank (module->comm); */
/*   int comm_size = ompi_comm_size (module->comm); */
/*   ompi_osc_rdma_rank_data_t *temp; */

/*    do { */
/*         temp = malloc (sizeof (*temp) * comm_size); */
/*         if (NULL == temp) { */
/*             ret = OMPI_ERR_OUT_OF_RESOURCE; */
/*             break; */
/*         } */
        
/*         /\* fill in rank -> node translation *\/ */
/*         temp[my_rank].node_id = module->node_id; */
/*         temp[my_rank].rank = ompi_comm_rank (module->shared_comm); */
        
/*         ret = module->comm->c_coll->coll_allgather (MPI_IN_PLACE, 1, MPI_2INT, temp, 1, MPI_2INT, */
/*                                                     module->comm, */
/*                                                     module->comm->c_coll->coll_allgather_module); */
/*         if (OMPI_SUCCESS != ret) { */
/*             break; */
/*         } */
        
/*         if (0 == ompi_comm_rank (module->shared_comm)) { */
/*             /\* fill in my part of the node array *\/ */
/*             my_data = (ompi_osc_rdma_region_t *) ((intptr_t)module->node_comm_info + */
/*                                                   ompi_comm_rank(module->local_leaders) * */
/*                                                   module->region_size); */
            
/*             my_data->base = (uint64_t) (intptr_t) module->rank_array; */
/*             /\* store my rank in the length field *\/ */
/*             my_data->len = (osc_rdma_size_t) my_rank; */

/*             if (module->selected_btl->btl_register_mem) { */
/*               memcpy (my_data->btl_handle_data, module->state_handle, */
/*                       module->selected_btl->btl_registration_handle_size); */
/*             } */

/*              /\* gather state data at each node leader *\/ */
/*             if (ompi_comm_size (module->local_leaders) > 1) { */
/*                 ret = module->local_leaders->c_coll->coll_allgather (MPI_IN_PLACE, */
/*                                                                      module->region_size, MPI_BYTE, */
/*                                                                      module->node_comm_info, */
/*                                                                      module->region_size, MPI_BYTE, */
/*                                                                      module->local_leaders, */
/*                                                                      module->local_leaders->c_coll->coll_allgather_module); */
/*                 if (OMPI_SUCCESS != ret) { */
/*                     OSC_RDMA_VERBOSE(MCA_BASE_VERBOSE_ERROR, */
/*                                      "leader allgather failed with ompi error code %d", ret); */
/*                     break; */
/*                 } */
/*             } */

/*             int base_rank = ompi_comm_rank (module->local_leaders) * */
/*               ((comm_size + module->node_count - 1) /  module->node_count); */
            
/*             /\* fill in the local part of the rank -> node map *\/ */
/*             for (int i = 0 ; i < RANK_ARRAY_COUNT(module) ; ++i) { */
/*               int save_rank = base_rank + i; */
/*               if (save_rank >= comm_size) { */
/*                 break; */
/*               } */
              
/*               module->rank_array[i] = temp[save_rank]; */
/*             } */
/*         } */
        
/*         free (temp); */
/*    } while (0); */
   
   
/*    ret = module->comm->c_coll->coll_allreduce (&ret, &global_result, 1, MPI_INT, MPI_MIN, */
/*                                                module->comm, */
/*                                                module->comm->c_coll->coll_allreduce_module); */
   
/*    if (OMPI_SUCCESS != ret) { */
/*      global_result = ret; */
/*    } */

/*    /\* none of these communicators are needed anymore so free them now*\/ */
/*    if (MPI_COMM_NULL != module->local_leaders) { */
/*      ompi_comm_free (&module->local_leaders); */
/*    } */
   
/*     if (MPI_COMM_NULL != module->shared_comm) { */
/*       ompi_comm_free (&module->shared_comm); */
/*     } */
    
/*     return global_result; */
    
/* } */
