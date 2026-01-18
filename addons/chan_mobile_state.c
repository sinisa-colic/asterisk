/*
 * chan_mobile_state.c - State management helpers for chan_mobile
 *
 * This module provides functions for managing and querying
 * device and call states.
 *
 * This file is included by chan_mobile.c, not compiled separately.
 */

/* Note: This file is #included from chan_mobile.c after all headers */

/*
 * State string conversion functions
 */

const char *hfp_slc_state_str(enum hfp_slc_state state)
{
	switch (state) {
	case HFP_SLC_DISCONNECTED:   return "DISCONNECTED";
	case HFP_SLC_CONNECTING:     return "CONNECTING";
	case HFP_SLC_BRSF_SENT:      return "BRSF_SENT";
	case HFP_SLC_CIND_TEST_SENT: return "CIND_TEST_SENT";
	case HFP_SLC_CIND_SENT:      return "CIND_SENT";
	case HFP_SLC_CMER_SENT:      return "CMER_SENT";
	case HFP_SLC_CLIP_SENT:      return "CLIP_SENT";
	case HFP_SLC_ECAM_SENT:      return "ECAM_SENT";
	case HFP_SLC_VGS_SENT:       return "VGS_SENT";
	case HFP_SLC_CMGF_SENT:      return "CMGF_SENT";
	case HFP_SLC_CNMI_SENT:      return "CNMI_SENT";
	case HFP_SLC_CONNECTED:      return "CONNECTED";
	default:                     return "UNKNOWN";
	}
}

const char *mbl_call_state_str(enum mbl_call_state state)
{
	switch (state) {
	case MBL_CALL_IDLE:             return "IDLE";
	case MBL_CALL_INCOMING_RING:    return "INCOMING_RING";
	case MBL_CALL_INCOMING_WAIT_CID: return "WAIT_CALLER_ID";
	case MBL_CALL_OUTGOING_DIALING: return "DIALING";
	case MBL_CALL_OUTGOING_ALERTING: return "ALERTING";
	case MBL_CALL_ACTIVE:           return "ACTIVE";
	case MBL_CALL_HOLD:             return "HOLD";
	case MBL_CALL_WAITING:          return "WAITING";
	case MBL_CALL_HANGUP_PENDING:   return "HANGUP_PENDING";
	default:                        return "UNKNOWN";
	}
}

/*
 * State transition functions
 */

void mbl_set_call_state(struct mbl_pvt *pvt, enum mbl_call_state new_state)
{
	enum mbl_call_state old_state = pvt->call_state;
	if (old_state != new_state) {
		ast_verb(4, "[%s] Call state: %s -> %s\n", pvt->id,
			mbl_call_state_str(old_state), mbl_call_state_str(new_state));
		pvt->call_state = new_state;
	}
}

void mbl_set_hfp_state(struct mbl_pvt *pvt, enum hfp_slc_state new_state)
{
	if (pvt->hfp) {
		enum hfp_slc_state old_state = pvt->hfp->slc_state;
		if (old_state != new_state) {
			ast_verb(4, "[%s] HFP SLC state: %s -> %s\n", pvt->id,
				hfp_slc_state_str(old_state), hfp_slc_state_str(new_state));
			pvt->hfp->slc_state = new_state;
		}
	}
}

/*
 * State query functions
 */

int mbl_has_service(struct mbl_pvt *pvt)
{
	if (!pvt->hfp) {
		return 0;
	}
	return pvt->hfp->cind_state[pvt->hfp->cind_map.service] ? 1 : 0;
}

int mbl_in_call(struct mbl_pvt *pvt)
{
	return (pvt->call_state != MBL_CALL_IDLE);
}

int mbl_is_available(struct mbl_pvt *pvt)
{
	if (!pvt->connected) {
		return 0;
	}
	if (!mbl_hfp_initialized(pvt)) {
		return 0;
	}
	if (mbl_in_call(pvt)) {
		return 0;
	}
	if (pvt->incoming_sms || pvt->outgoing_sms) {
		return 0;
	}
	return 1;
}

int mbl_hfp_initialized(struct mbl_pvt *pvt)
{
	if (!pvt->hfp) {
		return 0;
	}
	return pvt->hfp->initialized ? 1 : 0;
}
