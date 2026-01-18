# chan_mobile.c Modernization - Implementation Summary

## Status

**Phase 1: FD Lifecycle Management** - COMPLETED ✅
**Phase 2: Resilient SCO Listener** - COMPLETED ✅
**Phase 3: Transport Abstraction Layer** - COMPLETED ✅
**Phase 4: Improved CLI Diagnostics** - COMPLETED ✅

### Completed Changes:

#### Structure Updates:
- Added `pvt->sco_io_id` field (`int *`) to track device SCO socket io_context IDs
- Added `adapter->lock` mutex for thread-safe io_context operations

#### FD Lifecycle Helper Functions (new):
- `mbl_io_remove_sco()` - safely remove and close device SCO socket (acquires lock)
- `mbl_io_remove_sco_locked()` - same but caller holds lock
- `mbl_io_remove_adapter_sco()` - safely remove adapter SCO listener

#### Fixed FD Close Sites:
- `sco_accept()` - now uses `mbl_io_remove_sco_locked()` before close
- `mbl_read()` error path - now uses `mbl_io_remove_sco_locked()`
- `do_monitor_phone()` cleanup - now uses `mbl_io_remove_sco_locked()`
- `do_monitor_headset()` cleanup - now uses `mbl_io_remove_sco_locked()`
- `unload_module()` - now properly removes from io_context before close
- `mbl_load_adapter()` - proper initialization of new fields and cleanup paths

#### Resilient SCO Listener:
- `rebuild_accept_io_context()` - rebuilds accept_io on EBADF/EINVAL errors
- `rebuild_audio_io_context()` - rebuilds audio io on EBADF/EINVAL errors
- `do_sco_listen()` now:
  - Handles EBADF/EINVAL by rebuilding io_context instead of exiting
  - Implements backoff sleep (100ms) on transient errors
  - Limits consecutive errors (max 10) before exiting
  - Enhanced logging with errno and strerror

### Build Status:
- ✅ Compiles successfully
- ✅ Object file: `chan_mobile.o` (194KB)
- ✅ Shared object: `chan_mobile.so` (125KB)

#### Transport Abstraction Layer (Phase 3):
New files created:
- `chan_mobile_transport.h` - Transport abstraction header
- `chan_mobile_transport.c` - Transport initialization
- `chan_mobile_transport_bluez.c` - Real BlueZ backend
- `chan_mobile_transport_mock.c` - Mock backend for testing

#### Improved CLI Commands (Phase 4):
- `mobile show adapters` - Show all Bluetooth adapters with status
- `mobile show device <id>` - Detailed device status including:
  - Connection status (RFCOMM/SCO sockets)
  - HFP initialization state
  - Call status (incoming/outgoing/answered)
  - SMS status
  - HFP indicators (service, call, signal, battery)
  - Device flags (blackberry mode, alignment detection)

#### Test Infrastructure (Phase 5):
- `test_chan_mobile.c` - Unit tests for AT command parsing
- `Makefile.test` - Build and run tests
- Tests cover:
  - +CIND response parsing (indicator map and state)
  - +CLIP caller ID parsing
  - +CMTI SMS notification parsing
  - +BRSF feature parsing
  - AT command generation
  - Bluetooth address handling

Run tests:
```bash
cd /home/sinisa/telefonija/asterisk/addons
make -f Makefile.test
```

#### HFP State Machine (Phase 6):
- Added explicit `enum hfp_slc_state` with 12 states
- State transitions logged at verbosity level 4
- CLI shows current SLC state via `mobile show device <id>`
- States: DISCONNECTED -> CONNECTING -> BRSF_SENT -> CIND_TEST_SENT -> CIND_SENT -> CMER_SENT -> CLIP_SENT -> ECAM_SENT -> VGS_SENT -> CMGF_SENT -> CNMI_SENT -> CONNECTED

#### Connection Retry with Backoff (Phase 7):
- Exponential backoff on connection failures (5s -> 10s -> 20s -> ... -> 300s max)
- Tracks: `connect_failures`, `backoff_seconds`, `last_connect_attempt`
- CLI shows retry state via `mobile show device <id>`
- New CLI command: `mobile reset backoff <id>` to force immediate retry

#### Enhanced AMI Events (Phase 8):
- `MobileStatus` events now include more detail:
  - `Status: Connect` - includes Address
  - `Status: Disconnect` - device disconnected
  - `Status: Initialized` - HFP SLC complete, includes Service/Signal/Battery
  - `Status: ConnectFailed` - includes Reason, Failures, BackoffSeconds

#### Timeout and Keepalive (Phase 9):
- Improved timeout handling during HFP initialization with specific error messages
- 60-second keepalive timeout after initialization
- Sends AT+CIND? as keepalive to detect dead connections
- Disconnect reason set based on timeout state (e.g., "Timeout during CIND test")

#### Unit Tests (17 tests):
- +CIND test/state parsing
- +CLIP caller ID parsing  
- +CMTI SMS notification parsing
- +BRSF feature parsing
- +CIEV indicator event parsing
- +CUSD USSD response parsing
- Error response parsing
- SMS prompt detection
- AT command generation
- Bluetooth address handling

Run tests: `make -f Makefile.test` in addons/

#### Signal/Battery/Service Monitoring (Phase 10):
- New AMI events for indicator changes:
  - `MobileSignal` - Signal strength (0-5)
  - `MobileBattery` - Battery level (0-5)
  - `MobileService` - Cellular service available/lost
  - `MobileRoaming` - Roaming status changed
- Verbose logging at level 3-4 for indicator changes

#### Statistics Tracking (Phase 11):
- Per-device statistics:
  - `calls_in` - Incoming calls received
  - `calls_out` - Outgoing calls made
  - `calls_answered` - Calls that were answered
  - `total_call_seconds` - Total call duration
  - `sms_in` - SMS messages received
  - `sms_out` - SMS messages sent
  - `connected_since` - Connection uptime tracking
- New CLI commands:
  - `mobile show stats` - Show statistics summary for all devices
  - `mobile reset stats <id|all>` - Reset statistics for a device or all devices
- Statistics shown in `mobile show device <id>` output

#### Call Duration Tracking (Phase 12):
- Track call start/end times
- Calculate and accumulate call duration
- New AMI events:
  - `MobileCallStart` - Emitted when call is answered (includes Direction: Incoming/Outgoing)
  - `MobileCallEnd` - Emitted when call ends (includes Direction and Duration in seconds)
- CLI shows total call time in HH:MM:SS format

#### Device Health Check (Phase 13):
- New CLI command: `mobile check <id>` performs comprehensive health checks:
  - Connection status
  - RFCOMM socket status
  - HFP Service Level Connection state
  - Cellular service availability
  - Signal strength (PASS/WARN/FAIL based on level)
  - Battery level (PASS/WARN/FAIL based on level)
  - Roaming status
  - Device availability (not in call)
  - Recent disconnect warnings
- Summary shows passed/warnings/failed counts and overall health status

#### Device Management (Phase 14):
- New CLI command: `mobile disconnect <id>` - Force disconnect a device
  - Hangs up any active call first
  - Closes RFCOMM socket
  - Device will auto-reconnect on next discovery cycle
- New CLI command: `mobile show version` - Shows version and feature summary
  - Lists all available CLI commands
  - Shows adapter/device counts
  - Shows enabled features

### CLI Commands Summary:
| Command | Description |
|---------|-------------|
| `mobile show devices` | List all configured devices |
| `mobile show device <id>` | Detailed device status |
| `mobile show adapters` | List Bluetooth adapters |
| `mobile show stats` | Statistics for all devices |
| `mobile show version` | Version and feature info |
| `mobile check <id>` | Health check |
| `mobile disconnect <id>` | Force disconnect |
| `mobile reset backoff <id>` | Reset retry backoff |
| `mobile reset stats <id\|all>` | Reset statistics |
| `mobile search` | Scan for BT devices |
| `mobile rfcomm <id> <cmd>` | Send AT command |
| `mobile cusd <id> <code>` | Send USSD code |

### AMI Events Summary:
| Event | Description |
|-------|-------------|
| `MobileStatus` | Connect/Disconnect/Initialized/ConnectFailed |
| `MobileSignal` | Signal strength changed |
| `MobileBattery` | Battery level changed |
| `MobileService` | Cellular service status |
| `MobileRoaming` | Roaming status changed |
| `MobileCallStart` | Call answered (with direction) |
| `MobileCallEnd` | Call ended (with duration) |

#### Call State Tracking (Phase 15):
- New call state enum with 9 states:
  - `MBL_CALL_IDLE` - No call in progress
  - `MBL_CALL_INCOMING_RING` - Incoming call ringing
  - `MBL_CALL_INCOMING_WAIT_CID` - Waiting for caller ID
  - `MBL_CALL_OUTGOING_DIALING` - Outgoing call dialing
  - `MBL_CALL_OUTGOING_ALERTING` - Remote party ringing
  - `MBL_CALL_ACTIVE` - Call connected/active
  - `MBL_CALL_HOLD` - Call on hold
  - `MBL_CALL_WAITING` - Call waiting
  - `MBL_CALL_HANGUP_PENDING` - Hangup in progress
- State transitions logged at verbosity level 4
- CLI shows current call state via `mobile show device <id>`
- Tracks caller ID and dialed number per call

#### Enhanced Call AMI Events (Phase 16):
- `MobileCallIncoming` - Incoming call with CallerID and CallerIDName
- `MobileCallOutgoing` - Outgoing call with Destination number
- `MobileCallStart` - Call answered with Direction and CallerID/DialedNumber
- `MobileCallEnd` - Enhanced with Answered, CallerID, DialedNumber, PreviousState
- `MobileAudioError` - Audio read errors with error count

#### Audio Quality Monitoring (Phase 17):
- Track audio errors per call (`audio_errors` counter)
- AMI event emitted on first error and every 10th error
- Audio error count shown in CLI device status
- Counter reset on call end

#### Modular Code Structure (Phase 18):
New modular file structure for future development:

| File | Purpose |
|------|---------|
| `chan_mobile_pvt.h` | Shared structures and types |
| `chan_mobile_cli.h` | CLI command declarations |
| `chan_mobile_ami.h` | AMI event helper declarations |
| `chan_mobile_ami.c` | AMI event helper implementations |
| `chan_mobile_stats.h` | Statistics tracking declarations |
| `chan_mobile_stats.c` | Statistics tracking implementations |
| `chan_mobile_state.h` | State management declarations |
| `chan_mobile_state.c` | State management implementations |
| `chan_mobile_transport.h` | Transport abstraction layer |
| `chan_mobile_transport.c` | Transport initialization |
| `chan_mobile_transport_bluez.c` | Real BlueZ backend |
| `chan_mobile_transport_mock.c` | Mock backend for testing |

Future new features should be added to these modular files rather than chan_mobile.c.

### Next Steps:
1. Wire transport abstraction into chan_mobile.c (deferred - large refactor)
2. Add integration tests with mock transport
3. Handle new kernel/BlueZ compatibility issues
4. Add call waiting/hold support indicators
5. Migrate existing CLI handlers to chan_mobile_cli.c

## Key Changes Summary

### Before (buggy):
```c
// In sco_accept():
if (pvt->sco_socket != -1) {
    close(pvt->sco_socket);  // BUG: Not removed from io_context!
    pvt->sco_socket = -1;
}

// In do_sco_listen():
if (ast_io_wait(adapter->accept_io, 0) == -1) {
    ast_log(LOG_ERROR, "ast_io_wait() failed for adapter %s\n", adapter->id);
    break;  // BUG: Exits on first error!
}
```

### After (fixed):
```c
// In sco_accept():
if (pvt->sco_socket != -1) {
    mbl_io_remove_sco_locked(pvt);  // Safe removal from io_context + close
}

// In do_sco_listen():
if (ast_io_wait(adapter->accept_io, 0) == -1) {
    err = errno;
    ast_log(LOG_ERROR, "[%s] ast_io_wait(accept_io) failed: %s (errno=%d)\n",
            adapter->id, strerror(err), err);
    
    if (err == EBADF || err == EINVAL) {
        if (rebuild_accept_io_context(adapter) < 0) {
            break;  // Only exit if rebuild fails
        }
        consecutive_errors = 0;
    } else {
        consecutive_errors++;
        if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
            break;
        }
        usleep(BACKOFF_MS * 1000);
    }
    continue;  // Try again after recovery
}
```

## Testing

To test the changes:

1. **Compile:**
   ```bash
   cd /home/sinisa/telefonija/asterisk/addons
   make -f Makefile.install compile
   ```

2. **Install:**
   ```bash
   make -f Makefile.install install
   ```

3. **Load:**
   ```bash
   make -f Makefile.install load
   ```

4. **Verify:**
   ```bash
   make -f Makefile.install verify
   ```

Or all at once:
```bash
make -f Makefile.install all
```
