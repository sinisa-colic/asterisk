# chan_mobile.c Modernization Plan

## Overview

This document outlines the step-by-step plan to modernize `chan_mobile.c` for reliability on newer Linux kernels and BlueZ stacks, with testability and resilience improvements.

## Current Issues Identified

1. **FD Lifecycle Problems:**
   - `sco_accept()` closes old `pvt->sco_socket` without removing from `io_context`
   - No tracking of `io_context` IDs for device SCO sockets
   - `adapter->sco_id` is stored but not properly validated before removal
   - Missing cleanup in error paths

2. **SCO Listener Fragility:**
   - `do_sco_listen()` breaks on first `ast_io_wait()` failure
   - No recovery mechanism for stale/invalid FDs
   - No logging of errno or state context

3. **No Test Infrastructure:**
   - All code requires real Bluetooth hardware
   - No mock backend for unit/integration tests
   - No deterministic test scenarios

4. **HFP Init Complexity:**
   - Init sequence spread across multiple functions
   - No explicit state machine
   - Hard to debug state transitions

## Implementation Plan

### Phase 1: FD Lifecycle Management (Foundation)

#### Task 1.1: Add io_context ID tracking to structures
- [ ] Add `sco_io_id` field to `struct mbl_pvt` (for device SCO sockets)
- [ ] Change `adapter->sco_id` from `int *` to `int` (simplify)
- [ ] Add assertions in debug builds for FD lifecycle

#### Task 1.2: Create safe FD add/remove helpers
```c
// Pseudocode
static int mbl_io_add_sco(struct mbl_pvt *pvt, int fd, 
                          io_callback callback, void *data) {
    int id;
    ast_mutex_lock(&pvt->lock);
    if (pvt->sco_io_id != 0) {
        ast_log(LOG_ERROR, "SCO already registered for %s\n", pvt->id);
        ast_mutex_unlock(&pvt->lock);
        return -1;
    }
    id = ast_io_add(pvt->adapter->io, fd, callback, AST_IO_IN, data);
    if (id) {
        pvt->sco_io_id = id;
    }
    ast_mutex_unlock(&pvt->lock);
    return id ? 0 : -1;
}

static void mbl_io_remove_sco(struct mbl_pvt *pvt) {
    ast_mutex_lock(&pvt->lock);
    if (pvt->sco_io_id != 0 && pvt->adapter && pvt->adapter->io) {
        ast_io_remove(pvt->adapter->io, pvt->sco_io_id);
        pvt->sco_io_id = 0;
    }
    if (pvt->sco_socket != -1) {
        close(pvt->sco_socket);
        pvt->sco_socket = -1;
    }
    ast_mutex_unlock(&pvt->lock);
}
```

#### Task 1.3: Fix all FD close sites
- [ ] Update `sco_accept()` to remove from io_context before close
- [ ] Update `mbl_read()` error path
- [ ] Update device disconnect paths
- [ ] Update adapter cleanup in `unload_module()`

### Phase 2: Resilient SCO Listener

#### Task 2.1: Enhanced error logging
```c
static void log_io_error(const char *context, struct adapter_pvt *adapter, 
                         int err, const char *operation) {
    char errbuf[256];
    ast_log(LOG_ERROR, 
        "%s: %s failed for adapter %s: %s (errno=%d)\n",
        context, operation, adapter->id, 
        ast_strerror_r(err, errbuf, sizeof(errbuf)), err);
}
```

#### Task 2.2: Resilient do_sco_listen() with rebuild
```c
static void *do_sco_listen(void *data) {
    struct adapter_pvt *adapter = data;
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 5;
    const int BACKOFF_MS = 200;
    
    while (!check_unloading()) {
        int ret_accept, ret_audio;
        
        ret_accept = ast_io_wait(adapter->accept_io, 0);
        if (ret_accept == -1) {
            int err = errno;
            log_io_error("do_sco_listen", adapter, err, "ast_io_wait(accept_io)");
            
            if (err == EBADF || err == EINVAL) {
                // Rebuild accept_io context
                if (rebuild_accept_io_context(adapter) < 0) {
                    ast_log(LOG_ERROR, "Failed to rebuild accept_io for %s\n", 
                            adapter->id);
                    break;
                }
            } else {
                consecutive_errors++;
                if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                    ast_log(LOG_ERROR, "Too many errors, exiting listener for %s\n",
                            adapter->id);
                    break;
                }
                usleep(BACKOFF_MS * 1000);
            }
            continue;
        }
        
        ret_audio = ast_io_wait(adapter->io, 1);
        if (ret_audio == -1) {
            int err = errno;
            log_io_error("do_sco_listen", adapter, err, "ast_io_wait(io)");
            
            if (err == EBADF || err == EINVAL) {
                if (rebuild_audio_io_context(adapter) < 0) {
                    ast_log(LOG_ERROR, "Failed to rebuild audio io for %s\n",
                            adapter->id);
                    break;
                }
            } else {
                consecutive_errors++;
                if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                    break;
                }
                usleep(BACKOFF_MS * 1000);
            }
            continue;
        }
        
        consecutive_errors = 0; // Reset on success
    }
    
    return NULL;
}
```

#### Task 2.3: Implement io_context rebuild functions
```c
static int rebuild_accept_io_context(struct adapter_pvt *adapter) {
    struct io_context *new_io;
    int new_id;
    
    ast_mutex_lock(&adapter->lock); // Need to add lock to adapter_pvt
    
    // Create new context
    new_io = io_context_create();
    if (!new_io) {
        ast_mutex_unlock(&adapter->lock);
        return -1;
    }
    
    // Re-add listener socket if still valid
    if (adapter->sco_socket != -1 && fcntl(adapter->sco_socket, F_GETFD) != -1) {
        new_id = ast_io_add(new_io, adapter->sco_socket, sco_accept, 
                            AST_IO_IN, adapter);
        if (!new_id) {
            io_context_destroy(new_io);
            ast_mutex_unlock(&adapter->lock);
            return -1;
        }
        adapter->sco_id = new_id;
    }
    
    // Swap contexts atomically
    io_context_destroy(adapter->accept_io);
    adapter->accept_io = new_io;
    
    ast_mutex_unlock(&adapter->lock);
    return 0;
}

static int rebuild_audio_io_context(struct adapter_pvt *adapter) {
    struct io_context *new_io;
    struct mbl_pvt *pvt;
    
    new_io = io_context_create();
    if (!new_io) return -1;
    
    // Re-add all valid device SCO sockets
    AST_RWLIST_RDLOCK(&devices);
    AST_RWLIST_TRAVERSE(&devices, pvt, entry) {
        if (pvt->adapter == adapter && pvt->sco_socket != -1) {
            if (fcntl(pvt->sco_socket, F_GETFD) != -1) {
                int id = ast_io_add(new_io, pvt->sco_socket, 
                                    sco_audio_callback, AST_IO_IN, pvt);
                if (id) {
                    ast_mutex_lock(&pvt->lock);
                    pvt->sco_io_id = id;
                    ast_mutex_unlock(&pvt->lock);
                }
            }
        }
    }
    AST_RWLIST_UNLOCK(&devices);
    
    // Swap contexts
    AST_RWLIST_WRLOCK(&adapters);
    io_context_destroy(adapter->io);
    adapter->io = new_io;
    AST_RWLIST_UNLOCK(&adapters);
    
    return 0;
}
```

### Phase 3: Transport Backend Abstraction

#### Task 3.1: Define transport interface
```c
// In chan_mobile_transport.h (new file)
struct mbl_transport_ops {
    const char *name;
    
    // RFCOMM operations
    int (*rfcomm_connect)(bdaddr_t *src, bdaddr_t *dst, int port, int *fd_out);
    ssize_t (*rfcomm_read)(int fd, void *buf, size_t len);
    int (*rfcomm_write_full)(int fd, const void *buf, size_t len);
    void (*rfcomm_close)(int fd);
    
    // SCO operations
    int (*sco_listen_bind)(bdaddr_t *addr, int *fd_out);
    int (*sco_accept)(int listen_fd, bdaddr_t *addr_out, int *fd_out);
    int (*sco_connect)(bdaddr_t *src, bdaddr_t *dst, int *fd_out);
    ssize_t (*sco_read)(int fd, void *buf, size_t len);
    ssize_t (*sco_write)(int fd, const void *buf, size_t len);
    void (*sco_close)(int fd);
    
    // HCI operations (for adapter init)
    int (*hci_open)(const char *addr, int *dev_id_out, int *fd_out);
    void (*hci_close)(int fd);
    int (*hci_read_voice_setting)(int fd, uint16_t *vs);
};
```

#### Task 3.2: Implement BlueZ backend
- [ ] Move existing socket code into `chan_mobile_transport_bluez.c`
- [ ] Implement all transport_ops functions
- [ ] Keep as default backend

#### Task 3.3: Implement Mock backend
- [ ] Create `chan_mobile_transport_mock.c`
- [ ] Use UNIX domain sockets or pipes for RFCOMM simulation
- [ ] Use socketpairs for SCO audio simulation
- [ ] Add scripted response capability

#### Task 3.4: Backend selection
- [ ] Add config option: `transport = bluez|mock` (default: bluez)
- [ ] Compile-time define `CHAN_MOBILE_USE_MOCK` for tests
- [ ] Runtime backend selection in `mbl_load_config()`

### Phase 4: HFP State Machine

#### Task 4.1: Define state enum
```c
enum hfp_state {
    HFP_STATE_DISCONNECTED,
    HFP_STATE_RFCOMM_CONNECTED,
    HFP_STATE_INIT_BRSF_SENT,
    HFP_STATE_INIT_CIND_TEST_SENT,
    HFP_STATE_INIT_CIND_READ_SENT,
    HFP_STATE_INIT_CMER_SENT,
    HFP_STATE_INIT_CLIP_SENT,
    HFP_STATE_INIT_ECAM_SENT,
    HFP_STATE_INIT_READY,
    HFP_STATE_CALL_INCOMING_WAIT_CLIP,
    HFP_STATE_CALL_OUTGOING_DIALING,
    HFP_STATE_CALL_ALERTING,
    HFP_STATE_CALL_ACTIVE,
    HFP_STATE_SMS_MODE_INIT,
    HFP_STATE_SMS_READY,
    HFP_STATE_SMS_SENDING,
    HFP_STATE_ERROR,
};
```

#### Task 4.2: Add state to hfp_pvt
- [ ] Add `enum hfp_state state` field
- [ ] Add `enum hfp_state prev_state` for debugging
- [ ] Add state transition logging function

#### Task 4.3: Refactor init sequence
- [ ] Create `hfp_state_machine_step()` function
- [ ] Move init logic from `do_monitor_phone()` into state machine
- [ ] Handle timeouts with state transitions
- [ ] Make blackberry/nocallsetup explicit in state transitions

### Phase 5: Test Infrastructure

#### Task 5.1: Test framework setup
- [ ] Create `tests/` directory
- [ ] Add test runner (using Asterisk test framework or simple main())
- [ ] Create mock transport test harness

#### Task 5.2: Unit tests
- [ ] `test_at_parsing.c` - AT command parsing
- [ ] `test_message_queue.c` - Message queue behavior
- [ ] `test_alignment_detection.c` - Alignment detection logic
- [ ] `test_state_machine.c` - State transitions

#### Task 5.3: Integration tests
- [ ] `test_hfp_init_happy_path.c` - Full init sequence
- [ ] `test_incoming_call.c` - Incoming call handling
- [ ] `test_outgoing_call.c` - Outgoing call handling
- [ ] `test_sco_churn.c` - SCO connect/disconnect resilience
- [ ] `test_ebadf_recovery.c` - FD lifecycle recovery

#### Task 5.4: CI integration
- [ ] Add test target to Makefile
- [ ] Ensure tests run as non-root
- [ ] No Bluetooth hardware required

### Phase 6: Diagnostics and Observability

#### Task 6.1: Enhanced error logging
- [ ] Add errno to all error logs
- [ ] Include adapter/device IDs in all logs
- [ ] Add FD numbers and io_context IDs to relevant logs
- [ ] Add state information to logs

#### Task 6.2: CLI commands
- [ ] `mobile show adapters` - Show adapter status, FDs, io_context health
- [ ] `mobile debug dump <device>` - Show state, queue, timers, last AT message
- [ ] `mobile show io-stats` - Show io_context statistics

#### Task 6.3: AMI events
- [ ] `MobileReconnectAttempt` - When device reconnection starts
- [ ] `MobileScoConnect` - When SCO connects
- [ ] `MobileScoDisconnect` - When SCO disconnects
- [ ] `MobileStateChange` - On HFP state transitions

### Phase 7: New Kernel/BlueZ Compatibility

#### Task 7.1: Capabilities handling
- [ ] Document required capabilities (CAP_NET_ADMIN, CAP_NET_RAW)
- [ ] Add runtime capability check with helpful error message
- [ ] Update documentation

#### Task 7.2: Profile contention detection
- [ ] Detect rapid connect/disconnect cycles
- [ ] Log warning about PipeWire/desktop audio interference
- [ ] Add config option to disable auto-accept of SCO

#### Task 7.3: Voice setting flexibility
- [ ] Make voice setting check a warning instead of hard error
- [ ] Add config option to override voice setting check
- [ ] Log more detail about voice setting mismatch

## File Structure

```
asterisk/addons/
├── chan_mobile.c              (main file, refactored)
├── chan_mobile_transport.h    (transport interface)
├── chan_mobile_transport_bluez.c  (BlueZ implementation)
├── chan_mobile_transport_mock.c   (Mock implementation)
├── chan_mobile_state.h        (state machine definitions)
├── chan_mobile_state.c        (state machine implementation)
├── chan_mobile_test.h         (test utilities)
└── tests/
    ├── test_at_parsing.c
    ├── test_message_queue.c
    ├── test_state_machine.c
    ├── test_hfp_init.c
    ├── test_incoming_call.c
    ├── test_outgoing_call.c
    ├── test_sco_churn.c
    └── test_ebadf_recovery.c
```

## Testing Plan

### Unit Tests

1. **AT Parsing Tests:**
   - Parse `+CIND: (0,1,2,3,4,5,6)` correctly
   - Parse `+CLIP: "+1234567890",129` correctly
   - Handle malformed AT responses gracefully

2. **Message Queue Tests:**
   - Queue push/pop operations
   - Timeout handling
   - Queue cleanup on disconnect

3. **Alignment Detection Tests:**
   - Detect alignment in known-good audio frames
   - Handle misaligned frames correctly
   - Reset detection state properly

### Integration Tests (Mock Transport)

1. **HFP Init Happy Path:**
   ```
   Script:
   - Send: AT+BRSF=...
   - Expect: +BRSF: <value>
   - Send: OK
   - Send: AT+CIND=?
   - Expect: +CIND: (...)
   - Send: OK
   - Send: AT+CIND?
   - Expect: +CIND: <values>
   - Send: OK
   - Send: AT+CMER=3,0,0,1
   - Send: OK
   - Send: AT+CLIP=1
   - Send: OK
   - Verify: state == HFP_STATE_INIT_READY
   ```

2. **Incoming Call Path:**
   ```
   - Set state: HFP_STATE_INIT_READY
   - Inject: +CIEV: 2,1  (callsetup incoming)
   - Inject: RING
   - Inject: +CLIP: "+1234567890",129
   - Verify: Channel created
   - Verify: PBX started (mock)
   ```

3. **SCO Churn Test:**
   ```
   - Establish SCO connection
   - Start call
   - Simulate SCO disconnect
   - Verify: No thread crash
   - Verify: State recovers
   - Verify: Reconnection attempted
   ```

4. **EBADF Recovery Test:**
   ```
   - Register SCO socket in io_context
   - Force-close socket (simulate kernel close)
   - Call ast_io_wait() (should get EBADF)
   - Verify: io_context rebuilt
   - Verify: Valid sockets re-added
   ```

## How to Run Tests

### Prerequisites
```bash
# No Bluetooth hardware needed
# No root privileges needed
# Just compile with mock transport
```

### Compile with Mock Transport
```bash
cd asterisk/addons
make clean
# Set CHAN_MOBILE_USE_MOCK=1 in Makefile or via -D flag
make chan_mobile.so CHAN_MOBILE_USE_MOCK=1
```

### Run Unit Tests
```bash
cd tests
make
./test_at_parsing
./test_message_queue
./test_state_machine
```

### Run Integration Tests
```bash
cd tests
./test_hfp_init
./test_incoming_call
./test_sco_churn
```

## How to Run on Real Bluetooth

### Standard Operation
```bash
# Use default BlueZ transport
# Ensure Asterisk has required capabilities
sudo setcap cap_net_admin,cap_net_raw+ep /usr/sbin/asterisk

# Or run as root (not recommended)
sudo asterisk -f
```

### Debug Mode
```bash
# Enable verbose logging
asterisk -rvvv
module load chan_mobile.so
mobile show adapters
mobile debug dump <device_id>
```

## Implementation Order

1. **Week 1:** Phase 1 (FD Lifecycle) - Foundation
2. **Week 2:** Phase 2 (Resilient Listener) - Critical fix
3. **Week 3:** Phase 3 (Transport Abstraction) - Enables testing
4. **Week 4:** Phase 4 (State Machine) - Code clarity
5. **Week 5:** Phase 5 (Tests) - Validation
6. **Week 6:** Phase 6 (Diagnostics) - Observability
7. **Week 7:** Phase 7 (Compatibility) - Production readiness

## Success Criteria

- [ ] No thread crashes on SCO connect/disconnect churn
- [ ] `ast_io_wait()` failures recover automatically
- [ ] All FDs properly tracked and cleaned up
- [ ] Tests pass without Bluetooth hardware
- [ ] State machine makes init sequence debuggable
- [ ] Works on Linux kernel 6.8+ and BlueZ 5.60+
- [ ] Backward compatible with existing configs
