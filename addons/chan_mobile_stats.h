/*
 * chan_mobile_stats.h - Statistics tracking for chan_mobile
 *
 * This header is included by chan_mobile.c after structure definitions.
 */

#ifndef CHAN_MOBILE_STATS_H
#define CHAN_MOBILE_STATS_H

/* Forward declarations - structures defined in chan_mobile.c */
struct mbl_pvt;

/*
 * Statistics update functions
 */

/* Record incoming call */
void mbl_stats_call_in(struct mbl_pvt *pvt);

/* Record outgoing call */
void mbl_stats_call_out(struct mbl_pvt *pvt);

/* Record call answered */
void mbl_stats_call_answered(struct mbl_pvt *pvt);

/* Record call ended with duration */
void mbl_stats_call_ended(struct mbl_pvt *pvt, int duration);

/* Record SMS received */
void mbl_stats_sms_in(struct mbl_pvt *pvt);

/* Record SMS sent */
void mbl_stats_sms_out(struct mbl_pvt *pvt);

/* Record audio error */
void mbl_stats_audio_error(struct mbl_pvt *pvt);

/* Record device connected */
void mbl_stats_device_connected(struct mbl_pvt *pvt);

/* Record device disconnected */
void mbl_stats_device_disconnected(struct mbl_pvt *pvt, const char *reason);

/* Record connection failure */
void mbl_stats_connect_failed(struct mbl_pvt *pvt);

/*
 * Statistics reset functions
 */

/* Reset all statistics for a device */
void mbl_stats_reset(struct mbl_pvt *pvt);

/* Reset call statistics only */
void mbl_stats_reset_calls(struct mbl_pvt *pvt);

/* Reset SMS statistics only */
void mbl_stats_reset_sms(struct mbl_pvt *pvt);

/* Reset connection statistics only */
void mbl_stats_reset_connection(struct mbl_pvt *pvt);

/*
 * Statistics query functions
 */

/* Get total call time in seconds */
unsigned long mbl_stats_get_total_call_time(struct mbl_pvt *pvt);

/* Get connection uptime in seconds (0 if not connected) */
int mbl_stats_get_uptime(struct mbl_pvt *pvt);

/* Format uptime as HH:MM:SS string */
void mbl_stats_format_uptime(struct mbl_pvt *pvt, char *buf, size_t buflen);

/* Format total call time as HH:MM:SS string */
void mbl_stats_format_call_time(struct mbl_pvt *pvt, char *buf, size_t buflen);

#endif /* CHAN_MOBILE_STATS_H */
