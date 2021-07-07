#include "pnbc_osc_action_decrement.h"
#include "pnbc_osc_debug.h"


static enum TRIGGER_ACTION_STATE action_all_decrement_int(dec_args_t *const dec_args) {
  int ret = ACTION_SUCCESS;
  PNBC_OSC_DEBUG(10,"Dec_args %d!\n",*dec_args);

  (*dec_args)--;
  return ret;
}

trigger_action_all_cb_fn_t action_all_decrement_int_p = (trigger_action_all_cb_fn_t)action_all_decrement_int;

static enum TRIGGER_ACTION_STATE action_one_decrement_int(int index, dec_args_t *const dec_args) {
  int ret = ACTION_SUCCESS;
  (*dec_args)--;
  return ret;
}

trigger_action_one_cb_fn_t action_one_decrement_int_p = (trigger_action_one_cb_fn_t)action_one_decrement_int;
