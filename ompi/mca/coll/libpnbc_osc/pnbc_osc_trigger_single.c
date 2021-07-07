#include "pnbc_osc_debug.h"
#include "pnbc_osc_trigger_single.h"


void trigger_reset(triggerable_t *thing) {
  thing->reset(thing->trigger, thing->reset_value);
}

enum TRIGGER_ACTION_STATE trigger_test(triggerable_t *thing) {
  enum TRIGGER_ACTION_STATE ret = TRIGGER_PENDING;
  PNBC_OSC_DEBUG(10, "Testing the value of: %p\n",thing->trigger);
  if (thing->test(thing->trigger, thing->test_cbstate)) {
    PNBC_OSC_DEBUG(10, "Trigger at location %p, action cbstate at %p\n",thing,thing->action_cbstate);
    ret = thing->action(thing->action_cbstate);
    if (thing->auto_reset && ACTION_SUCCESS == ret)
      PNBC_OSC_DEBUG(10, "Trigger (auto) reset\n");
      trigger_reset(thing);
  }
  return ret;
}

enum TRIGGER_ACTION_STATE trigger_action(triggerable_t *thing) {
  return thing->action(thing->action_cbstate);
}

