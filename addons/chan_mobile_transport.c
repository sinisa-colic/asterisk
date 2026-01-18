/*
 * chan_mobile transport abstraction layer
 * 
 * Main transport initialization and selection
 */

#include "asterisk.h"
#include "chan_mobile_transport.h"

#include "asterisk/logger.h"

/* Global transport instance */
struct mbl_transport_ops *mbl_transport = NULL;

/* Current transport type */
static enum mbl_transport_type current_transport_type = MBL_TRANSPORT_BLUEZ;

int mbl_transport_init(enum mbl_transport_type type)
{
	int res;
	
	/* Cleanup any existing transport */
	if (mbl_transport && mbl_transport->cleanup) {
		mbl_transport->cleanup();
	}
	
	/* Select transport */
	switch (type) {
	case MBL_TRANSPORT_BLUEZ:
		mbl_transport = &mbl_transport_bluez;
		break;
	case MBL_TRANSPORT_MOCK:
		mbl_transport = &mbl_transport_mock;
		break;
	default:
		ast_log(LOG_ERROR, "chan_mobile: Unknown transport type %d\n", type);
		return -1;
	}
	
	current_transport_type = type;
	
	/* Initialize transport */
	if (mbl_transport->init) {
		res = mbl_transport->init();
		if (res < 0) {
			ast_log(LOG_ERROR, "chan_mobile: Failed to initialize %s transport\n", 
				mbl_transport->name);
			mbl_transport = NULL;
			return -1;
		}
	}
	
	ast_log(LOG_NOTICE, "chan_mobile: Using %s transport\n", mbl_transport->name);
	return 0;
}

void mbl_transport_cleanup(void)
{
	if (mbl_transport && mbl_transport->cleanup) {
		mbl_transport->cleanup();
	}
	mbl_transport = NULL;
}

enum mbl_transport_type mbl_transport_get_type(void)
{
	return current_transport_type;
}
