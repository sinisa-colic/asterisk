/*
 * chan_mobile Mock transport implementation
 * 
 * Fake Bluetooth transport for testing without hardware
 */

#include "asterisk.h"

#define MBL_ENABLE_MOCK_TRANSPORT
#include "chan_mobile_transport.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "asterisk/logger.h"
#include "asterisk/lock.h"

/* Maximum mock sockets */
#define MOCK_MAX_FDS 64
#define MOCK_BUFFER_SIZE 4096

/* Mock socket types */
enum mock_socket_type {
	MOCK_SOCKET_NONE = 0,
	MOCK_SOCKET_RFCOMM,
	MOCK_SOCKET_SCO,
	MOCK_SOCKET_SCO_LISTEN,
	MOCK_SOCKET_HCI,
};

/* Mock socket state */
struct mock_socket {
	enum mock_socket_type type;
	int connected;
	bdaddr_t local_addr;
	bdaddr_t remote_addr;
	int channel;  /* RFCOMM channel */
	
	/* Read buffer (data injected by test harness) */
	char read_buf[MOCK_BUFFER_SIZE];
	size_t read_head;
	size_t read_tail;
	
	/* Write buffer (data written by chan_mobile) */
	char write_buf[MOCK_BUFFER_SIZE];
	size_t write_head;
	size_t write_tail;
	
	/* Pending incoming connection (for SCO listen sockets) */
	int pending_incoming;
	bdaddr_t pending_addr;
};

/* Global mock state */
static struct {
	ast_mutex_t lock;
	struct mock_socket sockets[MOCK_MAX_FDS];
	int next_fd;
	int initialized;
	
	/* Mock device discovery results */
	int num_devices;
	bdaddr_t device_addrs[16];
	char device_names[16][248];
} mock_state;

/* Allocate a mock FD */
static int mock_alloc_fd(enum mock_socket_type type)
{
	int fd;
	
	ast_mutex_lock(&mock_state.lock);
	
	for (fd = 0; fd < MOCK_MAX_FDS; fd++) {
		if (mock_state.sockets[fd].type == MOCK_SOCKET_NONE) {
			memset(&mock_state.sockets[fd], 0, sizeof(struct mock_socket));
			mock_state.sockets[fd].type = type;
			ast_mutex_unlock(&mock_state.lock);
			return fd + 1000;  /* Offset to avoid collision with real FDs */
		}
	}
	
	ast_mutex_unlock(&mock_state.lock);
	errno = EMFILE;
	return -1;
}

/* Get mock socket from FD */
static struct mock_socket *mock_get_socket(int fd)
{
	int idx = fd - 1000;
	if (idx < 0 || idx >= MOCK_MAX_FDS)
		return NULL;
	if (mock_state.sockets[idx].type == MOCK_SOCKET_NONE)
		return NULL;
	return &mock_state.sockets[idx];
}

/* Free a mock FD */
static void mock_free_fd(int fd)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (sock) {
		ast_mutex_lock(&mock_state.lock);
		sock->type = MOCK_SOCKET_NONE;
		ast_mutex_unlock(&mock_state.lock);
	}
}

/*
 * RFCOMM operations
 */
static int mock_rfcomm_connect(bdaddr_t *src, bdaddr_t *dst, int channel)
{
	int fd = mock_alloc_fd(MOCK_SOCKET_RFCOMM);
	if (fd < 0)
		return -1;
	
	struct mock_socket *sock = mock_get_socket(fd);
	if (sock) {
		bacpy(&sock->local_addr, src);
		bacpy(&sock->remote_addr, dst);
		sock->channel = channel;
		sock->connected = 1;
	}
	
	ast_log(LOG_DEBUG, "mock: rfcomm_connect fd=%d channel=%d\n", fd, channel);
	return fd;
}

static int mock_rfcomm_write(int fd, const char *buf, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_RFCOMM) {
		errno = EBADF;
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	/* Store in write buffer for test harness to read */
	size_t space = MOCK_BUFFER_SIZE - sock->write_tail;
	if (len > space)
		len = space;
	
	memcpy(sock->write_buf + sock->write_tail, buf, len);
	sock->write_tail += len;
	
	ast_mutex_unlock(&mock_state.lock);
	
	ast_log(LOG_DEBUG, "mock: rfcomm_write fd=%d len=%zu\n", fd, len);
	return 0;
}

static ssize_t mock_rfcomm_read(int fd, char *buf, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_RFCOMM) {
		errno = EBADF;
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t avail = sock->read_tail - sock->read_head;
	if (avail == 0) {
		ast_mutex_unlock(&mock_state.lock);
		errno = EAGAIN;
		return -1;
	}
	
	if (len > avail)
		len = avail;
	
	memcpy(buf, sock->read_buf + sock->read_head, len);
	sock->read_head += len;
	
	/* Compact buffer if empty */
	if (sock->read_head == sock->read_tail) {
		sock->read_head = 0;
		sock->read_tail = 0;
	}
	
	ast_mutex_unlock(&mock_state.lock);
	
	ast_log(LOG_DEBUG, "mock: rfcomm_read fd=%d len=%zu\n", fd, len);
	return len;
}

static int mock_rfcomm_wait(int fd, int *ms)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_RFCOMM) {
		return 0;
	}
	
	/* Check if data available */
	ast_mutex_lock(&mock_state.lock);
	int has_data = (sock->read_tail > sock->read_head);
	ast_mutex_unlock(&mock_state.lock);
	
	if (has_data) {
		return fd;
	}
	
	/* Simulate timeout */
	if (ms && *ms > 0) {
		usleep((*ms) * 1000);
		*ms = 0;
	}
	
	return 0;
}

static void mock_rfcomm_close(int fd)
{
	ast_log(LOG_DEBUG, "mock: rfcomm_close fd=%d\n", fd);
	mock_free_fd(fd);
}

/*
 * SCO operations
 */
static int mock_sco_connect(bdaddr_t *src, bdaddr_t *dst)
{
	int fd = mock_alloc_fd(MOCK_SOCKET_SCO);
	if (fd < 0)
		return -1;
	
	struct mock_socket *sock = mock_get_socket(fd);
	if (sock) {
		bacpy(&sock->local_addr, src);
		bacpy(&sock->remote_addr, dst);
		sock->connected = 1;
	}
	
	ast_log(LOG_DEBUG, "mock: sco_connect fd=%d\n", fd);
	return fd;
}

static int mock_sco_bind(bdaddr_t *addr)
{
	int fd = mock_alloc_fd(MOCK_SOCKET_SCO_LISTEN);
	if (fd < 0)
		return -1;
	
	struct mock_socket *sock = mock_get_socket(fd);
	if (sock) {
		bacpy(&sock->local_addr, addr);
	}
	
	ast_log(LOG_DEBUG, "mock: sco_bind fd=%d\n", fd);
	return fd;
}

static int mock_sco_accept(int listen_fd, bdaddr_t *remote_addr)
{
	struct mock_socket *listen_sock = mock_get_socket(listen_fd);
	if (!listen_sock || listen_sock->type != MOCK_SOCKET_SCO_LISTEN) {
		errno = EBADF;
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	if (!listen_sock->pending_incoming) {
		ast_mutex_unlock(&mock_state.lock);
		errno = EAGAIN;
		return -1;
	}
	
	/* Accept the pending connection */
	int fd = mock_alloc_fd(MOCK_SOCKET_SCO);
	if (fd < 0) {
		ast_mutex_unlock(&mock_state.lock);
		return -1;
	}
	
	struct mock_socket *sock = mock_get_socket(fd);
	if (sock) {
		bacpy(&sock->local_addr, &listen_sock->local_addr);
		bacpy(&sock->remote_addr, &listen_sock->pending_addr);
		sock->connected = 1;
		
		if (remote_addr) {
			bacpy(remote_addr, &listen_sock->pending_addr);
		}
	}
	
	listen_sock->pending_incoming = 0;
	
	ast_mutex_unlock(&mock_state.lock);
	
	ast_log(LOG_DEBUG, "mock: sco_accept listen_fd=%d new_fd=%d\n", listen_fd, fd);
	return fd;
}

static int mock_sco_write(int fd, const char *buf, int len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_SCO) {
		return 0;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t space = MOCK_BUFFER_SIZE - sock->write_tail;
	if ((size_t)len > space)
		len = space;
	
	memcpy(sock->write_buf + sock->write_tail, buf, len);
	sock->write_tail += len;
	
	ast_mutex_unlock(&mock_state.lock);
	
	return 1;
}

static ssize_t mock_sco_read(int fd, char *buf, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_SCO) {
		errno = EBADF;
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t avail = sock->read_tail - sock->read_head;
	if (avail == 0) {
		ast_mutex_unlock(&mock_state.lock);
		errno = EAGAIN;
		return -1;
	}
	
	if (len > avail)
		len = avail;
	
	memcpy(buf, sock->read_buf + sock->read_head, len);
	sock->read_head += len;
	
	if (sock->read_head == sock->read_tail) {
		sock->read_head = 0;
		sock->read_tail = 0;
	}
	
	ast_mutex_unlock(&mock_state.lock);
	
	return len;
}

static void mock_sco_close(int fd)
{
	ast_log(LOG_DEBUG, "mock: sco_close fd=%d\n", fd);
	mock_free_fd(fd);
}

/*
 * HCI operations
 */
static int mock_hci_open(int dev_id)
{
	int fd = mock_alloc_fd(MOCK_SOCKET_HCI);
	ast_log(LOG_DEBUG, "mock: hci_open dev_id=%d fd=%d\n", dev_id, fd);
	return fd;
}

static void mock_hci_close(int fd)
{
	ast_log(LOG_DEBUG, "mock: hci_close fd=%d\n", fd);
	mock_free_fd(fd);
}

static int mock_hci_get_route(bdaddr_t *addr)
{
	/* Return mock device 0 */
	return 0;
}

static int mock_hci_devinfo(int dev_id, void *di)
{
	/* Fill in mock device info */
	struct hci_dev_info *info = (struct hci_dev_info *)di;
	memset(info, 0, sizeof(*info));
	info->dev_id = dev_id;
	snprintf(info->name, sizeof(info->name), "hci%d", dev_id);
	/* Mock address */
	str2ba("00:11:22:33:44:55", &info->bdaddr);
	return 0;
}

static int mock_hci_inquiry(int dev_id, int len, int max_rsp, void *lap, void **ii, long flags)
{
	ast_mutex_lock(&mock_state.lock);
	
	int count = mock_state.num_devices;
	if (count > max_rsp)
		count = max_rsp;
	
	if (count > 0) {
		inquiry_info *results = malloc(count * sizeof(inquiry_info));
		if (results) {
			for (int i = 0; i < count; i++) {
				memset(&results[i], 0, sizeof(inquiry_info));
				bacpy(&results[i].bdaddr, &mock_state.device_addrs[i]);
			}
			*ii = results;
		}
	} else {
		*ii = NULL;
	}
	
	ast_mutex_unlock(&mock_state.lock);
	
	ast_log(LOG_DEBUG, "mock: hci_inquiry found %d devices\n", count);
	return count;
}

/*
 * SDP operations
 */
static void *mock_sdp_connect(bdaddr_t *src, bdaddr_t *dst, uint32_t flags)
{
	/* Return a non-NULL pointer to indicate success */
	return (void *)0x12345678;
}

static int mock_sdp_search(void *session, void *search, void *attrid, void **rsp)
{
	/* Return empty results */
	*rsp = NULL;
	return 0;
}

static void mock_sdp_close(void *session)
{
	/* Nothing to do */
}

/*
 * Lifecycle
 */
static int mock_init(void)
{
	memset(&mock_state, 0, sizeof(mock_state));
	ast_mutex_init(&mock_state.lock);
	mock_state.initialized = 1;
	
	ast_log(LOG_NOTICE, "chan_mobile: Mock transport initialized\n");
	return 0;
}

static void mock_cleanup(void)
{
	if (mock_state.initialized) {
		ast_mutex_destroy(&mock_state.lock);
		mock_state.initialized = 0;
	}
	ast_log(LOG_NOTICE, "chan_mobile: Mock transport cleanup\n");
}

/*
 * Mock transport operations table
 */
struct mbl_transport_ops mbl_transport_mock = {
	.name = "mock",
	
	.rfcomm_connect = mock_rfcomm_connect,
	.rfcomm_write = mock_rfcomm_write,
	.rfcomm_read = mock_rfcomm_read,
	.rfcomm_wait = mock_rfcomm_wait,
	.rfcomm_close = mock_rfcomm_close,
	
	.sco_connect = mock_sco_connect,
	.sco_bind = mock_sco_bind,
	.sco_accept = mock_sco_accept,
	.sco_write = mock_sco_write,
	.sco_read = mock_sco_read,
	.sco_close = mock_sco_close,
	
	.hci_open = mock_hci_open,
	.hci_close = mock_hci_close,
	.hci_get_route = mock_hci_get_route,
	.hci_devinfo = mock_hci_devinfo,
	.hci_inquiry = mock_hci_inquiry,
	
	.sdp_connect = mock_sdp_connect,
	.sdp_search = mock_sdp_search,
	.sdp_close = mock_sdp_close,
	
	.init = mock_init,
	.cleanup = mock_cleanup,
};

/*
 * Test harness control functions
 */
int mbl_mock_rfcomm_inject(int fd, const char *data, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_RFCOMM) {
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t space = MOCK_BUFFER_SIZE - sock->read_tail;
	if (len > space) {
		ast_mutex_unlock(&mock_state.lock);
		return -1;
	}
	
	memcpy(sock->read_buf + sock->read_tail, data, len);
	sock->read_tail += len;
	
	ast_mutex_unlock(&mock_state.lock);
	
	return 0;
}

ssize_t mbl_mock_rfcomm_drain(int fd, char *buf, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_RFCOMM) {
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t avail = sock->write_tail - sock->write_head;
	if (len > avail)
		len = avail;
	
	memcpy(buf, sock->write_buf + sock->write_head, len);
	sock->write_head += len;
	
	if (sock->write_head == sock->write_tail) {
		sock->write_head = 0;
		sock->write_tail = 0;
	}
	
	ast_mutex_unlock(&mock_state.lock);
	
	return len;
}

int mbl_mock_sco_inject(int fd, const char *data, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_SCO) {
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t space = MOCK_BUFFER_SIZE - sock->read_tail;
	if (len > space) {
		ast_mutex_unlock(&mock_state.lock);
		return -1;
	}
	
	memcpy(sock->read_buf + sock->read_tail, data, len);
	sock->read_tail += len;
	
	ast_mutex_unlock(&mock_state.lock);
	
	return 0;
}

ssize_t mbl_mock_sco_drain(int fd, char *buf, size_t len)
{
	struct mock_socket *sock = mock_get_socket(fd);
	if (!sock || sock->type != MOCK_SOCKET_SCO) {
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	size_t avail = sock->write_tail - sock->write_head;
	if (len > avail)
		len = avail;
	
	memcpy(buf, sock->write_buf + sock->write_head, len);
	sock->write_head += len;
	
	if (sock->write_head == sock->write_tail) {
		sock->write_head = 0;
		sock->write_tail = 0;
	}
	
	ast_mutex_unlock(&mock_state.lock);
	
	return len;
}

int mbl_mock_sco_incoming(int listen_fd, bdaddr_t *remote_addr)
{
	struct mock_socket *sock = mock_get_socket(listen_fd);
	if (!sock || sock->type != MOCK_SOCKET_SCO_LISTEN) {
		return -1;
	}
	
	ast_mutex_lock(&mock_state.lock);
	
	sock->pending_incoming = 1;
	bacpy(&sock->pending_addr, remote_addr);
	
	ast_mutex_unlock(&mock_state.lock);
	
	return 0;
}

int mbl_mock_set_devices(int count, bdaddr_t *addrs, const char **names)
{
	if (count > 16)
		count = 16;
	
	ast_mutex_lock(&mock_state.lock);
	
	mock_state.num_devices = count;
	for (int i = 0; i < count; i++) {
		bacpy(&mock_state.device_addrs[i], &addrs[i]);
		if (names && names[i]) {
			strncpy(mock_state.device_names[i], names[i], 247);
			mock_state.device_names[i][247] = '\0';
		}
	}
	
	ast_mutex_unlock(&mock_state.lock);
	
	return 0;
}

void mbl_mock_reset(void)
{
	ast_mutex_lock(&mock_state.lock);
	
	for (int i = 0; i < MOCK_MAX_FDS; i++) {
		mock_state.sockets[i].type = MOCK_SOCKET_NONE;
	}
	mock_state.num_devices = 0;
	
	ast_mutex_unlock(&mock_state.lock);
}
