#include "pnbc_osc_debug.h"
#include <unistd.h>
#include "pnbc_osc_action_get.h"

static enum TRIGGER_ACTION_STATE action_all_get(get_args_t *get_args) {
  int ret = ACTION_SUCCESS;

PNBC_OSC_DEBUG(10, "Hello from action get!\n");
PNBC_OSC_DEBUG(5,"RGETPARAM *buf: %p, origin count: %i, origin type: %p, target: %i, target count: %i, target type: %p, target displ: %lu)\n",
                     get_args->buf, get_args->origin_count, get_args->origin_datatype, get_args->target,
                     get_args->target_count, get_args->target_datatype, get_args->target_displ);
PNBC_OSC_DEBUG(10, "[pnbc_alltoallv_init] %lu request address(get) %p\n",&get_args->request,(char*)get_args->request);


#ifdef PNBC_OSC_TIMING
      Iget_time -= MPI_Wtime();
#endif
//      if (get_args->origin_count!=0){
        ret = get_args->win->w_osc_module->osc_rget(get_args->buf,
                                                  get_args->origin_count, get_args->origin_datatype,
                                                get_args->target, get_args->target_displ,
                                                get_args->target_count, get_args->target_datatype,
                                                get_args->win, get_args->request);
        MPI_Wait(get_args->request, MPI_STATUS_IGNORE);
	int a = *(int*)get_args->buf;
        //PNBC_OSC_DEBUG(10, "Retrieved %d from %d count: %d\n",a,get_args->target,get_args->target_count);
	if(get_args->target_count>0){
          for (int i=1; i<get_args->target_count+1; i++){
	    PNBC_OSC_DEBUG(10, "Retrieved %d from %d count: %d\n",a,get_args->target,get_args->target_count);
	  }
	}

      //handle->req_count++;

        if (OMPI_SUCCESS != ret) {
          PNBC_OSC_Error("Error in osc_rget(%p, %i, %p, %i, %lu, %i, %p) (%i)\n",
                         get_args->buf,
                         get_args->origin_count, get_args->origin_datatype,
                         get_args->target, get_args->target_displ,
                         get_args->target_count, get_args->target_datatype,
                         ret);
        }
        else{
          PNBC_OSC_DEBUG(10,"RGET SUCCESS!\n");
        }
  //    }
  //    else{
  //      PNBC_OSC_DEBUG(10,"RGET SUCCESS (nothing to send!)\n"); //need to find a different way to handle this such that the request is preserved.
  //    }

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

