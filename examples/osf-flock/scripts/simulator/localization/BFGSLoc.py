from localization.LocalizationEngine import LocalizationEngine, LocalizationEngineDiagnostics
import numpy as np
import logging
from scipy.optimize import minimize
import time


log = logging.getLogger(__name__)

class BFGSLoc(LocalizationEngine):
  def __init__(self):
    """Initialize the weighted MDS LocalizationEngine"""
    self.estimated_positions = {}
    self.known_node_ids = set()

  def localize(self, range_measurements: dict, known_positions: dict) -> tuple[dict, LocalizationEngineDiagnostics]:
    """
    Localize nodes using weighted Multi-Dimensional Scaling (MDS).
    
    Args:
      range_measurements: dict with (from_id, to_id) tuple keys and distance values
      known_positions: dict with node_id keys and (x, y) position tuples
    
    Returns:
      tuple: (estimated_positions_dict, diagnostics_dict)
    """
    # Extract node IDs from measurements and known positions
    start_time = time.time_ns()

    node_ids = set()
    for from_id, to_id in range_measurements.keys():
      node_ids.add(from_id)
      node_ids.add(to_id)
    
    self.known_node_ids = set(known_positions.keys())
    unknown_node_ids = node_ids - self.known_node_ids
    
    if not node_ids:
      log.warning("No nodes found in range measurements")
      return {}, {'error': 'No nodes in measurements'}
    
    if not unknown_node_ids:
      log.info("All nodes have known positions")
      return known_positions, {'message': 'All nodes localized'}
    
    # Build distance matrix
    sorted_ids = sorted(list(node_ids))
    node_id_to_idx = {nid: idx for idx, nid in enumerate(sorted_ids)}
    n_nodes = len(sorted_ids)
    
    distance_matrix = np.full((n_nodes, n_nodes), np.inf)
    np.fill_diagonal(distance_matrix, 0)
    
    for (from_id, to_id), distance in range_measurements.items():
      from_idx = node_id_to_idx[from_id]
      to_idx = node_id_to_idx[to_id]
      distance_matrix[from_idx, to_idx] = distance
      distance_matrix[to_idx, from_idx] = distance
    
    # Set up initial positions
    positions = np.zeros((n_nodes, 2))
    
    # Place known positions
    for node_id, (x, y) in known_positions.items():
      idx = node_id_to_idx[node_id]
      positions[idx] = [x, y]
    
    # Initialize unknown positions with random placement near centroid of known positions
    if self.known_node_ids:
      known_positions_array = np.array([known_positions[nid] for nid in self.known_node_ids])
      centroid = np.mean(known_positions_array, axis=0)
    else:
      centroid = np.array([0, 0])
    
    for node_id in unknown_node_ids:
      idx = node_id_to_idx[node_id]
      positions[idx] = centroid + np.random.randn(2) * 0.1
    
    # Optimize positions using weighted least squares
    positions = self._optimize_positions(
      positions, distance_matrix, node_id_to_idx, 
      self.known_node_ids, unknown_node_ids
    )
    
    # Build result dictionary
    estimated_positions = {}
    for node_id, idx in node_id_to_idx.items():
      estimated_positions[str(node_id)] = {
        'x_m': float(positions[idx, 0]),
        'y_m': float(positions[idx, 1])
      }
    
    self.estimated_positions = estimated_positions
    
    end_time = time.time_ns()
    elapsed_time = end_time - start_time
    diagnostics = LocalizationEngineDiagnostics(runtime_us=elapsed_time / 1e3)
    
    log.info(f"Localization complete. Estimated {len(unknown_node_ids)} positions")
  

    return estimated_positions, diagnostics

  def _optimize_positions(self, positions, distance_matrix, node_id_to_idx, 
                          known_node_ids, unknown_node_ids):
    """
    Optimize node positions using weighted least squares minimization.
    Known positions are kept fixed.
    """
    # Create index list for unknown nodes
    unknown_indices = [node_id_to_idx[nid] for nid in unknown_node_ids]
    
    def objective(x_flat):
      """Objective function: weighted sum of squared distance errors"""
      # Update positions with optimized values
      x_opt = positions.copy()
      x_opt[unknown_indices] = x_flat.reshape(-1, 2)
      
      total_error = 0
      valid_measurements = 0
      
      # Calculate weighted error for each measurement
      for i in range(len(positions)):
        for j in range(i + 1, len(positions)):
          measured_dist = distance_matrix[i, j]
          
          # Skip if no measurement
          if np.isinf(measured_dist):
            continue
          
          # Calculate Euclidean distance
          estimated_dist = np.linalg.norm(x_opt[i] - x_opt[j])
          
#          # Weight measurements involving known positions more heavily
#          weight = 1.0
#          if i < len(positions) and j < len(positions):
#            # Find corresponding node IDs
#            node_i_id = next(nid for nid, idx in node_id_to_idx.items() if idx == i)
#            node_j_id = next(nid for nid, idx in node_id_to_idx.items() if idx == j)
#            
#            # Higher weight for measurements between known positions
#            known_count = (node_i_id in known_node_ids) + (node_j_id in known_node_ids)
#            weight = 1.0 + 0.5 * known_count
          
#          # Squared error weighted
#          error = weight * (estimated_dist - measured_dist) ** 2
          error = (estimated_dist - measured_dist) ** 2
          total_error += error
          valid_measurements += 1
      
      return total_error
    
    # Initial guess for unknown positions
    x0 = positions[unknown_indices].flatten()
    
    # Optimize
    if len(unknown_indices) > 0:
      result = minimize(objective, x0, method='BFGS', options={'maxiter': 300})
      if result.success:
        positions[unknown_indices] = result.x.reshape(-1, 2)
        log.debug(f"Optimization converged with final error: {result.fun}")
      else:
        log.warning(f"Optimization did not converge: {result.message}")
    
    return positions

  def clear(self):
    """Clear stored positions"""
    self.estimated_positions = {}
    self.known_node_ids = set()