# FLock

FLock is a localization and ranging extension built on top of [OSF](https://github.com/open-sf/osf). The example firmware is under [examples/osf-flock](examples/osf-flock), and its runtime configuration is defined in [examples/osf-flock/project-conf.h](examples/osf-flock/project-conf.h).

## Requirements

This project is a Contiki-NG-based firmware. FLOCK has been designed for the Qorvo DWM1001-Dev board.

A typical setup includes:

- the repository checked out with its submodules/dependencies available
- a supported target such as `nrf52840`
- a `BOARD` such as `dwm1001`
- a serial connection for flashing and shell access
- `nrfjprog` for the automatic deployment helper on Nordic boards

## Setup

FLock inherits the OSF deployment model: each node is assigned an integer ID based on its MAC address via a deployment map.

### Generate a deployment map

The preferred method is the helper script in [tools/nrf/nrf-helper.sh](tools/nrf/nrf-helper.sh):

```bash
cd tools/nrf
./nrf-helper.sh -info -s -d
```

This script scans connected Nordic devices, resolves each board MAC address, and writes deployment files under:

- [os/services/deployment/nulltb/deployment-map-nulltb.c](os/services/deployment/nulltb/deployment-map-nulltb.c)
- [os/services/deployment/nulltb/deployment-map-nulltb.h](os/services/deployment/nulltb/deployment-map-nulltb.h)

### Manual deployment map

If you want to create the files manually, the deployment table must match the format used by the deployment service. The array entries are `id, mac` pairs, and the header defines `DEPLOYMENT_MAPPING_LEN`.

Example for a two-node setup:

1. Create [os/services/deployment/nulltb/deployment-map-nulltb.c](os/services/deployment/nulltb/deployment-map-nulltb.c):

```c
#include "services/deployment/deployment.h"
#include "deployment-map-nulltb.h"

const struct id_mac deployment_nulltb[] = {
  { 1, {{0xf4, 0xce, 0x36, 0xfa, 0x3d, 0x0a, 0x38, 0x4f}}},
  { 2, {{0xf4, 0xce, 0x36, 0x08, 0xcb, 0x9d, 0x4b, 0x98}}},
  { 0, {{0}}}
};
```

2. Create [os/services/deployment/nulltb/deployment-map-nulltb.h](os/services/deployment/nulltb/deployment-map-nulltb.h):

```c
#ifndef DEPLOYMENT_MAP_NULLTB_H_
#define DEPLOYMENT_MAP_NULLTB_H_

#define DEPLOYMENT_MAPPING_LEN 2

#endif /* DEPLOYMENT_MAP_NULLTB_H_ */
```

During startup, the device prints its link-layer address and node ID. A value like:

```text
[INFO: Main      ] Node ID: 0
[INFO: Main      ] Link-layer address: f4ce.36fa.3d0a.384f
```

means that the MAC was not found in the deployment table yet. The `mac-addr` shell command can also be used to print the current radio address.

## Build and flash

Build and flash the example firmware from the example directory:

```bash
cd examples/osf-flock
make clean TARGET=nrf52840 BOARD=dwm1001 DEPLOYMENT=nulltb & make node.upload-all TARGET=nrf52840 BOARD=dwm1001 DEPLOYMENT=nulltb
```

The example project uses the OSF and FLock configuration from [examples/osf-flock/project-conf.h](examples/osf-flock/project-conf.h), and it compiles the FLock application modules from [examples/osf-flock/Makefile](examples/osf-flock/Makefile).

## Login

Nodes expose a shell via the serial console. The default baud rate is `230400`.

```bash
make TARGET=nrf52840 login PORT=/dev/ttyACM0 BAUDRATE=230400
```

If the port is not known in advance, use the serial port that matches your board and update `PORT` accordingly.

## Simulation

The Python simulation tooling lives under [examples/osf-flock/scripts/simulator](examples/osf-flock/scripts/simulator). Relevant entry points include:

- [examples/osf-flock/scripts/simulator/main.py](examples/osf-flock/scripts/simulator/main.py)
- [examples/osf-flock/scripts/simulator/main_new.py](examples/osf-flock/scripts/simulator/main_new.py)

The simulator uses the FLock localization logic in the same folder hierarchy and can be run from that directory.

## Configuration

The runtime configuration is driven by `FLOCK_CONF_*` and `OSF_CONF_*` values, typically defined in [examples/osf-flock/project-conf.h](examples/osf-flock/project-conf.h) and resolved in [examples/osf-flock/flock-conf.h](examples/osf-flock/flock-conf.h).

### Core FLock settings

| Option | Type | Default in example config | Notes |
| --- | --- | --- | --- |
| `FLOCK_MAX_NODES` | int | `51` | maximum supported node count |
| `FLOCK_CONF_DIST_TABLE_SIZE` | int | `FLOCK_MAX_NODES * (FLOCK_MAX_NODES - 1)` | distance table capacity |
| `FLOCK_CONF_POS_TABLE_SIZE` | int | `FLOCK_MAX_NODES` | position table capacity |
| `FLOCK_CONF_REPORT_INTERVAL` | clock_time_t | `CLOCK_SECOND * 1` | periodic report cadence |
| `FLOCK_CONF_PRINT_RANGE_REPORT` | bool | `1` | print range reports when received |
| `FLOCK_CONF_PRINT_RANGE_RESULT` | bool | `0` | print raw range result |

### OSF settings

| Option | Type | Default in example config | Notes |
| --- | --- | --- | --- |
| `OSF_CONF_PERIOD_MS` | int | `900` | OSF period in milliseconds |
| `OSF_CONF_NTX` | int | `3` | retransmissions per TX slot |
| `OSF_CONF_PROTO` | enum | `OSF_PROTO_STT` | protocol selection |
| `OSF_CONF_PHY` | enum | `PHY_BLE_2M` | PHY selection |

### Localization settings

| Option | Type | Default in example config | Notes |
| --- | --- | --- | --- |
| `FLOCK_CONF_LOC_LEARNING_RATE` | float | `0.020f` | gradient descent learning rate |
| `FLOCK_CONF_LOC_ITERATIONS` | int | `500` | maximum optimization iterations |
| `FLOCK_CONF_LOC_EPOCHS_TO_KEEP` | int | `0` | number of epochs retained |
| `FLOCK_CONF_LOC_MIN_DIST_MM` | int | `0` | minimum range used for localization |
| `FLOCK_CONF_LOC_MAX_DIST_MM` | int | `100000` | maximum range used for localization |
| `FLOCK_CONF_LOC_MAX_RETRIES` | int | `16` | localization retry count |
| `FLOCK_CONF_LOC_APRIORI_RANGE_ERROR_MM` | int | `250` | expected residual error in mm |
| `FLOCK_CONF_LOC_UPDATE_RATE` | clock_time_t | `0` | periodic localization rate |
| `FLOCK_CONF_LOC_INIT_AREA_M` | float | `80.0f` | initial search area in meters |

## Shell commands

A node in FLock can be controlled through the serial shell. The commands exposed by the example build are:

| Command | Parameters | Description |
| --- | --- | --- |
| `flock-dist-table` | none | print the current distance table |
| `flock-dist-table-clear` | none | clear the distance table |
| `flock-pos-table` | none | print the current position table |
| `flock-pos-table-clear` | none | clear the position table |
| `flock-set-pos` | `<id> <x> <y>` | set a node position in millimeters |
| `flock-set-dist` | `<from_id> <to_id> <dist_mm>` | manually set a distance entry |
| `flock-localize` | none | trigger localization |
| `flock-loc-set-iterations` | `<num>` | set the maximum number of localization iterations |
| `flock-loc-set-lr` | `<lr>` | set the learning rate |
| `flock-radio-status` | none | print the DW1000 radio status |

