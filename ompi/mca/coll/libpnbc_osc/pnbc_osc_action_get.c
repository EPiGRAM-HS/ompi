#include "pnbc_osc_debug.h"
#include "pnbc_osc_action_get.h"

static enum TRIGGER_ACTION_STATE action_all_get(get_args_t *get_args) {
  int ret = ACTION_SUCCESS;

PNBC_OSC_DEBUG(5,"*buf: %p, origin count: %i, origin type: %p, target: %i, target count: %i, target type: %p, target displ: %lu)\n",
                     get_args->buf, get_args->origin_count, get_args->origin_datatype, get_args->target,
                     get_args->target_count, get_args->target_datatype, get_args->target_displ);

#ifdef PNBC_OSC_TIMING
      Iget_time -= MPI_Wtime();
#endif

      ret = get_args->win->w_osc_module->osc_rget(get_args->buf,
                                                get_args->origin_count, get_args->origin_datatype,
                                                get_args->target, get_args->target_displ,
                                                get_args->target_count, get_args->target_datatype,
                                                get_args->win, get_args->request);
      //handle->req_count++;

      if (OMPI_SUCCESS != ret) {
        PNBC_OSC_Error("Error in osc_rget(%p, %i, %p, %i, %lu, %i, %p) (%i)",
                       get_args->buf,
                       get_args->origin_count, get_args->origin_datatype,
                       get_args->target, get_args->target_displ,
                       get_args->target_count, get_args->target_datatype,
                       ret);
      }

#ifdef PNBC_OSC_TIMING
      Iget_time += MPI_Wtime();
#endif



  return ret;
}

trigger_action_all_cb_fn_t action_all_get_p = (trigger_action_all_cb_fn_t)action_all_get;

static enum TRIGGER_ACTION_STATE action_one_get(int index, get_args_t *get_args) {
  return action_all_get(get_args);
}

trigger_action_one_cb_fn_t action_one_get_p = (trigger_action_one_cb_fn_t)action_one_get;

