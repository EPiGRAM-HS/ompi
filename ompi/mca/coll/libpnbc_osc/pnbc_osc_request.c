#include "pnbc_osc_request.h"
#include "pnbc_osc_debug.h"

int PNBC_OSC_Start(ompi_coll_libpnbc_osc_request_t *handle);
void PNBC_OSC_Free (ompi_coll_libpnbc_osc_request_t* handle);

static void request_construct(ompi_coll_libpnbc_osc_request_t *request);
static void request_destruct(ompi_coll_libpnbc_osc_request_t *request);
static int request_start(size_t count, ompi_request_t ** requests);
static int request_cancel(struct ompi_request_t *request, int complete);
static int request_free(struct ompi_request_t **ompi_req);

OBJ_CLASS_INSTANCE(ompi_coll_libpnbc_osc_request_t,
                   ompi_request_t,
                   request_construct,
                   request_destruct);


static void
request_construct(ompi_coll_libpnbc_osc_request_t *request)
{
    request->super.req_type = OMPI_REQUEST_COLL;
    request->super.req_status._cancelled = 0;
    request->super.req_start = request_start;
    request->super.req_cancel = request_cancel;
    request->super.req_free = request_free;
}

static void
request_destruct(ompi_coll_libpnbc_osc_request_t *request)
{
  if (NULL != request->schedule)
    OBJ_DESTRUCT(request->schedule);
  if (MPI_WIN_NULL != request->win) {
    MPI_Win_unlock_all(request->win);
    OBJ_DESTRUCT(request->win);
  }
}

static int
request_start(size_t count, ompi_request_t ** requests)
{
    int res;
    size_t i;
//todo: change debug messages
    PNBC_OSC_DEBUG(5, " ** request_start **\n");

    for (i = 0; i < count; i++) {
        ompi_coll_libpnbc_osc_request_t *handle = (ompi_coll_libpnbc_osc_request_t *) requests[i];

        PNBC_OSC_DEBUG(5, "--------------------------------\n");
        PNBC_OSC_DEBUG(5, "handle %p size %u\n", &handle, sizeof(handle));
        PNBC_OSC_DEBUG(5, "schedule %p size %u\n", &handle->schedule, sizeof(handle->schedule));
        if (NULL != handle->schedule) {
          PNBC_OSC_DEBUG(5, "triggers %p length %u\n", &handle->schedule->triggers, sizeof(handle->schedule->triggers_length));
          PNBC_OSC_DEBUG(5, "trigger_arrays %p length %u\n", &handle->schedule->trigger_arrays, sizeof(handle->schedule->trigger_arrays_length));
        }
        PNBC_OSC_DEBUG(5, "--------------------------------\n");

        handle->super.req_complete = REQUEST_PENDING;
        handle->nbc_complete = false;

        res = PNBC_OSC_Start(handle);
        if (OPAL_UNLIKELY(OMPI_SUCCESS != res)) {
            PNBC_OSC_DEBUG(5, " ** bad result from PNBC_OSC_Start **\n");
            return res;
        }
    }

    PNBC_OSC_DEBUG(5, " ** LEAVING request_start **\n");

    return OMPI_SUCCESS;

}


static int
request_cancel(struct ompi_request_t *request, int complete)
{
    return MPI_ERR_REQUEST;
}


static int
request_free(struct ompi_request_t **ompi_req)
{
    ompi_coll_libpnbc_osc_request_t *request =
        (ompi_coll_libpnbc_osc_request_t*) *ompi_req;

    if( !REQUEST_COMPLETE(&request->super) ) {
        return MPI_ERR_REQUEST;
    }

    PNBC_OSC_Free(request);

    OMPI_COLL_LIBPNBC_OSC_REQUEST_RETURN(request);
    *ompi_req = MPI_REQUEST_NULL;

    return OMPI_SUCCESS;
}


void PNBC_OSC_Free (ompi_coll_libpnbc_osc_request_t* handle) {

  if (NULL != handle->schedule) {
    /* release schedule */
    OBJ_RELEASE (handle->schedule);
    handle->schedule = NULL;
  }

}

