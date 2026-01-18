/*
 * chan_mobile transport abstraction layer
 * 
 * This provides an abstraction over Bluetooth RFCOMM and SCO sockets
 * to enable testing without real Bluetooth hardware.
 */

#ifndef CHAN_MOBILE_TRANSPORT_H
#define CHAN_MOBILE_TRANSPORT_H

#include <bluetooth/bluetooth.h>

/* Forward declarations */
struct mbl_transport;
struct mbl_transport_ops;

/*
 * Transport types
 */
enum mbl_transport_type {
	MBL_TRANSPORT_BLUEZ,    /* Real BlueZ Bluetooth stack */
	MBL_TRANSPORT_MOCK,     /* Mock transport for testing */
};

/*
 * Transport operations - vtable for different backends
 */
struct mbl_transport_ops {
	const char *name;
	
	/* RFCOMM operations */
	int (*rfcomm_connect)(bdaddr_t *src, bdaddr_t *dst, int channel);
	int (*rfcomm_write)(int fd, const char *buf, size_t len);
	ssize_t (*rfcomm_read)(int fd, char *buf, size_t len);
	int (*rfcomm_wait)(int fd, int *ms);
	void (*rfcomm_close)(int fd);
	
	/* SCO operations */
	int (*sco_connect)(bdaddr_t *src, bdaddr_t *dst);
	int (*sco_bind)(bdaddr_t *addr);
	int (*sco_accept)(int listen_fd, bdaddr_t *remote_addr);
	int (*sco_write)(int fd, const char *buf, int len);
	ssize_t (*sco_read)(int fd, char *buf, size_t len);
	void (*sco_close)(int fd);
	
	/* HCI operations */
	int (*hci_open)(int dev_id);
	void (*hci_close)(int fd);
	int (*hci_get_route)(bdaddr_t *addr);
	int (*hci_devinfo)(int dev_id, void *di);
	int (*hci_inquiry)(int dev_id, int len, int max_rsp, void *lap, void **ii, long flags);
	
	/* SDP operations */
	void *(*sdp_connect)(bdaddr_t *src, bdaddr_t *dst, uint32_t flags);
	int (*sdp_search)(void *session, void *search, void *attrid, void **rsp);
	void (*sdp_close)(void *session);
	
	/* Lifecycle */
	int (*init)(void);
	void (*cleanup)(void);
};

/*
 * Global transport instance
 */
extern struct mbl_transport_ops *mbl_transport;

/*
 * Initialize transport layer
 * Returns 0 on success, -1 on failure
 */
int mbl_transport_init(enum mbl_transport_type type);

/*
 * Cleanup transport layer
 */
void mbl_transport_cleanup(void);

/*
 * Get current transport type
 */
enum mbl_transport_type mbl_transport_get_type(void);

/*
 * BlueZ transport (real Bluetooth)
 */
extern struct mbl_transport_ops mbl_transport_bluez;

/*
 * Mock transport (for testing)
 */
extern struct mbl_transport_ops mbl_transport_mock;

/*
 * Mock transport control functions (for test harness)
 */
#ifdef MBL_ENABLE_MOCK_TRANSPORT

/* Inject data to be read from mock RFCOMM socket */
int mbl_mock_rfcomm_inject(int fd, const char *data, size_t len);

/* Get data written to mock RFCOMM socket */
ssize_t mbl_mock_rfcomm_drain(int fd, char *buf, size_t len);

/* Inject data to be read from mock SCO socket */
int mbl_mock_sco_inject(int fd, const char *data, size_t len);

/* Get data written to mock SCO socket */
ssize_t mbl_mock_sco_drain(int fd, char *buf, size_t len);

/* Simulate incoming SCO connection */
int mbl_mock_sco_incoming(int listen_fd, bdaddr_t *remote_addr);

/* Set mock device discovery results */
int mbl_mock_set_devices(int count, bdaddr_t *addrs, const char **names);

/* Reset all mock state */
void mbl_mock_reset(void);

#endif /* MBL_ENABLE_MOCK_TRANSPORT */

#endif /* CHAN_MOBILE_TRANSPORT_H */
