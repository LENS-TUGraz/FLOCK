from Scenario import Scenario
from localization import FlockLoc, LocalizationEngineDiagnostics
from localization import Dummy
from localization import MDSLoc
from localization import BFGSLoc
from localization import GNUGamaLoc
from matplotlib import pyplot as plt
import numpy as np
import pandas as pd
import json
from tqdm import tqdm

import logging

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

#%% Configuration Parameters

SEED = 42
REPETITIONS = 10
SCENARIOS = 25

SCENARIO_AREAS = [10]  # Square areas: 10x10, 20x20, etc.
#SCENARIO_NUM_NODES = [15, 30, 45]
SCENARIO_NUM_NODES = [15, 30, 45]
SCENARIO_KNOWN_POSITIONS = 4
SCENARIO_KNOWN_POSITIONS_PLACEMENT = 'corner'

RANGING_OFFSET_STD_M = 0.0
RANGING_OFFSET_MEAN_M = -0.0377
RANGING_NOISE_STD_M = 0.1368
RANGING_MAX_DISTANCE_M = 15.0

POSITION_NOISE_STD_M = 0.0

#%% Helper Functions

def kabsch_fit(A: np.ndarray, B: np.ndarray):
    """
    Returns the optimal rotation matrix R and translation vector t
    that aligns A to B (minimizing least squares error).
    A, B: Nx2 numpy arrays (corresponding points)
    """

    centroid_A = np.mean(A, axis=0)
    centroid_B = np.mean(B, axis=0)
    AA = A - centroid_A
    BB = B - centroid_B
    H = AA.T @ BB
    U, S, Vt = np.linalg.svd(H)
    R = Vt.T @ U.T

#    # Avoid reflections
#    if np.linalg.det(R) < 0:
#        Vt[1,:] *= -1
#        R = Vt.T @ U.T

    t = centroid_B - R @ centroid_A
    return R, t

def kabsch_fit_dict(true_positions: dict, estimated_positions: dict):
  """
  Align estimated_positions to true_positions using Kabsch algorithm.
  Both inputs are dicts of node_id -> (x_m, y_m)
  Returns a new dict of aligned estimated positions.
  """
  common_ids = set(true_positions.keys()).intersection(set(estimated_positions.keys()))
  if len(common_ids) < 2:
      log.warning("Not enough common points for Kabsch alignment.")
      log.warning(f"Common IDs: {common_ids}")
      return estimated_positions  # Cannot align with less than 2 points

  A = np.array([ (estimated_positions[node_id]['x_m'], estimated_positions[node_id]['y_m']) for node_id in common_ids])
  B = np.array([ (true_positions[node_id]['x_m'], true_positions[node_id]['y_m']) for node_id in common_ids])

  R, t = kabsch_fit(A, B)

  ret = {}
  for node_id in estimated_positions:
      est_pos = np.array([estimated_positions[node_id]['x_m'], estimated_positions[node_id]['y_m']])
      aligned_pos = R @ est_pos + t
      ret[node_id] = {'x_m': aligned_pos[0], 'y_m': aligned_pos[1]}

  return ret

def plot_results(scenario: Scenario, 
                 flock_results: dict=None, flock_diag: LocalizationEngineDiagnostics=None, 
                 mds_results: dict=None, mds_diag: LocalizationEngineDiagnostics=None,
                 mds16_results: dict=None, mds16_diag: LocalizationEngineDiagnostics=None,
                 mds128_results: dict=None, mds128_diag: LocalizationEngineDiagnostics=None,
                 bfgs_results: dict=None, bfgs_diag: LocalizationEngineDiagnostics=None,
                 gama_results: dict=None, gama_diag: LocalizationEngineDiagnostics=None,
                 repetition: int=0):
    plt.clf()
    fig, ax = plt.subplots(num=1)
    ax.set_title(f"Scenario: {scenario.name} Repetition: {repetition}")
    ax.set_xlim(0, scenario.area_x_m)
    ax.set_ylim(0, scenario.area_y_m)

    # Plot known positions
    known_x = [scenario.get_position_dict()[str(pos.id)]['x_m'] for pos in scenario.positions if pos.known]
    known_y = [scenario.get_position_dict()[str(pos.id)]['y_m'] for pos in scenario.positions if pos.known]
    ax.scatter(known_x, known_y, c='orange', marker='s', s=150, label='Known Positions')

    # Plot true positions
    true_x = [pos.x_m for pos in scenario.positions]
    true_y = [pos.y_m for pos in scenario.positions]
    ax.scatter(true_x, true_y, c='green', s=100 ,label='True Positions')

    # Plot MDS results
    if mds_results is not None:
      mds_ids = [str(pos.id) for pos in scenario.positions if str(pos.id) in mds_results]
      mds_pts = [(mds_results[i]['x_m'], mds_results[i]['y_m']) for i in mds_ids if mds_results[i]['x_m'] is not None and mds_results[i]['y_m'] is not None]
      mds_x = [p[0] for p in mds_pts]
      mds_y = [p[1] for p in mds_pts]
      ax.scatter(mds_x, mds_y, c='red', marker='^', label=f'MDS Estimated Positions {mds_diag.runtime_us:.2f} us {mds_diag.error_mm:.2f} mm')
      
      # Draw dashed lines from true positions to MDS estimates
      for pos in scenario.positions:
        if str(pos.id) in mds_results:
          ax.plot([pos.x_m, mds_results[str(pos.id)]['x_m']], 
                  [pos.y_m, mds_results[str(pos.id)]['y_m']], 
                  'r--', alpha=0.5, linewidth=0.8)
    # Plot MDS16 results
    if mds16_results is not None:
      mds16_ids = [str(pos.id) for pos in scenario.positions if str(pos.id) in mds16_results]
      mds16_pts = [(mds16_results[i]['x_m'], mds16_results[i]['y_m']) for i in mds16_ids if mds16_results[i]['x_m'] is not None and mds16_results[i]['y_m'] is not None]
      mds16_x = [p[0] for p in mds16_pts]
      mds16_y = [p[1] for p in mds16_pts]
      ax.scatter(mds16_x, mds16_y, c='magenta', marker='^', label=f'MDS16 Estimated Positions {mds16_diag.runtime_us:.2f} us {mds16_diag.error_mm:.2f} mm')
      
      # Draw dashed lines from true positions to MDS16 estimates
      for pos in scenario.positions:
        if str(pos.id) in mds16_results:
          ax.plot([pos.x_m, mds16_results[str(pos.id)]['x_m']], 
                  [pos.y_m, mds16_results[str(pos.id)]['y_m']], 
                  'magenta', linestyle='--', alpha=0.5, linewidth=0.8)

    # Plot MDS128 results
    if mds128_results is not None:
      mds128_ids = [str(pos.id) for pos in scenario.positions if str(pos.id) in mds128_results]
      mds128_pts = [(mds128_results[i]['x_m'], mds128_results[i]['y_m']) for i in mds128_ids if mds128_results[i]['x_m'] is not None and mds128_results[i]['y_m'] is not None]
      mds128_x = [p[0] for p in mds128_pts]
      mds128_y = [p[1] for p in mds128_pts]
      ax.scatter(mds128_x, mds128_y, c='olive', marker='^', label=f'MDS128 Estimated Positions {mds128_diag.runtime_us:.2f} us {mds128_diag.error_mm:.2f} mm')
      
      # Draw dashed lines from true positions to MDS128 estimates
      for pos in scenario.positions:
        if str(pos.id) in mds128_results:
          ax.plot([pos.x_m, mds128_results[str(pos.id)]['x_m']], 
                  [pos.y_m, mds128_results[str(pos.id)]['y_m']], 
                  'olive', linestyle='--', alpha=0.5, linewidth=0.8)
          
    # Plot BFGS results
    if bfgs_results is not None:
      bfgs_ids = [str(pos.id) for pos in scenario.positions if str(pos.id) in bfgs_results]
      bfgs_pts = [(bfgs_results[i]['x_m'], bfgs_results[i]['y_m']) for i in bfgs_ids if bfgs_results[i]['x_m'] is not None and bfgs_results[i]['y_m'] is not None]
      bfgs_x = [p[0] for p in bfgs_pts]
      bfgs_y = [p[1] for p in bfgs_pts]
      ax.scatter(bfgs_x, bfgs_y, c='purple', marker='D', label=f'BFGS Estimated Positions {bfgs_diag.runtime_us:.2f} us {bfgs_diag.error_mm:.2f} mm')
      
      # Draw dashed lines from true positions to BFGS estimates
      for pos in scenario.positions:
        if str(pos.id) in bfgs_results:
          ax.plot([pos.x_m, bfgs_results[str(pos.id)]['x_m']], 
                  [pos.y_m, bfgs_results[str(pos.id)]['y_m']], 
                  'purple', linestyle='--', alpha=0.5, linewidth=0.8)
          
    # Plot Gama results
    if gama_results is not None:
      gama_ids = [str(pos.id) for pos in scenario.positions if str(pos.id) in gama_results]
      gama_pts = [(gama_results[i]['x_m'], gama_results[i]['y_m']) for i in gama_ids if gama_results[i]['x_m'] is not None and gama_results[i]['y_m'] is not None]
      gama_x = [p[0] for p in gama_pts]
      gama_y = [p[1] for p in gama_pts]
      ax.scatter(gama_x, gama_y, c='cyan', marker='o', label=f'Gama Estimated Positions {gama_diag.runtime_us:.2f} us {gama_diag.error_mm:.2f} mm')
      
      # Draw dashed lines from true positions to Gama estimates
      for pos in scenario.positions:
        if str(pos.id) in gama_results:
          ax.plot([pos.x_m, gama_results[str(pos.id)]['x_m']], 
                  [pos.y_m, gama_results[str(pos.id)]['y_m']], 
                  'cyan', linestyle='--', alpha=0.5, linewidth=0.8)
          
    # Plot Flock results
    if flock_results is not None:
      flock_ids = [str(pos.id) for pos in scenario.positions if str(pos.id) in flock_results]
      flock_pts = [(flock_results[i]['x_m'], flock_results[i]['y_m']) for i in flock_ids if flock_results[i]['x_m'] is not None and flock_results[i]['y_m'] is not None]
      flock_x = [p[0] for p in flock_pts]
      flock_y = [p[1] for p in flock_pts]
      if flock_diag is not None and flock_diag.success:
        ax.scatter(flock_x, flock_y, c='blue', marker='x', label=f'Flock Estimated Positions {flock_diag.runtime_us:.2f} us {flock_diag.error_mm:.2f} mm')
      else:
        ax.scatter(flock_x, flock_y, c='red', marker='x', label=f'Flock Estimated Positions {flock_diag.runtime_us:.2f} us {flock_diag.error_mm:.2f} mm (FAILED)')
      
      # Draw dashed lines from true positions to Flock estimates
      for pos in scenario.positions:
        if str(pos.id) in flock_results:
          ax.plot([pos.x_m, flock_results[str(pos.id)]['x_m']], 
                  [pos.y_m, flock_results[str(pos.id)]['y_m']], 
                  'b--', alpha=0.5, linewidth=0.8)
    
    # Plot a dashed grey line between true positions with a distance less than max ranging distance
    for i in range(len(scenario.positions)):
      for j in range(i+1, len(scenario.positions)):
        pos_i = scenario.positions[i]
        pos_j = scenario.positions[j]
        dist_ij = np.sqrt((pos_i.x_m - pos_j.x_m)**2 + (pos_i.y_m - pos_j.y_m)**2)
        if dist_ij <= RANGING_MAX_DISTANCE_M:
          ax.plot([pos_i.x_m, pos_j.x_m], [pos_i.y_m, pos_j.y_m], 'grey', linestyle='--', alpha=0.3, linewidth=0.5)
        if dist_ij > RANGING_MAX_DISTANCE_M:
          ax.plot([pos_i.x_m, pos_j.x_m], [pos_i.y_m, pos_j.y_m], 'orange', linestyle='--', alpha=0.1, linewidth=0.5)

    ax.set_aspect('equal', adjustable='box')
    ax.legend(loc='upper left', bbox_to_anchor=(1, 1))
    plt.savefig(f"fig/localization_results_scenario_{scenario.name}_repetition_{repetition}.png", bbox_inches='tight')
    plt.pause(0.01)



def eval_results(scenario: Scenario, ranging_noise_std_m: float, ranging_offset_m: float, ranging_max_distance_m: float, flock_results: dict=None, mds_results: dict=None, mds16_results: dict=None, mds128_results: dict=None, bfgs_results: dict=None, gama_results: dict=None):
    def compute_rmse(true_positions: dict, estimated_positions: dict):
        common_ids = set(true_positions.keys()).intersection(set(estimated_positions.keys()))
        if len(common_ids) == 0:
            return None
        errors = []
        for node_id in common_ids:
            true_pos = true_positions[node_id]
            est_pos = estimated_positions[node_id]
            if est_pos['x_m'] is not None and est_pos['y_m'] is not None:
                error = np.sqrt((true_pos['x_m'] - est_pos['x_m'])**2 + (true_pos['y_m'] - est_pos['y_m'])**2)
                errors.append(error)
        if len(errors) == 0:
            return None
        rmse = np.sqrt(np.mean(np.array(errors)**2))
        return rmse
    
    def compute_errors(true_positions: dict, estimated_positions: dict):
        common_ids = set(true_positions.keys()).intersection(set(estimated_positions.keys()))
        errors = {}
        for node_id in common_ids:
            true_pos = true_positions[node_id]
            est_pos = estimated_positions[node_id]
            if est_pos['x_m'] is not None and est_pos['y_m'] is not None:
                error = np.sqrt((true_pos['x_m'] - est_pos['x_m'])**2 + (true_pos['y_m'] - est_pos['y_m'])**2)
                errors[node_id] = float(error)
        return errors

    rec = {}

    true_positions = scenario.get_position_dict()

    flock_rmse = compute_rmse(true_positions, flock_results) if flock_results is not None else None
    mds_rmse = compute_rmse(true_positions, mds_results) if mds_results is not None else None
    mds16_rmse = compute_rmse(true_positions, mds16_results) if mds16_results is not None else None
    mds128_rmse = compute_rmse(true_positions, mds128_results) if mds128_results is not None else None
    bfgs_rmse = compute_rmse(true_positions, bfgs_results) if bfgs_results is not None else None
    gama_rmse = compute_rmse(true_positions, gama_results) if gama_results is not None else None

    log.debug(f"RMSE Results for Scenario {scenario.name}:")
    log.debug(f"  Flock RMSE: {flock_rmse:.3f} m" if flock_rmse is not None else "  Flock RMSE: N/A")
    log.debug(f"  MDS RMSE: {mds_rmse:.3f} m" if mds_rmse is not None else "  MDS RMSE: N/A")
    log.debug(f"  BFGS RMSE: {bfgs_rmse:.3f} m" if bfgs_rmse is not None else "  BFGS RMSE: N/A")
    log.debug(f"  Gama RMSE: {gama_rmse:.3f} m" if gama_rmse is not None else "  Gama RMSE: N/A")

    flock_errors = compute_errors(true_positions, flock_results) if flock_results is not None else {}
    mds_errors = compute_errors(true_positions, mds_results) if mds_results is not None else {}
    mds16_errors = compute_errors(true_positions, mds16_results) if mds16_results is not None else {}
    mds128_errors = compute_errors(true_positions, mds128_results) if mds128_results is not None else {}
    bfgs_errors = compute_errors(true_positions, bfgs_results) if bfgs_results is not None else {}
    gama_errors = compute_errors(true_positions, gama_results) if gama_results is not None else {}


    rec['scenario'] = scenario.name
    rec['ranging_noise_std_m'] = ranging_noise_std_m
    rec['ranging_offset_m'] = ranging_offset_m
    rec['ranging_max_distance_m'] = ranging_max_distance_m
    rec['flock_rmse'] = flock_rmse
    rec['mds_rmse'] = mds_rmse
    rec['mds16_rmse'] = mds16_rmse
    rec['mds128_rmse'] = mds128_rmse
    rec['bfgs_rmse'] = bfgs_rmse
    rec['gama_rmse'] = gama_rmse
    rec['flock_errors'] = json.dumps(flock_errors)
    rec['mds_errors'] = json.dumps(mds_errors)
    rec['mds16_errors'] = json.dumps(mds16_errors)
    rec['mds128_errors'] = json.dumps(mds128_errors)
    rec['bfgs_errors'] = json.dumps(bfgs_errors)
    rec['gama_errors'] = json.dumps(gama_errors)

    return rec



#%% Main Simulation Loop
flock_loc = Dummy()
mds_loc = Dummy()
bfgs_loc = Dummy()
gama_loc = Dummy()

flock_loc = FlockLoc(port='/dev/ttyACM1', baudrate=230400, timeout=10)
#flock_loc = FlockLoc(port='/dev/ttyACM0', baudrate=115200, timeout=1)
mds_loc = MDSLoc()
mds16_loc = MDSLoc(n_init=16)
mds128_loc = MDSLoc(n_init=128)
bfgs_loc = BFGSLoc()
gama_loc = GNUGamaLoc()

scenarios = []
results = []

total_iterations = len(SCENARIO_NUM_NODES) * len(SCENARIO_AREAS) * SCENARIOS * REPETITIONS
pbar = tqdm(total=total_iterations, desc="Simulation Progress", unit="iter")

for num_nodes in SCENARIO_NUM_NODES:
  for area_size in SCENARIO_AREAS:
    pbar.set_description(f"Nodes={num_nodes}, Area={area_size}x{area_size}")
  
    for i in range(SCENARIOS):
      s = Scenario.generate_random(f"scenario_{num_nodes}_X{area_size}_Y{area_size}_k{SCENARIO_KNOWN_POSITIONS}_{i}", 
                              num_nodes=num_nodes, 
                              area_x_m=area_size, area_y_m=area_size, 
                              known_positions=SCENARIO_KNOWN_POSITIONS, known_positions_placement=SCENARIO_KNOWN_POSITIONS_PLACEMENT,
                              seed=SEED)
      scenarios.append(s)
      Scenario.save_scenarios(scenarios, f"scenarios/scenarios_{num_nodes}_X{area_size}_Y{area_size}_k{SCENARIO_KNOWN_POSITIONS}.json")

      for r in range(REPETITIONS):
        pbar.set_postfix(scenario=i, rep=r)

        ranging_offset_m = np.random.normal(RANGING_OFFSET_MEAN_M, RANGING_OFFSET_STD_M)

#        flock_loc.clear()
        mds_loc.clear()
        mds16_loc.clear()
        mds128_loc.clear()
        bfgs_loc.clear()
        gama_loc.clear()

        range_measurements = s.generate_distances(noise_std_m=RANGING_NOISE_STD_M, noise_offset_m=ranging_offset_m, max_distance_m=RANGING_MAX_DISTANCE_M)
        known_positions = s.generate_known_positions(noise_std_m=POSITION_NOISE_STD_M)

        try:
          flock_results, flock_diag = flock_loc.localize(
              range_measurements=range_measurements,
              known_positions=known_positions)
          mds_results, mds_diag = mds_loc.localize(
              range_measurements=range_measurements,
              known_positions=known_positions)
          mds16_results, mds16_diag = mds16_loc.localize(
              range_measurements=range_measurements,
              known_positions=known_positions)
          mds128_results, mds128_diag = mds128_loc.localize(
              range_measurements=range_measurements,
              known_positions=known_positions)
          bfgs_results, bfgs_diag = bfgs_loc.localize(
              range_measurements=range_measurements,
              known_positions=known_positions)
          gama_results, gama_diag = gama_loc.localize(
              range_measurements=range_measurements,
              known_positions=known_positions)
        except Exception as e:
          log.error(f"Error during localization for scenario {s.name} repetition {r}: {e}")
          flock_results, flock_diag = None, None
          mds_results, mds_diag = None, None
          mds16_results, mds16_diag = None, None
          mds128_results, mds128_diag = None, None
          bfgs_results, bfgs_diag = None, None
          gama_results, gama_diag = None, None

        if len(known_positions) == 0:
          if flock_results is not None:
            flock_results = kabsch_fit_dict(s.get_position_dict(), flock_results)
          if mds_results is not None:
            mds_results = kabsch_fit_dict(s.get_position_dict(), mds_results)

        plot_results(s, flock_results, flock_diag, 
                     mds_results, mds_diag,
                     mds16_results, mds16_diag,
                     mds128_results, mds128_diag,
                     bfgs_results, bfgs_diag,
                     gama_results, gama_diag,
                     repetition=r) 

        res = eval_results(s, ranging_offset_m, RANGING_MAX_DISTANCE_M, RANGING_MAX_DISTANCE_M, flock_results, mds_results, mds16_results, mds128_results, bfgs_results, gama_results)

        res['flock_runtime_us'] = flock_diag.runtime_us if flock_diag is not None else None
        res['mds_runtime_us'] = mds_diag.runtime_us if mds_diag is not None else None
        res['mds16_runtime_us'] = mds16_diag.runtime_us if mds16_diag is not None else None
        res['mds128_runtime_us'] = mds128_diag.runtime_us if mds128_diag is not None else None
        res['bfgs_runtime_us'] = bfgs_diag.runtime_us if bfgs_diag is not None else None
        res['gama_runtime_us'] = gama_diag.runtime_us if gama_diag is not None else None
        res['flock_diagnostics'] = json.dumps(flock_diag.__dict__) if flock_diag is not None else None

        log.debug(f"Runtimes (us): Flock: {res['flock_runtime_us']}, MDS: {res['mds_runtime_us']}, MDS16: {res['mds16_runtime_us']}, MDS128: {res['mds128_runtime_us']}, BFGS: {res['bfgs_runtime_us']}, Gama: {res['gama_runtime_us']}")

        res['repetition'] = r
        res['area_size'] = area_size
        res['num_nodes'] = num_nodes
        results.append(res)

        # Save intermediate results
        df = pd.DataFrame(results)
        df.to_csv(f"results/simulation_results.csv", index=False)
        pbar.update(1)

      flock_loc.clear()
      mds_loc.clear()
      mds16_loc.clear()
      mds128_loc.clear()
      bfgs_loc.clear()
      gama_loc.clear()
      SEED += 1

pbar.close()