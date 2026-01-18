/*
 * chan_mobile CLI commands implementation
 *
 * This file is #included by chan_mobile.c and contains all CLI command handlers.
 * It is not compiled separately.
 */

/* CLI Commands implementation */

static char *handle_cli_mobile_show_devices(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt;
	char bdaddr[18];
	char group[6];

#define FORMAT1 "%-15.15s %-17.17s %-5.5s %-15.15s %-9.9s %-10.10s %-3.3s\n"

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile show devices";
		e->usage =
			"Usage: mobile show devices\n"
			"       Shows the state of Bluetooth Cell / Mobile devices.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	ast_cli(a->fd, FORMAT1, "ID", "Address", "Group", "Adapter", "Connected", "State", "SMS");
	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		ast_mutex_lock(&pvt->lock);
		ba2str(&pvt->addr, bdaddr);
		snprintf(group, sizeof(group), "%d", pvt->group);
		ast_cli(a->fd, FORMAT1,
				pvt->id,
				bdaddr,
				group,
				pvt->adapter->id,
				pvt->connected ? "Yes" : "No",
				(!pvt->connected) ? "None" : (pvt->owner) ? "Busy" : (pvt->outgoing_sms || pvt->incoming_sms) ? "SMS" : (mbl_has_service(pvt)) ? "Free" : "No Service",
				(pvt->has_sms) ? "Yes" : "No"
		       );
		ast_mutex_unlock(&pvt->lock);
	}
	AST_RWLIST_UNLOCK(&devices);

#undef FORMAT1

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_show_device(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;
	char bdaddr[18];

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile show device";
		e->usage =
			"Usage: mobile show device <id>\n"
			"       Shows detailed status of a specific Bluetooth device.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[3]))
			break;
	}

	if (!pvt) {
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Device '%s' not found.\n", a->argv[3]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	ba2str(&pvt->addr, bdaddr);

	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "=== Device: %s ===\n", pvt->id);
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  Address:           %s\n", bdaddr);
	ast_cli(a->fd, "  Type:              %s\n", pvt->type == MBL_TYPE_PHONE ? "Phone (HFP)" : "Headset (HSP)");
	ast_cli(a->fd, "  Adapter:           %s\n", pvt->adapter ? pvt->adapter->id : "(none)");
	ast_cli(a->fd, "  Group:             %d\n", pvt->group);
	ast_cli(a->fd, "  Context:           %s\n", pvt->context);
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Connection Status ---\n");
	ast_cli(a->fd, "  Connected:         %s\n", pvt->connected ? "Yes" : "No");
	ast_cli(a->fd, "  RFCOMM Port:       %d\n", pvt->rfcomm_port);
	ast_cli(a->fd, "  RFCOMM Socket:     %d\n", pvt->rfcomm_socket);
	ast_cli(a->fd, "  SCO Socket:        %d\n", pvt->sco_socket);
	if (pvt->hfp) {
		ast_cli(a->fd, "  HFP Initialized:   %s\n", pvt->hfp->initialized ? "Yes" : "No");
		ast_cli(a->fd, "  HFP SLC State:     %s\n", hfp_slc_state_str(pvt->hfp->slc_state));
	}
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Call Status ---\n");
	ast_cli(a->fd, "  Call State:        %s\n", mbl_call_state_str(pvt->call_state));
	ast_cli(a->fd, "  Has Channel:       %s\n", pvt->owner ? "Yes" : "No");
	ast_cli(a->fd, "  Incoming:          %s\n", pvt->incoming ? "Yes" : "No");
	ast_cli(a->fd, "  Outgoing:          %s\n", pvt->outgoing ? "Yes" : "No");
	ast_cli(a->fd, "  Answered:          %s\n", pvt->answered ? "Yes" : "No");
	ast_cli(a->fd, "  Need Hangup:       %s\n", pvt->needchup ? "Yes" : "No");
	if (pvt->caller_id[0]) {
		ast_cli(a->fd, "  Caller ID:         %s\n", pvt->caller_id);
	}
	if (pvt->dialed_number[0]) {
		ast_cli(a->fd, "  Dialed Number:     %s\n", pvt->dialed_number);
	}
	if (pvt->audio_errors > 0) {
		ast_cli(a->fd, "  Audio Errors:      %u\n", pvt->audio_errors);
	}
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- SMS Status ---\n");
	ast_cli(a->fd, "  SMS Capable:       %s\n", pvt->has_sms ? "Yes" : "No");
	ast_cli(a->fd, "  Incoming SMS:      %s\n", pvt->incoming_sms ? "Yes" : "No");
	ast_cli(a->fd, "  Outgoing SMS:      %s\n", pvt->outgoing_sms ? "Yes" : "No");
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- HFP Indicators ---\n");
	if (pvt->hfp) {
		ast_cli(a->fd, "  Service:           %d (idx=%d)\n", 
			pvt->hfp->cind_state[pvt->hfp->cind_map.service], pvt->hfp->cind_map.service);
		ast_cli(a->fd, "  Call:              %d (idx=%d)\n", 
			pvt->hfp->cind_state[pvt->hfp->cind_map.call], pvt->hfp->cind_map.call);
		ast_cli(a->fd, "  Call Setup:        %d (idx=%d)\n", 
			pvt->hfp->cind_state[pvt->hfp->cind_map.callsetup], pvt->hfp->cind_map.callsetup);
		ast_cli(a->fd, "  Signal:            %d (idx=%d)\n", 
			pvt->hfp->cind_state[pvt->hfp->cind_map.signal], pvt->hfp->cind_map.signal);
		ast_cli(a->fd, "  Battery:           %d (idx=%d)\n", 
			pvt->hfp->cind_state[pvt->hfp->cind_map.battchg], pvt->hfp->cind_map.battchg);
	}
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Flags ---\n");
	ast_cli(a->fd, "  Blackberry Mode:   %s\n", pvt->blackberry ? "Yes" : "No");
	ast_cli(a->fd, "  No Callsetup:      %s\n", pvt->no_callsetup ? "Yes" : "No");
	ast_cli(a->fd, "  Alignment Detect:  %s\n", pvt->do_alignment_detection ? "Yes" : "No");
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Connection Retry ---\n");
	ast_cli(a->fd, "  Connect Failures:  %d\n", pvt->connect_failures);
	ast_cli(a->fd, "  Backoff Seconds:   %d\n", pvt->backoff_seconds);
	if (pvt->last_connect_attempt > 0) {
		time_t now = time(NULL);
		int elapsed = (int)(now - pvt->last_connect_attempt);
		ast_cli(a->fd, "  Last Attempt:      %d seconds ago\n", elapsed);
		if (pvt->backoff_seconds > 0 && elapsed < pvt->backoff_seconds) {
			ast_cli(a->fd, "  Next Attempt In:   %d seconds\n", pvt->backoff_seconds - elapsed);
		}
	}
	if (pvt->last_disconnect_time > 0) {
		time_t now = time(NULL);
		int elapsed = (int)(now - pvt->last_disconnect_time);
		ast_cli(a->fd, "  Last Disconnect:   %d seconds ago\n", elapsed);
		ast_cli(a->fd, "  Disconnect Reason: %s\n", pvt->last_disconnect_reason);
	}
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Statistics ---\n");
	if (pvt->connected && pvt->connected_since > 0) {
		time_t now = time(NULL);
		int uptime = (int)(now - pvt->connected_since);
		int hours = uptime / 3600;
		int mins = (uptime % 3600) / 60;
		int secs = uptime % 60;
		ast_cli(a->fd, "  Connected For:     %02d:%02d:%02d\n", hours, mins, secs);
	}
	ast_cli(a->fd, "  Calls In:          %u\n", pvt->calls_in);
	ast_cli(a->fd, "  Calls Out:         %u\n", pvt->calls_out);
	ast_cli(a->fd, "  Calls Answered:    %u\n", pvt->calls_answered);
	{
		unsigned long total_secs = pvt->total_call_seconds;
		int hours = total_secs / 3600;
		int mins = (total_secs % 3600) / 60;
		int secs = total_secs % 60;
		ast_cli(a->fd, "  Total Call Time:   %02d:%02d:%02d\n", hours, mins, secs);
	}
	ast_cli(a->fd, "  SMS In:            %u\n", pvt->sms_in);
	ast_cli(a->fd, "  SMS Out:           %u\n", pvt->sms_out);
	ast_cli(a->fd, "\n");

	ast_mutex_unlock(&pvt->lock);
	AST_RWLIST_UNLOCK(&devices);

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_show_adapters(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct adapter_pvt *adapter;
	char bdaddr[18];

#define FORMAT1 "%-15.15s %-5.5s %-17.17s %-10.10s %-12.12s %-10.10s\n"

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile show adapters";
		e->usage =
			"Usage: mobile show adapters\n"
			"       Shows the state of Bluetooth adapters.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	ast_cli(a->fd, FORMAT1, "ID", "DevID", "Address", "In Use", "SCO Socket", "SCO Thread");
	AST_RWLIST_RDLOCK(&adapters);
	AST_RWLIST_TRAVERSE(&adapters, adapter, entry) {
		ba2str(&adapter->addr, bdaddr);
		ast_cli(a->fd, FORMAT1,
			adapter->id,
			adapter->dev_id >= 0 ? "hciX" : "N/A",
			bdaddr,
			adapter->inuse ? "Yes" : "No",
			adapter->sco_socket >= 0 ? "Open" : "Closed",
			adapter->sco_listener_thread != AST_PTHREADT_NULL ? "Running" : "Stopped"
		);
	}
	AST_RWLIST_UNLOCK(&adapters);

#undef FORMAT1

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_reset_backoff(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile reset backoff";
		e->usage =
			"Usage: mobile reset backoff <id>\n"
			"       Resets the connection backoff for a device, allowing immediate retry.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[3]))
			break;
	}

	if (!pvt) {
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Device '%s' not found.\n", a->argv[3]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	pvt->connect_failures = 0;
	pvt->backoff_seconds = 0;
	pvt->last_connect_attempt = 0;
	ast_mutex_unlock(&pvt->lock);
	AST_RWLIST_UNLOCK(&devices);

	ast_cli(a->fd, "Backoff reset for device '%s'. Will retry on next discovery cycle.\n", a->argv[3]);

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_show_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt;
	unsigned int total_calls_in = 0, total_calls_out = 0;
	unsigned int total_sms_in = 0, total_sms_out = 0;

#define STATS_FORMAT_HDR "%-15.15s %-10.10s %8.8s %8.8s %8.8s %8.8s %12.12s\n"
#define STATS_FORMAT     "%-15.15s %-10.10s %8u %8u %8u %8u %12.12s\n"

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile show stats";
		e->usage =
			"Usage: mobile show stats\n"
			"       Shows call and SMS statistics for all mobile devices.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	ast_cli(a->fd, STATS_FORMAT_HDR, "Device", "Status", "CallsIn", "CallsOut", "SMSIn", "SMSOut", "Uptime");
	ast_cli(a->fd, "-------------------------------------------------------------------------------\n");

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		char uptime_str[16] = "N/A";
		
		if (pvt->connected && pvt->connected_since > 0) {
			time_t now = time(NULL);
			int uptime = (int)(now - pvt->connected_since);
			int hours = uptime / 3600;
			int mins = (uptime % 3600) / 60;
			snprintf(uptime_str, sizeof(uptime_str), "%02d:%02d", hours, mins);
		}
		
		ast_cli(a->fd, STATS_FORMAT,
			pvt->id,
			pvt->connected ? "Connected" : "Disconn",
			pvt->calls_in,
			pvt->calls_out,
			pvt->sms_in,
			pvt->sms_out,
			uptime_str
		);
		
		total_calls_in += pvt->calls_in;
		total_calls_out += pvt->calls_out;
		total_sms_in += pvt->sms_in;
		total_sms_out += pvt->sms_out;
	}
	AST_RWLIST_UNLOCK(&devices);

	ast_cli(a->fd, "-------------------------------------------------------------------------------\n");
	ast_cli(a->fd, STATS_FORMAT,
		"TOTAL", "", total_calls_in, total_calls_out, total_sms_in, total_sms_out, "");

#undef STATS_FORMAT_HDR
#undef STATS_FORMAT

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_reset_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile reset stats";
		e->usage =
			"Usage: mobile reset stats <id|all>\n"
			"       Resets call/SMS statistics for a device or all devices.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_WRLOCK(&devices);
	
	if (!strcmp(a->argv[3], "all")) {
		AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
			ast_mutex_lock(&pvt->lock);
			pvt->calls_in = 0;
			pvt->calls_out = 0;
			pvt->calls_answered = 0;
			pvt->total_call_seconds = 0;
			pvt->sms_in = 0;
			pvt->sms_out = 0;
			ast_mutex_unlock(&pvt->lock);
		}
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Statistics reset for all devices.\n");
		return CLI_SUCCESS;
	}

	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[3]))
			break;
	}

	if (!pvt) {
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Device '%s' not found.\n", a->argv[3]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	pvt->calls_in = 0;
	pvt->calls_out = 0;
	pvt->calls_answered = 0;
	pvt->total_call_seconds = 0;
	pvt->sms_in = 0;
	pvt->sms_out = 0;
	ast_mutex_unlock(&pvt->lock);
	AST_RWLIST_UNLOCK(&devices);

	ast_cli(a->fd, "Statistics reset for device '%s'.\n", a->argv[3]);

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_check(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;
	int checks_passed = 0;
	int checks_failed = 0;
	int checks_warning = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile check";
		e->usage =
			"Usage: mobile check <id>\n"
			"       Performs health checks on a mobile device.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}

	if (!pvt) {
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Device '%s' not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_cli(a->fd, "\n=== Health Check for Device '%s' ===\n\n", pvt->id);

	/* Check 1: Connection status */
	ast_cli(a->fd, "  [%s] Connection Status\n", pvt->connected ? "PASS" : "FAIL");
	if (pvt->connected) {
		checks_passed++;
	} else {
		checks_failed++;
		if (pvt->connect_failures > 0) {
			ast_cli(a->fd, "        -> %d failed connection attempts, backoff: %d seconds\n",
				pvt->connect_failures, pvt->backoff_seconds);
		}
	}

	/* Check 2: RFCOMM socket */
	if (pvt->connected) {
		ast_cli(a->fd, "  [%s] RFCOMM Socket\n", pvt->rfcomm_socket >= 0 ? "PASS" : "FAIL");
		if (pvt->rfcomm_socket >= 0) {
			checks_passed++;
		} else {
			checks_failed++;
		}
	}

	/* Check 3: HFP State */
	if (pvt->connected && pvt->hfp) {
		const char *state_str = hfp_slc_state_str(pvt->hfp->slc_state);
		int is_connected = (pvt->hfp->slc_state == HFP_SLC_CONNECTED);
		ast_cli(a->fd, "  [%s] HFP Service Level Connection (%s)\n",
			is_connected ? "PASS" : "WARN", state_str);
		if (is_connected) {
			checks_passed++;
		} else {
			checks_warning++;
			ast_cli(a->fd, "        -> HFP initialization may be in progress\n");
		}
	}

	/* Check 4: Cellular service */
	if (pvt->connected && pvt->hfp) {
		int has_service = mbl_has_service(pvt);
		ast_cli(a->fd, "  [%s] Cellular Service\n", has_service ? "PASS" : "WARN");
		if (has_service) {
			checks_passed++;
		} else {
			checks_warning++;
			ast_cli(a->fd, "        -> No cellular service available\n");
		}
	}

	/* Check 5: Signal strength */
	if (pvt->connected && pvt->hfp) {
		int signal = pvt->hfp->cind_state[pvt->hfp->cind_map.signal];
		const char *result = signal >= 3 ? "PASS" : (signal >= 1 ? "WARN" : "FAIL");
		ast_cli(a->fd, "  [%s] Signal Strength (%d/5)\n", result, signal);
		if (signal >= 3) {
			checks_passed++;
		} else if (signal >= 1) {
			checks_warning++;
			ast_cli(a->fd, "        -> Low signal may affect call quality\n");
		} else {
			checks_failed++;
			ast_cli(a->fd, "        -> No signal\n");
		}
	}

	/* Check 6: Battery level */
	if (pvt->connected && pvt->hfp) {
		int battery = pvt->hfp->cind_state[pvt->hfp->cind_map.battchg];
		const char *result = battery >= 2 ? "PASS" : (battery >= 1 ? "WARN" : "FAIL");
		ast_cli(a->fd, "  [%s] Battery Level (%d/5)\n", result, battery);
		if (battery >= 2) {
			checks_passed++;
		} else if (battery >= 1) {
			checks_warning++;
			ast_cli(a->fd, "        -> Low battery\n");
		} else {
			checks_failed++;
			ast_cli(a->fd, "        -> Critical battery level\n");
		}
	}

	/* Check 7: Not roaming */
	if (pvt->connected && pvt->hfp) {
		int roaming = pvt->hfp->cind_state[pvt->hfp->cind_map.roam];
		ast_cli(a->fd, "  [%s] Roaming Status\n", roaming ? "WARN" : "PASS");
		if (roaming) {
			checks_warning++;
			ast_cli(a->fd, "        -> Device is roaming (may incur charges)\n");
		} else {
			checks_passed++;
		}
	}

	/* Check 8: No active call (available) */
	if (pvt->connected) {
		int busy = (pvt->owner != NULL || pvt->incoming || pvt->outgoing);
		ast_cli(a->fd, "  [%s] Device Available\n", busy ? "INFO" : "PASS");
		if (busy) {
			ast_cli(a->fd, "        -> Device is currently in use\n");
		} else {
			checks_passed++;
		}
	}

	/* Check 9: Last disconnect reason */
	if (pvt->last_disconnect_time > 0) {
		time_t now = time(NULL);
		int elapsed = (int)(now - pvt->last_disconnect_time);
		if (elapsed < 300) { /* Within last 5 minutes */
			ast_cli(a->fd, "  [WARN] Recent Disconnect (%d seconds ago)\n", elapsed);
			ast_cli(a->fd, "        -> Reason: %s\n", pvt->last_disconnect_reason);
			checks_warning++;
		}
	}

	AST_RWLIST_UNLOCK(&devices);

	/* Summary */
	ast_cli(a->fd, "\n  --- Summary ---\n");
	ast_cli(a->fd, "  Passed:   %d\n", checks_passed);
	ast_cli(a->fd, "  Warnings: %d\n", checks_warning);
	ast_cli(a->fd, "  Failed:   %d\n", checks_failed);
	
	if (checks_failed == 0 && checks_warning == 0) {
		ast_cli(a->fd, "\n  Overall: HEALTHY\n\n");
	} else if (checks_failed == 0) {
		ast_cli(a->fd, "\n  Overall: OK (with warnings)\n\n");
	} else {
		ast_cli(a->fd, "\n  Overall: UNHEALTHY\n\n");
	}

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_disconnect(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile disconnect";
		e->usage =
			"Usage: mobile disconnect <id>\n"
			"       Disconnects a mobile device. It will automatically reconnect\n"
			"       on the next discovery cycle unless you set a backoff.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}

	if (!pvt) {
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Device '%s' not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	if (!pvt->connected) {
		AST_RWLIST_UNLOCK(&devices);
		ast_cli(a->fd, "Device '%s' is not connected.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	
	/* If there's an active call, hang it up first */
	if (pvt->owner) {
		ast_cli(a->fd, "Device '%s' has an active call, hanging up...\n", a->argv[2]);
		mbl_queue_hangup(pvt);
	}
	
	/* Close RFCOMM socket to trigger disconnect */
	if (pvt->rfcomm_socket >= 0) {
		close(pvt->rfcomm_socket);
		pvt->rfcomm_socket = -1;
	}
	
	/* Set disconnect reason */
	ast_copy_string(pvt->last_disconnect_reason, "CLI disconnect command", sizeof(pvt->last_disconnect_reason));
	pvt->last_disconnect_time = time(NULL);
	
	ast_mutex_unlock(&pvt->lock);
	AST_RWLIST_UNLOCK(&devices);

	ast_cli(a->fd, "Device '%s' disconnected. Will reconnect on next discovery cycle.\n", a->argv[2]);
	ast_cli(a->fd, "Use 'mobile reset backoff %s' to force immediate reconnection.\n", a->argv[2]);

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_show_version(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt;
	struct adapter_pvt *adapter;
	int device_count = 0;
	int adapter_count = 0;
	int connected_count = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile show version";
		e->usage =
			"Usage: mobile show version\n"
			"       Shows chan_mobile version and feature summary.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	/* Count devices and adapters */
	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		device_count++;
		if (pvt->connected) {
			connected_count++;
		}
	}
	AST_RWLIST_UNLOCK(&devices);

	AST_RWLIST_RDLOCK(&adapters);
	AST_RWLIST_TRAVERSE(&adapters, adapter, entry) {
		adapter_count++;
	}
	AST_RWLIST_UNLOCK(&adapters);

	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "=== chan_mobile - Modernized Version ===\n\n");
	ast_cli(a->fd, "  Version:           1.0-modernized\n");
	ast_cli(a->fd, "  Build Date:        %s %s\n", __DATE__, __TIME__);
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Features ---\n");
	ast_cli(a->fd, "  HFP State Machine: Yes (12 states)\n");
	ast_cli(a->fd, "  Connection Retry:  Yes (exponential backoff)\n");
	ast_cli(a->fd, "  Keepalive:         Yes (60s timeout)\n");
	ast_cli(a->fd, "  Statistics:        Yes (calls, SMS, duration)\n");
	ast_cli(a->fd, "  AMI Events:        Yes (extended)\n");
	ast_cli(a->fd, "  Health Check:      Yes\n");
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- Status ---\n");
	ast_cli(a->fd, "  Adapters:          %d\n", adapter_count);
	ast_cli(a->fd, "  Devices:           %d configured\n", device_count);
	ast_cli(a->fd, "  Connected:         %d\n", connected_count);
	ast_cli(a->fd, "\n");
	ast_cli(a->fd, "  --- CLI Commands ---\n");
	ast_cli(a->fd, "  mobile show devices      - List all devices\n");
	ast_cli(a->fd, "  mobile show device <id>  - Device details\n");
	ast_cli(a->fd, "  mobile show adapters     - List adapters\n");
	ast_cli(a->fd, "  mobile show stats        - Statistics summary\n");
	ast_cli(a->fd, "  mobile check <id>        - Health check\n");
	ast_cli(a->fd, "  mobile disconnect <id>   - Force disconnect\n");
	ast_cli(a->fd, "  mobile reset backoff <id>- Reset retry backoff\n");
	ast_cli(a->fd, "  mobile reset stats <id>  - Reset statistics\n");
	ast_cli(a->fd, "  mobile search            - Scan for devices\n");
	ast_cli(a->fd, "  mobile rfcomm <id> <cmd> - Send AT command\n");
	ast_cli(a->fd, "  mobile cusd <id> <code>  - Send USSD code\n");
	ast_cli(a->fd, "  mobile dial <id> <num>   - Dial a number\n");
	ast_cli(a->fd, "  mobile answer <id>       - Answer incoming call\n");
	ast_cli(a->fd, "  mobile hangup <id>       - Hang up call\n");
	ast_cli(a->fd, "  mobile sms <id> <n> <m>  - Send SMS\n");
	ast_cli(a->fd, "  mobile dtmf <id> <digits>- Send DTMF\n");
	ast_cli(a->fd, "\n");

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_search(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct adapter_pvt *adapter;
	inquiry_info *ii = NULL;
	int max_rsp, num_rsp;
	int len, flags;
	int i, phport, hsport;
	char addr[19] = {0};
	char name[31] = {0};

#define FORMAT1 "%-17.17s %-30.30s %-6.6s %-7.7s %-4.4s\n"
#define FORMAT2 "%-17.17s %-30.30s %-6.6s %-7.7s %d\n"

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile search";
		e->usage =
			"Usage: mobile search\n"
			"       Searches for Bluetooth Cell / Mobile devices in range.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 2)
		return CLI_SHOWUSAGE;

	/* find a free adapter */
	AST_RWLIST_RDLOCK(&adapters);
	AST_RWLIST_TRAVERSE(&adapters, adapter, entry) {
		if (!adapter->inuse)
			break;
	}
	AST_RWLIST_UNLOCK(&adapters);

	if (!adapter) {
		ast_cli(a->fd, "All Bluetooth adapters are in use at this time.\n");
		return CLI_SUCCESS;
	}

	len  = 8;
	max_rsp = 255;
	flags = IREQ_CACHE_FLUSH;

	ii = ast_alloca(max_rsp * sizeof(inquiry_info));
	num_rsp = hci_inquiry(adapter->dev_id, len, max_rsp, NULL, &ii, flags);
	if (num_rsp > 0) {
		ast_cli(a->fd, FORMAT1, "Address", "Name", "Usable", "Type", "Port");
		for (i = 0; i < num_rsp; i++) {
			ba2str(&(ii + i)->bdaddr, addr);
			name[0] = 0x00;
			if (hci_read_remote_name(adapter->hci_socket, &(ii + i)->bdaddr, sizeof(name) - 1, name, 0) < 0)
				strcpy(name, "[unknown]");
			phport = sdp_search(addr, HANDSFREE_AGW_PROFILE_ID);
			if (!phport)
				hsport = sdp_search(addr, HEADSET_PROFILE_ID);
			else
				hsport = 0;
			ast_cli(a->fd, FORMAT2, addr, name, (phport > 0 || hsport > 0) ? "Yes" : "No",
				(phport > 0) ? "Phone" : "Headset", (phport > 0) ? phport : hsport);
		}
	} else
		ast_cli(a->fd, "No Bluetooth Cell / Mobile devices found.\n");

#undef FORMAT1
#undef FORMAT2

	return CLI_SUCCESS;
}

static char *handle_cli_mobile_rfcomm(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	char buf[128];
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile rfcomm";
		e->usage =
			"Usage: mobile rfcomm <device ID> <command>\n"
			"       Send <command> to the rfcomm port on the device\n"
			"       with the specified <device ID>.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		goto e_return;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		goto e_unlock_pvt;
	}

	snprintf(buf, sizeof(buf), "%s\r", a->argv[3]);
	rfcomm_write(pvt->rfcomm_socket, buf);
	msg_queue_push(pvt, AT_OK, AT_UNKNOWN);

e_unlock_pvt:
	ast_mutex_unlock(&pvt->lock);
e_return:
	return CLI_SUCCESS;
}

static char *handle_cli_mobile_cusd(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	char buf[128];
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile cusd";
		e->usage =
			"Usage: mobile cusd <device ID> <command>\n"
			"       Send cusd <command> to the rfcomm port on the device\n"
			"       with the specified <device ID>.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		goto e_return;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		goto e_unlock_pvt;
	}

	snprintf(buf, sizeof(buf), "%s", a->argv[3]);
	if (hfp_send_cusd(pvt->hfp, buf) || msg_queue_push(pvt, AT_OK, AT_CUSD)) {
		ast_cli(a->fd, "[%s] error sending CUSD\n", pvt->id);
		goto e_unlock_pvt;
	}

e_unlock_pvt:
	ast_mutex_unlock(&pvt->lock);
e_return:
	return CLI_SUCCESS;
}

/*!
 * \brief CLI command: mobile dial <device> <number>
 * Dial a number using the specified mobile device
 */
static char *handle_cli_mobile_dial(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;
	char buf[128];

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile dial";
		e->usage =
			"Usage: mobile dial <device ID> <number>\n"
			"       Dial <number> on the mobile device with the specified <device ID>.\n"
			"       Example: mobile dial W350i +12025551234\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	if (pvt->owner) {
		ast_cli(a->fd, "Device %s is busy (already in a call).\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	if (!mbl_has_service(pvt)) {
		ast_cli(a->fd, "Device %s has no cellular service.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	/* Send ATD command to dial */
	snprintf(buf, sizeof(buf), "ATD%s;\r", a->argv[3]);
	if (rfcomm_write(pvt->rfcomm_socket, buf) < 0) {
		ast_cli(a->fd, "[%s] Error sending dial command.\n", pvt->id);
	} else {
		ast_cli(a->fd, "[%s] Dialing %s...\n", pvt->id, a->argv[3]);
		msg_queue_push(pvt, AT_OK, AT_D);
	}

	ast_mutex_unlock(&pvt->lock);
	return CLI_SUCCESS;
}

/*!
 * \brief CLI command: mobile answer <device>
 * Answer an incoming call on the specified mobile device
 */
static char *handle_cli_mobile_answer(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile answer";
		e->usage =
			"Usage: mobile answer <device ID>\n"
			"       Answer an incoming call on the mobile device with the specified <device ID>.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	if (!pvt->incoming) {
		ast_cli(a->fd, "Device %s has no incoming call to answer.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	/* Send ATA command to answer */
	if (rfcomm_write(pvt->rfcomm_socket, "ATA\r") < 0) {
		ast_cli(a->fd, "[%s] Error sending answer command.\n", pvt->id);
	} else {
		ast_cli(a->fd, "[%s] Answering incoming call...\n", pvt->id);
		msg_queue_push(pvt, AT_OK, AT_A);
	}

	ast_mutex_unlock(&pvt->lock);
	return CLI_SUCCESS;
}

/*!
 * \brief CLI command: mobile hangup <device>
 * Hang up a call on the specified mobile device
 */
static char *handle_cli_mobile_hangup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile hangup";
		e->usage =
			"Usage: mobile hangup <device ID>\n"
			"       Hang up the current call on the mobile device with the specified <device ID>.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	/* Send AT+CHUP command to hang up */
	if (hfp_send_chup(pvt->hfp) < 0) {
		ast_cli(a->fd, "[%s] Error sending hangup command.\n", pvt->id);
	} else {
		ast_cli(a->fd, "[%s] Hanging up call...\n", pvt->id);
		msg_queue_push(pvt, AT_OK, AT_CHUP);
	}

	ast_mutex_unlock(&pvt->lock);
	return CLI_SUCCESS;
}

/*!
 * \brief CLI command: mobile sms <device> <number> <message>
 * Send an SMS via the specified mobile device
 */
static char *handle_cli_mobile_sms(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;
	char *message;
	int i;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile sms";
		e->usage =
			"Usage: mobile sms <device ID> <number> <message>\n"
			"       Send an SMS to <number> via the mobile device with the specified <device ID>.\n"
			"       Example: mobile sms W350i +12025551234 Hello World\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 5)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	if (!pvt->has_sms) {
		ast_cli(a->fd, "Device %s does not support SMS.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	if (!mbl_has_service(pvt)) {
		ast_cli(a->fd, "Device %s has no cellular service.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	/* Build message from remaining arguments */
	message = ast_malloc(1024);
	if (!message) {
		ast_cli(a->fd, "Memory allocation error.\n");
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}
	message[0] = '\0';
	for (i = 4; i < a->argc; i++) {
		if (i > 4)
			strcat(message, " ");
		strncat(message, a->argv[i], 1023 - strlen(message));
	}

	/* Send SMS using HFP functions - message is queued with msg_queue_push_data */
	if (hfp_send_cmgs(pvt->hfp, a->argv[3]) < 0 ||
	    msg_queue_push_data(pvt, AT_SMS_PROMPT, AT_CMGS, message) != 0) {
		ast_cli(a->fd, "[%s] Error initiating SMS send.\n", pvt->id);
		ast_free(message);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	ast_cli(a->fd, "[%s] Sending SMS to %s: %s\n", pvt->id, a->argv[3], message);

	ast_mutex_unlock(&pvt->lock);
	return CLI_SUCCESS;
}

/*!
 * \brief CLI command: mobile dtmf <device> <digits>
 * Send DTMF tones on the specified mobile device
 */
static char *handle_cli_mobile_dtmf(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct mbl_pvt *pvt = NULL;
	const char *digits;
	char buf[32];
	int i;

	switch (cmd) {
	case CLI_INIT:
		e->command = "mobile dtmf";
		e->usage =
			"Usage: mobile dtmf <device ID> <digits>\n"
			"       Send DTMF <digits> on the mobile device with the specified <device ID>.\n"
			"       Example: mobile dtmf W350i 1234#\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	AST_RWLIST_RDLOCK(&devices);
	AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
		if (!strcmp(pvt->id, a->argv[2]))
			break;
	}
	AST_RWLIST_UNLOCK(&devices);

	if (!pvt) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_SUCCESS;
	}

	ast_mutex_lock(&pvt->lock);
	if (!pvt->connected) {
		ast_cli(a->fd, "Device %s not connected.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	if (!pvt->owner) {
		ast_cli(a->fd, "Device %s is not in a call.\n", a->argv[2]);
		ast_mutex_unlock(&pvt->lock);
		return CLI_SUCCESS;
	}

	digits = a->argv[3];
	ast_cli(a->fd, "[%s] Sending DTMF: %s\n", pvt->id, digits);

	/* Send each digit using AT+VTS */
	for (i = 0; digits[i] != '\0'; i++) {
		snprintf(buf, sizeof(buf), "AT+VTS=%c\r", digits[i]);
		if (rfcomm_write(pvt->rfcomm_socket, buf) < 0) {
			ast_cli(a->fd, "[%s] Error sending DTMF digit '%c'.\n", pvt->id, digits[i]);
			break;
		}
		msg_queue_push(pvt, AT_OK, AT_VTS);
		/* Small delay between digits */
		usleep(100000);  /* 100ms */
	}

	ast_mutex_unlock(&pvt->lock);
	return CLI_SUCCESS;
}
