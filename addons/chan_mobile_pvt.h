/*
 * chan_mobile_pvt.h - Private structures and types for chan_mobile
 *
 * This header defines the core data structures shared across chan_mobile modules.
 */

#ifndef CHAN_MOBILE_PVT_H
#define CHAN_MOBILE_PVT_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sco.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "asterisk.h"
#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/linkedlists.h"
#include "asterisk/io.h"
#include "asterisk/smoother.h"
#include "asterisk/dsp.h"
#include "asterisk/sched.h"

/* Frame sizes for audio */
#define DEVICE_FRAME_SIZE 48
#define CHANNEL_FRAME_SIZE 320

/* Device types */
enum mbl_type {
	MBL_TYPE_PHONE,
	MBL_TYPE_HEADSET
};

/* HFP Service Level Connection states */
enum hfp_slc_state {
	HFP_SLC_DISCONNECTED = 0,
	HFP_SLC_CONNECTING,
	HFP_SLC_BRSF_SENT,
	HFP_SLC_CIND_TEST_SENT,
	HFP_SLC_CIND_SENT,
	HFP_SLC_CMER_SENT,
	HFP_SLC_CLIP_SENT,
	HFP_SLC_ECAM_SENT,
	HFP_SLC_VGS_SENT,
	HFP_SLC_CMGF_SENT,
	HFP_SLC_CNMI_SENT,
	HFP_SLC_CONNECTED,
};

/* Call states */
enum mbl_call_state {
	MBL_CALL_IDLE = 0,		/*!< no call in progress */
	MBL_CALL_INCOMING_RING,		/*!< incoming call ringing */
	MBL_CALL_INCOMING_WAIT_CID,	/*!< waiting for caller ID */
	MBL_CALL_OUTGOING_DIALING,	/*!< outgoing call dialing */
	MBL_CALL_OUTGOING_ALERTING,	/*!< remote party ringing */
	MBL_CALL_ACTIVE,		/*!< call connected/active */
	MBL_CALL_HOLD,			/*!< call on hold */
	MBL_CALL_WAITING,		/*!< call waiting */
	MBL_CALL_HANGUP_PENDING,	/*!< hangup in progress */
};

/* Forward declarations */
struct mbl_pvt;
struct adapter_pvt;
struct hfp_pvt;

/* Caller ID info */
struct cidinfo {
	char *cnum;
	char *cnam;
};

/* HFP CIND indicator mapping */
struct hfp_cind {
	int service;
	int call;
	int callsetup;
	int callheld;
	int signal;
	int roam;
	int battchg;
};

/* HFP AG (Audio Gateway) features */
struct hfp_ag {
	unsigned int cw:1;
	unsigned int ecnr:1;
	unsigned int voice:1;
	unsigned int ring:1;
	unsigned int tag:1;
	unsigned int reject:1;
	unsigned int status:1;
	unsigned int control:1;
	unsigned int errors:1;
};

/* HFP HF (Hands-Free) features */
struct hfp_hf {
	unsigned int ecnr:1;
	unsigned int cw:1;
	unsigned int cid:1;
	unsigned int voice:1;
	unsigned int volume:1;
	unsigned int status:1;
	unsigned int control:1;
};

/* HFP private data */
struct hfp_pvt {
	struct mbl_pvt *owner;		/*!< the mbl_pvt struct that owns this struct */
	int initialized:1;		/*!< whether the hfp connection is initialized */
	int sent_alerting:1;		/*!< have we sent alerting? */
	struct hfp_ag brsf;		/*!< the supported feature set of the AG */
	int cind_index[16];		/*!< the cind/ciev index to name mapping for this AG */
	int cind_state[16];		/*!< the cind/ciev state for this AG */
	struct hfp_cind cind_map;	/*!< the cind name to index mapping for this AG */
	int rsock;			/*!< our rfcomm socket */
	int rport;			/*!< our rfcomm port */
	enum hfp_slc_state slc_state;	/*!< HFP Service Level Connection state */
};

/* Bluetooth adapter private data */
struct adapter_pvt {
	int dev_id;					/* device id */
	int hci_socket;					/* device descriptor */
	char id[31];					/* the 'name' from mobile.conf */
	bdaddr_t addr;					/* adapter address */
	unsigned int inuse:1;				/* are we in use ? */
	unsigned int alignment_detection:1;		/* do alignment detection on this adapter? */
	struct io_context *accept_io;			/* roles accept io context */
	struct io_context *io;				/* roles io context */
	int sco_socket;					/* roles sco listening socket */
	int *sco_id;					/*!< io context id for sco_socket (NULL if not registered) */
	pthread_t sco_listener_thread;			/* roles sco listener thread */
	ast_mutex_t lock;				/*!< lock for thread-safe io_context operations */
	AST_LIST_ENTRY(adapter_pvt) entry;
};

/* Message queue entry */
struct msg_queue_entry {
	int expected;
	int response_to;
	void *data;
	AST_LIST_ENTRY(msg_queue_entry) entry;
};

/* Mobile device private data */
struct mbl_pvt {
	struct ast_channel *owner;			/* Channel we belong to, possibly NULL */
	struct ast_frame fr;				/* "null" frame */
	ast_mutex_t lock;				/*!< pvt lock */
	AST_LIST_HEAD_NOLOCK(msg_queue, msg_queue_entry) msg_queue;
	enum mbl_type type;				/* Phone or Headset */
	char id[31];					/* The id from mobile.conf */
	int group;					/* group number for group dialling */
	bdaddr_t addr;					/* address of device */
	struct adapter_pvt *adapter;			/* the adapter we use */
	char context[AST_MAX_CONTEXT];			/* the context for incoming calls */
	struct hfp_pvt *hfp;				/*!< hfp pvt */
	int rfcomm_port;				/* rfcomm port number */
	int rfcomm_socket;				/* rfcomm socket descriptor */
	char rfcomm_buf[256];
	char io_buf[CHANNEL_FRAME_SIZE + AST_FRIENDLY_OFFSET];
	struct ast_smoother *bt_out_smoother;		/* our bt_out_smoother, for making 48 byte frames */
	struct ast_smoother *bt_in_smoother;		/* our smoother, for making "normal" CHANNEL_FRAME_SIZEed byte frames */
	int sco_socket;					/* sco socket descriptor */
	int *sco_io_id;					/*!< io context id for sco_socket (NULL if not registered) */
	pthread_t monitor_thread;			/* monitor thread handle */
	int timeout;					/*!< used to set the timeout for rfcomm data (may be used in the future) */
	unsigned int no_callsetup:1;
	unsigned int has_sms:1;
	unsigned int do_alignment_detection:1;
	unsigned int alignment_detection_triggered:1;
	unsigned int blackberry:1;
	short alignment_samples[4];
	int alignment_count;
	int ring_sched_id;
	struct ast_dsp *dsp;
	struct ast_sched_context *sched;
	int hangupcause;

	/* flags */
	unsigned int outgoing:1;	/*!< outgoing call */
	unsigned int incoming:1;	/*!< incoming call */
	unsigned int outgoing_sms:1;	/*!< outgoing sms */
	unsigned int incoming_sms:1;	/*!< outgoing sms */
	unsigned int needcallerid:1;	/*!< we need callerid */
	unsigned int needchup:1;	/*!< we need to send a chup */
	unsigned int needring:1;	/*!< we need to send a RING */
	unsigned int answered:1;	/*!< we sent/received an answer */
	unsigned int connected:1;	/*!< do we have an rfcomm connection to a device */

	/* call state tracking */
	enum mbl_call_state call_state;
	char caller_id[64];		/*!< caller ID for current/last call */
	char dialed_number[64];		/*!< dialed number for outgoing calls */
	int hangup_cause;		/*!< hangup cause code */
	unsigned int audio_errors;	/*!< count of audio read/write errors */

	/* connection retry state */
	int connect_failures;		/*!< consecutive connection failures */
	time_t last_connect_attempt;	/*!< timestamp of last connection attempt */
	int backoff_seconds;		/*!< current backoff interval */
	
	/* disconnect tracking */
	char last_disconnect_reason[64];	/*!< reason for last disconnect */
	time_t last_disconnect_time;		/*!< timestamp of last disconnect */

	/* statistics */
	unsigned int calls_in;			/*!< incoming calls received */
	unsigned int calls_out;			/*!< outgoing calls made */
	unsigned int calls_answered;		/*!< calls answered */
	unsigned int sms_in;			/*!< SMS messages received */
	unsigned int sms_out;			/*!< SMS messages sent */
	time_t connected_since;			/*!< timestamp when device connected */
	time_t call_start_time;			/*!< timestamp when current call started */
	unsigned long total_call_seconds;	/*!< total call duration in seconds */

	AST_LIST_ENTRY(mbl_pvt) entry;
};

/* Helper function declarations */
const char *hfp_slc_state_str(enum hfp_slc_state state);
const char *mbl_call_state_str(enum mbl_call_state state);
void mbl_set_call_state(struct mbl_pvt *pvt, enum mbl_call_state new_state);
int mbl_has_service(struct mbl_pvt *pvt);

/* List declarations - defined in chan_mobile.c */
AST_RWLIST_HEAD(mbl_pvt_list, mbl_pvt);
AST_RWLIST_HEAD(adapter_pvt_list, adapter_pvt);

extern struct mbl_pvt_list devices;
extern struct adapter_pvt_list adapters;

#endif /* CHAN_MOBILE_PVT_H */
