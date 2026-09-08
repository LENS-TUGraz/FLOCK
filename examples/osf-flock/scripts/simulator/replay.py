"""
Replay ranging data from sstwr.csv to a connected Flock device for localization.

This script reads pre-recorded SS-TWR ranging measurements and sends them to
a connected embedded device to perform localization, similar to main.py but
using real recorded data instead of simulated scenarios.
"""

from localization import FlockLoc, LocalizationEngineDiagnostics
from localization import MDSLoc
from localization import GNUGamaLoc
from matplotlib import pyplot as plt
import numpy as np
import pandas as pd
import argparse
import os

import logging

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

# Default paths relative to script directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SSTWR_CSV = os.path.join(SCRIPT_DIR, 'rec_data/trento/department/sstwr.csv')
DEFAULT_POSITIONS_CSV = os.path.join(SCRIPT_DIR, 'rec_data/trento/department/department.csv')

# Default configuration
DEFAULT_PORT = '/dev/ttyACM0'
DEFAULT_BAUDRATE = 230400
DEFAULT_TIMEOUT = 10
DEFAULT_KNOWN_NODES = [17, 5, 21, 29]  # Corner nodes as known positions


def load_ranging_data(sstwr_csv_path: str, distance_column: str = 'dist') -> pd.DataFrame:
    """
    Load ranging data from sstwr.csv file.
    
    Args:
        sstwr_csv_path: Path to the sstwr.csv file
        distance_column: Which distance column to use ('dist', 'dist_bias', 'dist_drift', 'dist_drift_bias')
    
    Returns:
        DataFrame with columns: node, anchor, distance
    """
    log.info(f"Loading ranging data from {sstwr_csv_path}")
    df = pd.read_csv(sstwr_csv_path, sep='\t')
    log.info(f"Loaded {len(df)} ranging measurements")
    return df


def load_positions(positions_csv_path: str) -> dict:
    """
    Load node positions from department.csv file.
    
    Args:
        positions_csv_path: Path to the positions CSV file
        
    Returns:
        Dictionary mapping node_id -> {'x_m': x, 'y_m': y}
    """
    log.info(f"Loading positions from {positions_csv_path}")
    df = pd.read_csv(positions_csv_path, sep='\t')
    
    positions = {}
    for _, row in df.iterrows():
        node_id = int(row['node'])
        positions[str(node_id)] = {
            'x_m': float(row['x']),
            'y_m': float(row['y'])
        }
    
    log.info(f"Loaded {len(positions)} node positions")
    return positions


def aggregate_range_measurements(df: pd.DataFrame, distance_column: str = 'dist') -> dict:
    """
    Aggregate ranging measurements by computing median distance for each node pair.
    
    Args:
        df: DataFrame with ranging measurements
        distance_column: Column to use for distance
        
    Returns:
        Dictionary mapping (from_id, to_id) -> distance_m
    """
    log.info(f"Aggregating range measurements using {distance_column} column")
    
    # Group by node pairs and compute median
    grouped = df.groupby(['node', 'anchor'])[distance_column].median()
    
    range_measurements = {}
    for (node, anchor), dist in grouped.items():
        # Use tuple with sorted IDs to avoid duplicates
        key = (int(node), int(anchor))
        range_measurements[key] = float(dist)
    
    log.info(f"Aggregated to {len(range_measurements)} unique distance measurements")
    return range_measurements


def get_unique_node_pairs(df: pd.DataFrame) -> list:
    """Get list of unique (node, anchor) pairs in the data."""
    pairs = df.groupby(['node', 'anchor']).size().reset_index()[['node', 'anchor']]
    return [(int(row['node']), int(row['anchor'])) for _, row in pairs.iterrows()]


def iterate_range_measurements(df: pd.DataFrame, distance_column: str = 'dist'):
    """
    Iterator that yields range measurement dictionaries, updating one measurement per node-pair at a time.
    
    Groups measurements by (node, anchor) pairs and iterates through them round by round.
    In each round, every node pair gets its next measurement (if available).
    
    Args:
        df: DataFrame with ranging measurements
        distance_column: Column to use for distance
        
    Yields:
        Tuple of (round_index, measurements_in_round, current_range_measurements_dict)
    """
    # Group measurements by node pair and convert to lists
    pair_measurements = {}
    for _, row in df.iterrows():
        node = int(row['node'])
        anchor = int(row['anchor'])
        dist = float(row[distance_column])
        seqn = row.get('seqn', 0)
        ts = row.get('ts', 0)
        
        key = (node, anchor)
        if key not in pair_measurements:
            pair_measurements[key] = []
        pair_measurements[key].append({'dist': dist, 'seqn': seqn, 'ts': ts})
    
    # Find the maximum number of measurements for any pair
    max_rounds = max(len(measurements) for measurements in pair_measurements.values())
    log.info(f"Found {len(pair_measurements)} unique node pairs, max {max_rounds} measurements per pair")
    
    current_distances = {}
    
    # Iterate round by round
    for round_idx in range(max_rounds):
        measurements_this_round = 0
        latest_ts = 0
        latest_seqn = 0
        
        # Update one measurement per node pair
        for (node, anchor), measurements in pair_measurements.items():
            if round_idx < len(measurements):
                measurement = measurements[round_idx]
                current_distances[(node, anchor)] = measurement['dist']
                measurements_this_round += 1
                latest_ts = max(latest_ts, measurement['ts'])
                latest_seqn = max(latest_seqn, measurement['seqn'])
        
        # Yield current state
        yield round_idx, latest_seqn, latest_ts, measurements_this_round, dict(current_distances)


def get_known_positions(positions: dict, known_node_ids: list) -> dict:
    """
    Extract known positions for specified nodes.
    
    Args:
        positions: Full position dictionary
        known_node_ids: List of node IDs to treat as known (anchors)
        
    Returns:
        Dictionary mapping node_id -> (x_m, y_m) for known nodes
    """
    known_positions = {}
    for node_id in known_node_ids:
        key = str(node_id)
        if key in positions:
            pos = positions[key]
            known_positions[key] = (pos['x_m'], pos['y_m'])
        else:
            log.warning(f"Node {node_id} not found in positions")
    
    log.info(f"Using {len(known_positions)} known positions: {list(known_positions.keys())}")
    return known_positions


def plot_results(true_positions: dict, known_node_ids: list, 
                 flock_positions: dict = None, flock_rmse: float = None,
                 mds_positions: dict = None, mds_rmse: float = None,
                 gama_positions: dict = None, gama_rmse: float = None,
                 title: str = "Localization Results"):
    """
    Plot localization results comparing true vs estimated positions for multiple methods.
    """
    plt.clf()
    fig, ax = plt.subplots(num=1)
    ax.set_title(title)
    
    # Collect all x, y values to set axis limits
    all_x = [pos['x_m'] for pos in true_positions.values()]
    all_y = [pos['y_m'] for pos in true_positions.values()]
    
    margin = 10
    ax.set_xlim(min(all_x) - margin, max(all_x) + margin)
    ax.set_ylim(min(all_y) - margin, max(all_y) + margin)
    
    # Plot known positions (anchors)
    known_x = [true_positions[str(nid)]['x_m'] for nid in known_node_ids if str(nid) in true_positions]
    known_y = [true_positions[str(nid)]['y_m'] for nid in known_node_ids if str(nid) in true_positions]
    ax.scatter(known_x, known_y, c='orange', marker='s', s=150, label='Known Positions (Anchors)', zorder=5)
    
    # Plot true positions
    true_x = [pos['x_m'] for pos in true_positions.values()]
    true_y = [pos['y_m'] for pos in true_positions.values()]
    ax.scatter(true_x, true_y, c='green', s=100, label='True Positions', zorder=3)
    
    # Add node labels
    for node_id, pos in true_positions.items():
        ax.annotate(node_id, (pos['x_m'], pos['y_m']), textcoords="offset points", 
                   xytext=(5, 5), fontsize=8, alpha=0.7)
    
    # Plot Flock estimated positions if available
    if flock_positions:
        est_x = [pos['x_m'] for pos in flock_positions.values() if pos['x_m'] is not None]
        est_y = [pos['y_m'] for pos in flock_positions.values() if pos['y_m'] is not None]
        label = f'Flock (RMSE: {flock_rmse:.3f} m)' if flock_rmse else 'Flock'
        ax.scatter(est_x, est_y, c='blue', marker='x', s=80, label=label, zorder=4)
        
        # Draw error lines from true to estimated
        for node_id, est_pos in flock_positions.items():
            if est_pos['x_m'] is not None and node_id in true_positions:
                true_pos = true_positions[node_id]
                ax.plot([true_pos['x_m'], est_pos['x_m']], 
                       [true_pos['y_m'], est_pos['y_m']], 
                       'b--', alpha=0.5, linewidth=0.8)
    
    # Plot MDS estimated positions if available
    if mds_positions:
        est_x = [pos['x_m'] for pos in mds_positions.values() if pos['x_m'] is not None]
        est_y = [pos['y_m'] for pos in mds_positions.values() if pos['y_m'] is not None]
        label = f'MDS (RMSE: {mds_rmse:.3f} m)' if mds_rmse else 'MDS'
        ax.scatter(est_x, est_y, c='red', marker='^', s=80, label=label, zorder=4)
        
        # Draw error lines from true to estimated
        for node_id, est_pos in mds_positions.items():
            if est_pos['x_m'] is not None and node_id in true_positions:
                true_pos = true_positions[node_id]
                ax.plot([true_pos['x_m'], est_pos['x_m']], 
                       [true_pos['y_m'], est_pos['y_m']], 
                       'r--', alpha=0.5, linewidth=0.8)
    
    # Plot Gama estimated positions if available
    if gama_positions:
        est_x = [pos['x_m'] for pos in gama_positions.values() if pos['x_m'] is not None]
        est_y = [pos['y_m'] for pos in gama_positions.values() if pos['y_m'] is not None]
        label = f'Gama (RMSE: {gama_rmse:.3f} m)' if gama_rmse else 'Gama'
        ax.scatter(est_x, est_y, c='cyan', marker='o', s=80, label=label, zorder=4)
        
        # Draw error lines from true to estimated
        for node_id, est_pos in gama_positions.items():
            if est_pos['x_m'] is not None and node_id in true_positions:
                true_pos = true_positions[node_id]
                ax.plot([true_pos['x_m'], est_pos['x_m']], 
                       [true_pos['y_m'], est_pos['y_m']], 
                       'c--', alpha=0.5, linewidth=0.8)
    
    ax.set_xlabel('X (m)')
    ax.set_ylabel('Y (m)')
    ax.set_aspect('equal', adjustable='box')
    ax.legend(loc='upper left', bbox_to_anchor=(1, 1))
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.draw()


def compute_rmse(true_positions: dict, estimated_positions: dict) -> float:
    """Compute RMSE between true and estimated positions."""
    errors = []
    for node_id, est_pos in estimated_positions.items():
        if node_id in true_positions and est_pos['x_m'] is not None:
            true_pos = true_positions[node_id]
            error = np.sqrt((true_pos['x_m'] - est_pos['x_m'])**2 + 
                          (true_pos['y_m'] - est_pos['y_m'])**2)
            errors.append(error)
    
    if errors:
        return np.sqrt(np.mean(np.array(errors)**2))
    return None


def main():
    parser = argparse.ArgumentParser(description='Replay SS-TWR ranging data for localization')
    parser.add_argument('--sstwr', type=str, default=DEFAULT_SSTWR_CSV,
                       help='Path to sstwr.csv file')
    parser.add_argument('--positions', type=str, default=DEFAULT_POSITIONS_CSV,
                       help='Path to positions CSV file')
    parser.add_argument('--port', type=str, default=DEFAULT_PORT,
                       help='Serial port for Flock device')
    parser.add_argument('--baudrate', type=int, default=DEFAULT_BAUDRATE,
                       help='Serial baudrate')
    parser.add_argument('--timeout', type=int, default=DEFAULT_TIMEOUT,
                       help='Serial timeout in seconds')
    parser.add_argument('--known-nodes', type=int, nargs='+', default=DEFAULT_KNOWN_NODES,
                       help='Node IDs to use as known positions (anchors)')
    parser.add_argument('--distance-column', type=str, default='dist',
                       choices=['dist', 'dist_bias', 'dist_drift', 'dist_drift_bias'],
                       help='Distance column to use from sstwr.csv')
    parser.add_argument('--output', type=str, default=None,
                       help='Output CSV file for results (default: results/replay_results.csv)')
    parser.add_argument('--no-plot', action='store_true',
                       help='Disable plotting results')
    parser.add_argument('--debug', action='store_true',
                       help='Enable debug logging')
    
    args = parser.parse_args()
    
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Set default output path
    if args.output is None:
        args.output = os.path.join(SCRIPT_DIR, 'results/replay_results.csv')
    
    # Load data
    ranging_df = load_ranging_data(args.sstwr, args.distance_column)
    positions = load_positions(args.positions)
    
    # Get known positions
    known_positions = get_known_positions(positions, args.known_nodes)
    
    if len(known_positions) < 3:
        log.error("Need at least 3 known positions for localization")
        return
    
    # Connect to Flock device
    log.info(f"Connecting to Flock device on {args.port}")
    flock_loc = FlockLoc(port=args.port, baudrate=args.baudrate, timeout=args.timeout)
    
    # Initialize MDS localization
    log.info("Initializing MDS localization")
    mds_loc = MDSLoc(n_init=256)

    # Initialize GNU Gama localization
    log.info("Initializing GNU Gama localization")
    gama_loc = GNUGamaLoc()
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    
    # Results storage
    results = []
    
    log.info(f"Processing ranging measurements by rounds (one measurement per node-pair per round)...")
    
    # Enable interactive plotting
    if not args.no_plot:
        plt.ion()
        os.makedirs(os.path.join(SCRIPT_DIR, 'fig'), exist_ok=True)
    
    # Iterate through measurements round by round
    total_rounds = None
    for round_idx, seqn, ts, measurements_this_round, range_measurements in iterate_range_measurements(ranging_df, args.distance_column):
        if total_rounds is None:
            # Get total rounds from iterator info logged earlier
            total_rounds = "?"
        
        # Clear previous data
#        flock_loc.clear()
#        mds_loc.clear()
        
        # Run Flock localization
        flock_positions, flock_diag = flock_loc.localize(
            range_measurements=range_measurements,
            known_positions=known_positions
        )
        
        # Run MDS localization
        mds_positions, mds_diag = mds_loc.localize(
            range_measurements=range_measurements,
            known_positions=known_positions
        )
        
        # Run GNU Gama localization
        gama_positions, gama_diag = gama_loc.localize(
            range_measurements=range_measurements,
            known_positions=known_positions
        )
        
        # Compute RMSE
        flock_rmse = compute_rmse(positions, flock_positions) if flock_positions else None
        mds_rmse = compute_rmse(positions, mds_positions) if mds_positions else None
        gama_rmse = compute_rmse(positions, gama_positions) if gama_positions else None
        
        # Build result record
        result = {
            'round': round_idx,
            'seqn': seqn,
            'timestamp': ts,
            'measurements_this_round': measurements_this_round,
            'num_distances': len(range_measurements),
            'flock_rmse_m': flock_rmse,
            'flock_runtime_us': flock_diag.runtime_us if flock_diag else None,
            'flock_error_mm': flock_diag.error_mm if flock_diag else None,
            'flock_success': flock_diag.success if flock_diag else False,
            'mds_rmse_m': mds_rmse,
            'mds_runtime_us': mds_diag.runtime_us if mds_diag else None,
            'mds_error_mm': mds_diag.error_mm if mds_diag else None,
            'gama_rmse_m': gama_rmse,
            'gama_runtime_us': gama_diag.runtime_us if gama_diag else None,
            'gama_error_mm': gama_diag.error_mm if gama_diag else None,
        }
        
        # Add per-node errors for Flock
        if flock_positions:
            for node_id, est_pos in flock_positions.items():
                if node_id in positions and est_pos['x_m'] is not None:
                    true_pos = positions[node_id]
                    error = np.sqrt((true_pos['x_m'] - est_pos['x_m'])**2 + 
                                  (true_pos['y_m'] - est_pos['y_m'])**2)
                    result[f'flock_node_{node_id}_error_m'] = error
                    result[f'flock_node_{node_id}_x_m'] = est_pos['x_m']
                    result[f'flock_node_{node_id}_y_m'] = est_pos['y_m']
        
        # Add per-node errors for MDS
        if mds_positions:
            for node_id, est_pos in mds_positions.items():
                if node_id in positions and est_pos['x_m'] is not None:
                    true_pos = positions[node_id]
                    error = np.sqrt((true_pos['x_m'] - est_pos['x_m'])**2 + 
                                  (true_pos['y_m'] - est_pos['y_m'])**2)
                    result[f'mds_node_{node_id}_error_m'] = error
                    result[f'mds_node_{node_id}_x_m'] = est_pos['x_m']
                    result[f'mds_node_{node_id}_y_m'] = est_pos['y_m']
        
        # Add per-node errors for Gama
        if gama_positions:
            for node_id, est_pos in gama_positions.items():
                if node_id in positions and est_pos['x_m'] is not None:
                    true_pos = positions[node_id]
                    error = np.sqrt((true_pos['x_m'] - est_pos['x_m'])**2 + 
                                  (true_pos['y_m'] - est_pos['y_m'])**2)
                    result[f'gama_node_{node_id}_error_m'] = error
                    result[f'gama_node_{node_id}_x_m'] = est_pos['x_m']
                    result[f'gama_node_{node_id}_y_m'] = est_pos['y_m']
        
        results.append(result)
        
        # Update plot every round
        if not args.no_plot:
            plot_results(positions, args.known_nodes, 
                        flock_positions=flock_positions, flock_rmse=flock_rmse,
                        mds_positions=mds_positions, mds_rmse=mds_rmse,
                        gama_positions=gama_positions, gama_rmse=gama_rmse,
                        title=f"Round {round_idx}: {len(range_measurements)} distances")
            plt.pause(0.01)
        
        # Progress update every 10 rounds
        if (round_idx + 1) % 10 == 0:
            log.info(f"Round {round_idx + 1}: {len(range_measurements)} distances "
                    f"(Flock: {flock_rmse:.3f} m, MDS: {mds_rmse:.3f} m, Gama: {gama_rmse:.3f} m)" 
                    if flock_rmse and mds_rmse and gama_rmse else f"Round {round_idx + 1}: {len(range_measurements)} distances")
            
            # Save intermediate results
            df_results = pd.DataFrame(results)
            df_results.to_csv(args.output, index=False)
    
    # Save final results
    df_results = pd.DataFrame(results)
    df_results.to_csv(args.output, index=False)
    log.info(f"Results saved to {args.output}")
    
    # Summary statistics
    log.info("\n" + "="*50)
    log.info("SUMMARY STATISTICS")
    log.info("="*50)
    
    flock_rmse_vals = df_results['flock_rmse_m'].dropna()
    mds_rmse_vals = df_results['mds_rmse_m'].dropna()
    gama_rmse_vals = df_results['gama_rmse_m'].dropna()
    
    if len(flock_rmse_vals) > 0:
        log.info(f"Flock RMSE: mean={flock_rmse_vals.mean():.3f} m, "
                f"std={flock_rmse_vals.std():.3f} m, "
                f"min={flock_rmse_vals.min():.3f} m, "
                f"max={flock_rmse_vals.max():.3f} m")
    
    if len(mds_rmse_vals) > 0:
        log.info(f"MDS RMSE:   mean={mds_rmse_vals.mean():.3f} m, "
                f"std={mds_rmse_vals.std():.3f} m, "
                f"min={mds_rmse_vals.min():.3f} m, "
                f"max={mds_rmse_vals.max():.3f} m")
    
    if len(gama_rmse_vals) > 0:
        log.info(f"Gama RMSE:  mean={gama_rmse_vals.mean():.3f} m, "
                f"std={gama_rmse_vals.std():.3f} m, "
                f"min={gama_rmse_vals.min():.3f} m, "
                f"max={gama_rmse_vals.max():.3f} m")
    
    # Plot final results
    if not args.no_plot and len(results) > 0:
        plt.ioff()  # Disable interactive mode for final plots
        os.makedirs(os.path.join(SCRIPT_DIR, 'fig'), exist_ok=True)
        
        # Get last localization results for plotting
        last_flock_positions = None
        last_mds_positions = None
        last_gama_positions = None
        
        if flock_positions:
            last_flock_positions = flock_positions
        if mds_positions:
            last_mds_positions = mds_positions
        if gama_positions:
            last_gama_positions = gama_positions
        
        # Save final state plot
        plot_results(positions, args.known_nodes, 
                    flock_positions=last_flock_positions, flock_rmse=flock_rmse,
                    mds_positions=last_mds_positions, mds_rmse=mds_rmse,
                    gama_positions=last_gama_positions, gama_rmse=gama_rmse,
                    title="Replay Localization (Final State)")
        plt.savefig(os.path.join(SCRIPT_DIR, 'fig/replay_results.png'), bbox_inches='tight', dpi=150)
        
        # Plot RMSE over time
        plt.figure(figsize=(12, 6))
        plt.subplot(1, 2, 1)
        plt.plot(df_results['round'], df_results['flock_rmse_m'], 'b-', alpha=0.7, label='Flock')
        plt.plot(df_results['round'], df_results['mds_rmse_m'], 'r-', alpha=0.7, label='MDS')
        plt.plot(df_results['round'], df_results['gama_rmse_m'], 'c-', alpha=0.7, label='Gama')
        plt.xlabel('Round')
        plt.ylabel('RMSE (m)')
        plt.title('Localization RMSE over Rounds')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        plt.subplot(1, 2, 2)
        plt.plot(df_results['round'], df_results['num_distances'], 'g-', alpha=0.7)
        plt.xlabel('Round')
        plt.ylabel('Number of Distance Measurements')
        plt.title('Distance Measurements Accumulated')
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(os.path.join(SCRIPT_DIR, 'fig/replay_rmse_over_time.png'), dpi=150)
        plt.show()
    
    # Clean up
    flock_loc.clear()
    mds_loc.clear()
    gama_loc.clear()


if __name__ == '__main__':
    main()
