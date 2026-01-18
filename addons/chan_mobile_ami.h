/*
 * chan_mobile_ami.h - AMI event helpers for chan_mobile
 *
 * This header is included by chan_mobile.c after structure definitions.
 */

#ifndef CHAN_MOBILE_AMI_H
#define CHAN_MOBILE_AMI_H

/* Forward declarations - structures defined in chan_mobile.c */
struct mbl_pvt;

/*
 * Device status events
 */

/* Device connected */
void mbl_ami_device_connect(struct mbl_pvt *pvt, const char *address);

/* Device disconnected */
void mbl_ami_device_disconnect(struct mbl_pvt *pvt, const char *reason);

/* Device initialized (HFP SLC complete) */
void mbl_ami_device_initialized(struct mbl_pvt *pvt);

/* Connection failed */
void mbl_ami_connect_failed(struct mbl_pvt *pvt, const char *reason);

/*
 * Indicator events
 */

/* Signal strength changed */
void mbl_ami_signal(struct mbl_pvt *pvt, int signal);

/* Battery level changed */
void mbl_ami_battery(struct mbl_pvt *pvt, int battery);

/* Service status changed */
void mbl_ami_service(struct mbl_pvt *pvt, int service);

/* Roaming status changed */
void mbl_ami_roaming(struct mbl_pvt *pvt, int roaming);

/*
 * Call events
 */

/* Incoming call detected (waiting for caller ID) */
void mbl_ami_call_incoming_wait(struct mbl_pvt *pvt);

/* Incoming call with caller ID */
void mbl_ami_call_incoming(struct mbl_pvt *pvt, const char *caller_id, const char *caller_name);

/* Outgoing call initiated */
void mbl_ami_call_outgoing(struct mbl_pvt *pvt, const char *destination);

/* Call started (answered) */
void mbl_ami_call_start(struct mbl_pvt *pvt, int is_incoming);

/* Call ended */
void mbl_ami_call_end(struct mbl_pvt *pvt, int was_incoming, int was_outgoing, 
                      int was_answered, int duration, const char *caller_id,
                      const char *dialed_number, const char *previous_state);

/* Audio error */
void mbl_ami_audio_error(struct mbl_pvt *pvt, const char *error_msg);

/*
 * SMS events
 */

/* SMS received */
void mbl_ami_sms_received(struct mbl_pvt *pvt, const char *from, const char *text);

/* SMS sent */
void mbl_ami_sms_sent(struct mbl_pvt *pvt, const char *to);

#endif /* CHAN_MOBILE_AMI_H */
