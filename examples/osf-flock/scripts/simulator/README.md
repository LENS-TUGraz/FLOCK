# Simulator documentation

This directory contains the Python-based evaluation and visualization tooling for the FLock localization stack. It generates random network scenarios, creates ranging graphs, runs multiple localization methods, and saves plots and CSV summaries for comparison.

## Contents

- [main.py](main.py): active evaluation loop used to run the localization benchmarks
- [Scenario.py](Scenario.py): scenario generation, connectivity checks, and distance generation
- [Position.py](Position.py): node position data model
- [localization](localization): localization implementations and diagnostics
- [requirements.txt](requirements.txt): Python dependencies
- [fig](fig): generated plot output directory
- [scenarios](scenarios): saved scenario JSON files
- [results](results): CSV results written by the simulator

## Overview

The simulator models a set of nodes placed in a 2D area and generates pairwise ranging distances. It then feeds those distances and a subset of known anchor positions into several localization engines and compares the estimated coordinates against the true positions.

The main workflow is:

1. Generate one or more scenarios with random node positions.
2. Produce pairwise distance measurements with optional noise and offset.
3. Provide a subset of known positions.
4. Run candidate localizers.
5. Plot results and save metrics.

The active benchmarking loop is in [main.py](main.py). It evaluates:

- FLock via [localization/FlockLoc.py](localization/FlockLoc.py)
- MDS via [localization/MDSLoc.py](localization/MDSLoc.py)
- BFGS via [localization/BFGSLoc.py](localization/BFGSLoc.py)
- GNU Gama via [localization/GNUGamaLoc.py](localization/GNUGamaLoc.py)
- dummy baseline utilities when required

## Requirements

Use Python 3.10+ and install the project dependencies from [requirements.txt](requirements.txt):

```bash
cd examples/osf-flock/scripts/simulator
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

The environment already includes optional simulator-specific packages such as `matplotlib`, `numpy`, `pandas`, `scipy`, `networkx`, `pyserial`, and `tqdm`.

## Running the simulator

From the simulator directory:

```bash
cd examples/osf-flock/scripts/simulator
source .venv/bin/activate
python main.py
```

This launches the simulation loop defined in [main.py](main.py). It will:

- generate scenarios for each configured node count and area size
- create random ranging noise and offset values
- run localization algorithms
- save generated plots into [fig](fig)
- save scenario files under [scenarios](scenarios)
- save aggregate metrics to [results/simulation_results.csv](results/simulation_results.csv)

## Replaying recorded ranging data

The simulator also includes a replay workflow in [replay.py](replay.py). This mode does not generate synthetic scenarios; instead, it loads a recorded SS-TWR CSV and re-injects the same distance measurements into a connected FLock device, allowing you to replay real-world measurements against the embedded localization stack.

Typical usage:

```bash
cd examples/osf-flock/scripts/simulator
source .venv/bin/activate
python replay.py \
  --sstwr rec_data/trento/department/sstwr.csv \
  --positions rec_data/trento/department/department.csv \
  --port /dev/ttyACM0 \
  --baudrate 230400 \
  --known-nodes 17 5 21 29
```

The script reads:

- a ranging dataset such as `sstwr.csv`
- a node-position file such as `department.csv`
- a serial device to which the FLock node is connected

It then:

1. loads the ranging table and known node coordinates
2. aggregates distances by node pair
3. sends one measurement batch at a time to the FLock runtime
4. runs the localization algorithms on the replayed data
5. saves an output CSV under [results](results)
6. optionally renders live plots for the replay progression

### Replay-specific arguments

The script supports the following useful arguments:

```bash
python replay.py --help
```

Key options include:

- `--sstwr`: path to the ranging CSV
- `--positions`: path to the position CSV
- `--port`: target serial port for the FLock node
- `--baudrate`: serial baudrate used by the board
- `--known-nodes`: node IDs to treat as anchors
- `--distance-column`: one of `dist`, `dist_bias`, `dist_drift`, or `dist_drift_bias`
- `--output`: output CSV path for replay metrics
- `--no-plot`: disable plots while replaying
- `--debug`: enable verbose debug logging

### Replay output

The replay script writes a CSV summary and, when plotting is enabled, saves figures such as:

- [fig/replay_results.png](fig/replay_results.png)
- [fig/replay_rmse_over_time.png](fig/replay_rmse_over_time.png)

These outputs show the per-round localization performance and the RMSE evolution over the replay sequence.

### Scenario generation

The scenario logic in [Scenario.py](Scenario.py) creates a list of `Position` objects, each with:

- `id`
- `x_m`
- `y_m`
- `known`

A scenario can be generated randomly, optionally ensuring the graph remains connected. Random known positions can be placed either in corners or at random indices.

The main parameters are defined near the top of [main.py](main.py):

```python
SEED = 42
REPETITIONS = 10
SCENARIOS = 25
SCENARIO_AREAS = [10]
SCENARIO_NUM_NODES = [15, 30, 45]
SCENARIO_KNOWN_POSITIONS = 4
SCENARIO_KNOWN_POSITIONS_PLACEMENT = 'corner'
RANGING_NOISE_STD_M = 0.1368
RANGING_OFFSET_MEAN_M = -0.0377
RANGING_MAX_DISTANCE_M = 15.0
```

These values control:

- number of runs per configuration
- number of scenarios to generate
- node density and area size
- how many anchors are known
- measurement noise and observation range

## Output files

### Plots

The plotting functions in [main.py](main.py) create PNG files in [fig](fig). Each plot shows:

- known positions
- true node positions
- FLock estimates
- MDS estimates
- BFGS estimates
- Gama estimates
- connectivity edges between nodes

### Scenario JSON

Scenario files are stored under [scenarios](scenarios) and can be reused later. They serialize a scenario as a list of positions and metadata such as name, area size, and node count.

### Result CSV

The simulator writes aggregated metrics to [results/simulation_results.csv](results/simulation_results.csv). The recorded values include:

- scenario name
- area size
- number of nodes
- ranging noise and offset values
- RMSE per algorithm
- per-node error dictionaries
- runtime diagnostics when available

## Localization engine interface

Each localization implementation follows a common pattern via [localization/LocalizationEngine.py](localization/LocalizationEngine.py):

- `localize(range_measurements, known_positions)`
- optional `clear()` method to reset internal state

The implementations return:

- a dictionary of estimated positions keyed by node id string
- a `LocalizationEngineDiagnostics` object containing runtime information and success metadata