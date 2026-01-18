/*
 * chan_mobile_ami.c - AMI event helpers for chan_mobile
 *
 * This module provides helper functions for emitting AMI events
 * related to mobile device status, calls, and SMS.
 *
 * This file is included by chan_mobile.c, not compiled separately.
 */

/* Note: This file is #included from chan_mobile.c after all headers */

/*
 * Device status events
 */

void mbl_ami_device_connect(struct mbl_pvt *pvt, const char *address)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileStatus",
		"Status: Connect\r\n"
		"Device: %s\r\n"
		"Address: %s\r\n",
		pvt->id, address ? address : "");
}

void mbl_ami_device_disconnect(struct mbl_pvt *pvt, const char *reason)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileStatus",
		"Status: Disconnect\r\n"
		"Device: %s\r\n"
		"Reason: %s\r\n",
		pvt->id, reason ? reason : "Unknown");
}

void mbl_ami_device_initialized(struct mbl_pvt *pvt)
{
	int service = 0, signal = 0, battery = 0;
	
	if (pvt->hfp) {
		service = pvt->hfp->cind_state[pvt->hfp->cind_map.service];
		signal = pvt->hfp->cind_state[pvt->hfp->cind_map.signal];
		battery = pvt->hfp->cind_state[pvt->hfp->cind_map.battchg];
	}
	
	manager_event(EVENT_FLAG_SYSTEM, "MobileStatus",
		"Status: Initialized\r\n"
		"Device: %s\r\n"
		"Service: %d\r\n"
		"Signal: %d\r\n"
		"Battery: %d\r\n",
		pvt->id, service, signal, battery);
}

void mbl_ami_connect_failed(struct mbl_pvt *pvt, const char *reason)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileStatus",
		"Status: ConnectFailed\r\n"
		"Device: %s\r\n"
		"Reason: %s\r\n"
		"Failures: %d\r\n"
		"BackoffSeconds: %d\r\n",
		pvt->id, reason ? reason : "Unknown",
		pvt->connect_failures, pvt->backoff_seconds);
}

/*
 * Indicator events
 */

void mbl_ami_signal(struct mbl_pvt *pvt, int signal)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileSignal",
		"Device: %s\r\n"
		"Signal: %d\r\n",
		pvt->id, signal);
}

void mbl_ami_battery(struct mbl_pvt *pvt, int battery)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileBattery",
		"Device: %s\r\n"
		"Battery: %d\r\n",
		pvt->id, battery);
}

void mbl_ami_service(struct mbl_pvt *pvt, int service)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileService",
		"Device: %s\r\n"
		"Service: %d\r\n",
		pvt->id, service);
}

void mbl_ami_roaming(struct mbl_pvt *pvt, int roaming)
{
	manager_event(EVENT_FLAG_SYSTEM, "MobileRoaming",
		"Device: %s\r\n"
		"Roaming: %d\r\n",
		pvt->id, roaming);
}

/*
 * Call events
 */

void mbl_ami_call_incoming_wait(struct mbl_pvt *pvt)
{
	manager_event(EVENT_FLAG_CALL, "MobileCallIncoming",
		"Device: %s\r\n"
		"State: WaitingCallerID\r\n",
		pvt->id);
}

void mbl_ami_call_incoming(struct mbl_pvt *pvt, const char *caller_id, const char *caller_name)
{
	manager_event(EVENT_FLAG_CALL, "MobileCallIncoming",
		"Device: %s\r\n"
		"State: Ringing\r\n"
		"CallerID: %s\r\n"
		"CallerIDName: %s\r\n",
		pvt->id,
		caller_id ? caller_id : "Unknown",
		caller_name ? caller_name : "");
}

void mbl_ami_call_outgoing(struct mbl_pvt *pvt, const char *destination)
{
	manager_event(EVENT_FLAG_CALL, "MobileCallOutgoing",
		"Device: %s\r\n"
		"Destination: %s\r\n",
		pvt->id, destination ? destination : "");
}

void mbl_ami_call_start(struct mbl_pvt *pvt, int is_incoming)
{
	if (is_incoming) {
		manager_event(EVENT_FLAG_CALL, "MobileCallStart",
			"Device: %s\r\n"
			"Direction: Incoming\r\n"
			"CallerID: %s\r\n",
			pvt->id, pvt->caller_id);
	} else {
		manager_event(EVENT_FLAG_CALL, "MobileCallStart",
			"Device: %s\r\n"
			"Direction: Outgoing\r\n"
			"DialedNumber: %s\r\n",
			pvt->id, pvt->dialed_number);
	}
}

void mbl_ami_call_end(struct mbl_pvt *pvt, int was_incoming, int was_outgoing,
                      int was_answered, int duration, const char *caller_id,
                      const char *dialed_number, const char *previous_state)
{
	const char *direction = was_incoming ? "Incoming" : (was_outgoing ? "Outgoing" : "Unknown");
	
	manager_event(EVENT_FLAG_CALL, "MobileCallEnd",
		"Device: %s\r\n"
		"Direction: %s\r\n"
		"Duration: %d\r\n"
		"Answered: %s\r\n"
		"CallerID: %s\r\n"
		"DialedNumber: %s\r\n"
		"PreviousState: %s\r\n",
		pvt->id,
		direction,
		duration,
		was_answered ? "Yes" : "No",
		caller_id ? caller_id : "",
		dialed_number ? dialed_number : "",
		previous_state ? previous_state : "");
}

void mbl_ami_audio_error(struct mbl_pvt *pvt, const char *error_msg)
{
	manager_event(EVENT_FLAG_CALL, "MobileAudioError",
		"Device: %s\r\n"
		"ErrorCount: %u\r\n"
		"Error: %s\r\n",
		pvt->id, pvt->audio_errors, error_msg ? error_msg : "Unknown");
}

/*
 * SMS events
 */

void mbl_ami_sms_received(struct mbl_pvt *pvt, const char *from, const char *text)
{
	manager_event(EVENT_FLAG_CALL, "MobileSMSReceived",
		"Device: %s\r\n"
		"From: %s\r\n"
		"Message: %s\r\n",
		pvt->id,
		from ? from : "Unknown",
		text ? text : "");
}

void mbl_ami_sms_sent(struct mbl_pvt *pvt, const char *to)
{
	manager_event(EVENT_FLAG_CALL, "MobileSMSSent",
		"Device: %s\r\n"
		"To: %s\r\n",
		pvt->id, to ? to : "");
}
