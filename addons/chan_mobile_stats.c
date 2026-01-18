/*
 * chan_mobile_stats.c - Statistics tracking for chan_mobile
 *
 * This module provides functions for tracking and managing
 * statistics for mobile devices.
 *
 * This file is included by chan_mobile.c, not compiled separately.
 */

/* Note: This file is #included from chan_mobile.c after all headers */

/*
 * Statistics update functions
 */

void mbl_stats_call_in(struct mbl_pvt *pvt)
{
	pvt->calls_in++;
}

void mbl_stats_call_out(struct mbl_pvt *pvt)
{
	pvt->calls_out++;
}

void mbl_stats_call_answered(struct mbl_pvt *pvt)
{
	pvt->calls_answered++;
	pvt->call_start_time = time(NULL);
}

void mbl_stats_call_ended(struct mbl_pvt *pvt, int duration)
{
	if (duration > 0) {
		pvt->total_call_seconds += duration;
	}
	pvt->call_start_time = 0;
	pvt->audio_errors = 0;
}

void mbl_stats_sms_in(struct mbl_pvt *pvt)
{
	pvt->sms_in++;
}

void mbl_stats_sms_out(struct mbl_pvt *pvt)
{
	pvt->sms_out++;
}

void mbl_stats_audio_error(struct mbl_pvt *pvt)
{
	pvt->audio_errors++;
}

void mbl_stats_device_connected(struct mbl_pvt *pvt)
{
	pvt->connected_since = time(NULL);
	pvt->connect_failures = 0;
	pvt->backoff_seconds = 0;
}

void mbl_stats_device_disconnected(struct mbl_pvt *pvt, const char *reason)
{
	pvt->last_disconnect_time = time(NULL);
	if (reason) {
		ast_copy_string(pvt->last_disconnect_reason, reason, 
			sizeof(pvt->last_disconnect_reason));
	}
	pvt->connected_since = 0;
}

void mbl_stats_connect_failed(struct mbl_pvt *pvt)
{
	pvt->connect_failures++;
	pvt->last_connect_attempt = time(NULL);
	
	/* Apply exponential backoff */
	if (pvt->backoff_seconds == 0) {
		pvt->backoff_seconds = 5; /* Start with 5 seconds */
	} else {
		pvt->backoff_seconds *= 2;
		if (pvt->backoff_seconds > 300) {
			pvt->backoff_seconds = 300; /* Max 5 minutes */
		}
	}
}

/*
 * Statistics reset functions
 */

void mbl_stats_reset(struct mbl_pvt *pvt)
{
	mbl_stats_reset_calls(pvt);
	mbl_stats_reset_sms(pvt);
	mbl_stats_reset_connection(pvt);
}

void mbl_stats_reset_calls(struct mbl_pvt *pvt)
{
	pvt->calls_in = 0;
	pvt->calls_out = 0;
	pvt->calls_answered = 0;
	pvt->total_call_seconds = 0;
	pvt->audio_errors = 0;
}

void mbl_stats_reset_sms(struct mbl_pvt *pvt)
{
	pvt->sms_in = 0;
	pvt->sms_out = 0;
}

void mbl_stats_reset_connection(struct mbl_pvt *pvt)
{
	pvt->connect_failures = 0;
	pvt->backoff_seconds = 0;
	pvt->last_connect_attempt = 0;
	pvt->last_disconnect_time = 0;
	pvt->last_disconnect_reason[0] = '\0';
}

/*
 * Statistics query functions
 */

unsigned long mbl_stats_get_total_call_time(struct mbl_pvt *pvt)
{
	unsigned long total = pvt->total_call_seconds;
	
	/* Add current call duration if in a call */
	if (pvt->call_start_time > 0) {
		total += (unsigned long)(time(NULL) - pvt->call_start_time);
	}
	
	return total;
}

int mbl_stats_get_uptime(struct mbl_pvt *pvt)
{
	if (!pvt->connected || pvt->connected_since == 0) {
		return 0;
	}
	return (int)(time(NULL) - pvt->connected_since);
}

void mbl_stats_format_uptime(struct mbl_pvt *pvt, char *buf, size_t buflen)
{
	int uptime = mbl_stats_get_uptime(pvt);
	
	if (uptime <= 0) {
		snprintf(buf, buflen, "N/A");
	} else {
		int hours = uptime / 3600;
		int mins = (uptime % 3600) / 60;
		int secs = uptime % 60;
		snprintf(buf, buflen, "%02d:%02d:%02d", hours, mins, secs);
	}
}

void mbl_stats_format_call_time(struct mbl_pvt *pvt, char *buf, size_t buflen)
{
	unsigned long total = mbl_stats_get_total_call_time(pvt);
	int hours = total / 3600;
	int mins = (total % 3600) / 60;
	int secs = total % 60;
	snprintf(buf, buflen, "%02d:%02d:%02d", hours, mins, secs);
}
