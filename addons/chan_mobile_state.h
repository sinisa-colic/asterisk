/*
 * chan_mobile_state.h - State management helpers for chan_mobile
 *
 * This header is included by chan_mobile.c after structure definitions.
 */

#ifndef CHAN_MOBILE_STATE_H
#define CHAN_MOBILE_STATE_H

/* Forward declarations - structures defined in chan_mobile.c */
struct mbl_pvt;
struct hfp_pvt;

/* State enums - defined in chan_mobile.c */
enum hfp_slc_state;
enum mbl_call_state;

/*
 * State string conversion functions
 */

/* Convert HFP SLC state to string */
const char *hfp_slc_state_str(enum hfp_slc_state state);

/* Convert call state to string */
const char *mbl_call_state_str(enum mbl_call_state state);

/*
 * State transition functions
 */

/* Set call state with logging */
void mbl_set_call_state(struct mbl_pvt *pvt, enum mbl_call_state new_state);

/* Set HFP SLC state with logging */
void mbl_set_hfp_state(struct mbl_pvt *pvt, enum hfp_slc_state new_state);

/*
 * State query functions
 */

/* Check if device has cellular service */
int mbl_has_service(struct mbl_pvt *pvt);

/* Check if device is in a call */
int mbl_in_call(struct mbl_pvt *pvt);

/* Check if device is available for new call */
int mbl_is_available(struct mbl_pvt *pvt);

/* Check if HFP is fully initialized */
int mbl_hfp_initialized(struct mbl_pvt *pvt);

#endif /* CHAN_MOBILE_STATE_H */
