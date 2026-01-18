/*
 * chan_mobile BlueZ transport implementation
 * 
 * Real Bluetooth transport using Linux BlueZ stack
 */

#include "asterisk.h"
#include "chan_mobile_transport.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sco.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include "asterisk/logger.h"

/*
 * RFCOMM operations
 */
static int bluez_rfcomm_connect(bdaddr_t *src, bdaddr_t *dst, int channel)
{
	struct sockaddr_rc addr;
	int s;

	if ((s = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0) {
		ast_log(LOG_DEBUG, "bluez: rfcomm socket() failed (%d: %s)\n", errno, strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.rc_family = AF_BLUETOOTH;
	bacpy(&addr.rc_bdaddr, src);
	addr.rc_channel = 0;
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ast_log(LOG_DEBUG, "bluez: rfcomm bind() failed (%d: %s)\n", errno, strerror(errno));
		close(s);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.rc_family = AF_BLUETOOTH;
	bacpy(&addr.rc_bdaddr, dst);
	addr.rc_channel = channel;
	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ast_log(LOG_DEBUG, "bluez: rfcomm connect() failed (%d: %s)\n", errno, strerror(errno));
		close(s);
		return -1;
	}

	return s;
}

static int bluez_rfcomm_write(int fd, const char *buf, size_t len)
{
	const char *p = buf;
	ssize_t out_count;

	while (len > 0) {
		if ((out_count = write(fd, p, len)) == -1) {
			if (errno == EINTR)
				continue;
			ast_log(LOG_DEBUG, "bluez: rfcomm write() error (%d: %s)\n", errno, strerror(errno));
			return -1;
		}
		len -= out_count;
		p += out_count;
	}

	return 0;
}

static ssize_t bluez_rfcomm_read(int fd, char *buf, size_t len)
{
	ssize_t r;
	
	do {
		r = read(fd, buf, len);
	} while (r == -1 && errno == EINTR);
	
	return r;
}

static int bluez_rfcomm_wait(int fd, int *ms)
{
	int exception, outfd;
	outfd = ast_waitfor_n_fd(&fd, 1, ms, &exception);
	if (outfd < 0)
		outfd = 0;
	return outfd;
}

static void bluez_rfcomm_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

/*
 * SCO operations
 */
static int bluez_sco_connect(bdaddr_t *src, bdaddr_t *dst)
{
	struct sockaddr_sco addr;
	int s;

	if ((s = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO)) < 0) {
		ast_log(LOG_DEBUG, "bluez: sco socket() failed (%d: %s)\n", errno, strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sco_family = AF_BLUETOOTH;
	bacpy(&addr.sco_bdaddr, dst);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ast_log(LOG_DEBUG, "bluez: sco connect() failed (%d: %s)\n", errno, strerror(errno));
		close(s);
		return -1;
	}

	return s;
}

static int bluez_sco_bind(bdaddr_t *addr)
{
	struct sockaddr_sco sco_addr;
	int s;

	if ((s = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO)) < 0) {
		ast_log(LOG_DEBUG, "bluez: sco socket() failed (%d: %s)\n", errno, strerror(errno));
		return -1;
	}

	memset(&sco_addr, 0, sizeof(sco_addr));
	sco_addr.sco_family = AF_BLUETOOTH;
	bacpy(&sco_addr.sco_bdaddr, addr);

	if (bind(s, (struct sockaddr *)&sco_addr, sizeof(sco_addr)) < 0) {
		ast_log(LOG_DEBUG, "bluez: sco bind() failed (%d: %s)\n", errno, strerror(errno));
		close(s);
		return -1;
	}

	if (listen(s, 5) < 0) {
		ast_log(LOG_DEBUG, "bluez: sco listen() failed (%d: %s)\n", errno, strerror(errno));
		close(s);
		return -1;
	}

	return s;
}

static int bluez_sco_accept(int listen_fd, bdaddr_t *remote_addr)
{
	struct sockaddr_sco addr;
	socklen_t addrlen = sizeof(addr);
	int sock;

	sock = accept(listen_fd, (struct sockaddr *)&addr, &addrlen);
	if (sock >= 0 && remote_addr) {
		bacpy(remote_addr, &addr.sco_bdaddr);
	}
	return sock;
}

static int bluez_sco_write(int fd, const char *buf, int len)
{
	int r;

	if (fd == -1)
		return 0;

	r = write(fd, buf, len);
	if (r == -1) {
		ast_log(LOG_DEBUG, "bluez: sco write() error (%d: %s)\n", errno, strerror(errno));
		return 0;
	}

	return 1;
}

static ssize_t bluez_sco_read(int fd, char *buf, size_t len)
{
	return read(fd, buf, len);
}

static void bluez_sco_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

/*
 * HCI operations
 */
static int bluez_hci_open(int dev_id)
{
	return hci_open_dev(dev_id);
}

static void bluez_hci_close(int fd)
{
	hci_close_dev(fd);
}

static int bluez_hci_get_route(bdaddr_t *addr)
{
	return hci_get_route(addr);
}

static int bluez_hci_devinfo(int dev_id, void *di)
{
	return hci_devinfo(dev_id, (struct hci_dev_info *)di);
}

static int bluez_hci_inquiry(int dev_id, int len, int max_rsp, void *lap, void **ii, long flags)
{
	return hci_inquiry(dev_id, len, max_rsp, (const uint8_t *)lap, (inquiry_info **)ii, flags);
}

/*
 * SDP operations
 */
static void *bluez_sdp_connect(bdaddr_t *src, bdaddr_t *dst, uint32_t flags)
{
	return sdp_connect(src, dst, flags);
}

static int bluez_sdp_search(void *session, void *search, void *attrid, void **rsp)
{
	return sdp_service_search_attr_req((sdp_session_t *)session, 
		(sdp_list_t *)search, SDP_ATTR_REQ_RANGE, (sdp_list_t *)attrid, (sdp_list_t **)rsp);
}

static void bluez_sdp_close(void *session)
{
	sdp_close((sdp_session_t *)session);
}

/*
 * Lifecycle
 */
static int bluez_init(void)
{
	ast_log(LOG_NOTICE, "chan_mobile: BlueZ transport initialized\n");
	return 0;
}

static void bluez_cleanup(void)
{
	ast_log(LOG_NOTICE, "chan_mobile: BlueZ transport cleanup\n");
}

/*
 * BlueZ transport operations table
 */
struct mbl_transport_ops mbl_transport_bluez = {
	.name = "bluez",
	
	.rfcomm_connect = bluez_rfcomm_connect,
	.rfcomm_write = bluez_rfcomm_write,
	.rfcomm_read = bluez_rfcomm_read,
	.rfcomm_wait = bluez_rfcomm_wait,
	.rfcomm_close = bluez_rfcomm_close,
	
	.sco_connect = bluez_sco_connect,
	.sco_bind = bluez_sco_bind,
	.sco_accept = bluez_sco_accept,
	.sco_write = bluez_sco_write,
	.sco_read = bluez_sco_read,
	.sco_close = bluez_sco_close,
	
	.hci_open = bluez_hci_open,
	.hci_close = bluez_hci_close,
	.hci_get_route = bluez_hci_get_route,
	.hci_devinfo = bluez_hci_devinfo,
	.hci_inquiry = bluez_hci_inquiry,
	
	.sdp_connect = bluez_sdp_connect,
	.sdp_search = bluez_sdp_search,
	.sdp_close = bluez_sdp_close,
	
	.init = bluez_init,
	.cleanup = bluez_cleanup,
};
