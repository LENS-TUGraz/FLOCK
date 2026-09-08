import json
import numpy as np

from Position import Position

import logging
log = logging.getLogger(__name__)

class Scenario:
  def __init__(self, name: str, num_nodes: int, area_x_m: float, area_y_m: float, positions=None):
    self.name = name
    self.num_nodes = num_nodes
    self.area_x_m = area_x_m
    self.area_y_m = area_y_m
    self.positions = positions if positions is not None else []

  def __str__(self):
    return (f"Scenario(name={self.name}, num_nodes={self.num_nodes}, area_x={self.area_x_m}, "
            f"area_y={self.area_y_m}, positions={self.positions})")
  
  @staticmethod
  def _is_connected(positions, max_distance_m):
    """Check if the graph formed by nodes within max_distance_m is connected using BFS."""
    if len(positions) <= 1:
      return True
    if max_distance_m is None:
      return True  # No distance constraint means fully connected
    
    # Build adjacency list
    n = len(positions)
    adj = [[] for _ in range(n)]
    for i in range(n):
      for j in range(i + 1, n):
        dist = np.sqrt((positions[i].x_m - positions[j].x_m)**2 + 
                       (positions[i].y_m - positions[j].y_m)**2)
        if dist <= max_distance_m:
          adj[i].append(j)
          adj[j].append(i)
    
    # BFS from node 0
    visited = [False] * n
    queue = [0]
    visited[0] = True
    count = 1
    
    while queue:
      node = queue.pop(0)
      for neighbor in adj[node]:
        if not visited[neighbor]:
          visited[neighbor] = True
          count += 1
          queue.append(neighbor)
    
    return count == n

  @staticmethod
  def generate_random(name: str, num_nodes: int, area_x_m: float, area_y_m: float, seed: int=42, known_positions: int=0, known_positions_placement: str='corner', ensure_connected=True, max_distance_m: float=None):
    log.info(f"Generating random scenario '{name}' with {num_nodes} nodes in area {area_x_m}x{area_y_m} m")
    
    max_attempts = 100
    attempt = 0
    
    while attempt < max_attempts:
      np.random.seed(seed + attempt)
      ret_pos = []
      for i in range(num_nodes):
        pos = Position(
          id=i + 1,
          x_m=float(np.random.rand() * area_x_m),
          y_m=float(np.random.rand() * area_y_m),
          known=False
        )
        ret_pos.append(pos)
      
      if not ensure_connected or Scenario._is_connected(ret_pos, max_distance_m):
        if attempt > 0:
          log.info(f"Found connected graph after {attempt + 1} attempts")
        break
      
      attempt += 1
      log.debug(f"Attempt {attempt}: graph not connected, retrying...")
    
    if ensure_connected and not Scenario._is_connected(ret_pos, max_distance_m):
      raise ValueError(f"Could not generate a connected graph after {max_attempts} attempts. "
                       f"Try increasing max_distance_m or reducing the area.")

    # Set known positions
    if known_positions > 0:
      if known_positions_placement == 'corner':
        # Select the furthest nodes in each quadrant
        center = (area_x_m / 2, area_y_m / 2)
        quadrants = [
          (0, area_x_m / 2, 0, area_y_m / 2),  # Bottom-left
          (area_x_m / 2, area_x_m, 0, area_y_m / 2),  # Bottom-right
          (0, area_x_m / 2, area_y_m / 2, area_y_m),  # Top-left
          (area_x_m / 2, area_x_m, area_y_m / 2, area_y_m)  # Top-right
        ]
        for q in quadrants:
          if known_positions <= 0:
            break
          q_nodes = [pos for pos in ret_pos if q[0] <= pos.x_m <= q[1] and q[2] <= pos.y_m <= q[3]]
          if q_nodes:
            furthest_node = max(q_nodes, key=lambda pos: (pos.x_m - center[0])**2 + (pos.y_m - center[1])**2)
            furthest_node.known = True
            known_positions -= 1
      elif known_positions_placement == 'random':
        indices = np.random.choice(range(num_nodes), size=known_positions, replace=False)
        for idx in indices:
          ret_pos[idx].known = True
      else:
        raise ValueError("known_positions_placement must be 'corner' or 'random'")

    log.info(f"Generated {len(ret_pos)} positions for scenario '{name}'")
    return Scenario(name, num_nodes, area_x_m, area_y_m, ret_pos)
  
  @staticmethod
  def save_scenarios(scenarios, filename):
    # scenarios: list of Scenario objects
    data = [s.to_dict() for s in scenarios]
    with open(filename, 'w') as f:
      json.dump(data, f, indent=4)
  
  @staticmethod
  def load_scenarios(filename):
    with open(filename, 'r') as f:
      data = json.load(f)
    return [Scenario.from_dict(d) for d in data]

  def to_dict(self):
    return {
      'name': self.name,
      'num_nodes': self.num_nodes,
      'area_x': self.area_x_m,
      'area_y': self.area_y_m,
      'positions': [p.to_dict() for p in self.positions]
    }
  
  @staticmethod
  def from_dict(d):
    return Scenario(
      d['name'],
      d['num_nodes'],
      d['area_x'],
      d['area_y'],
      [Position(**p) for p in d['positions']]
    )

  def generate_distances(self, noise_std_m:float=0.0, noise_offset_m:float=0.0, max_distance_m:float=None):
    distances = {}
    for i in range(len(self.positions)):
      for j in range(i + 1, len(self.positions)):
        pos_i = self.positions[i]
        pos_j = self.positions[j]
        dist = np.linalg.norm(np.array([pos_i.x_m, pos_i.y_m]) - np.array([pos_j.x_m, pos_j.y_m]))
        if max_distance_m is not None and dist > max_distance_m:
          continue
        if noise_std_m > 0.0:
          dist += np.random.normal(0, noise_std_m)
        if noise_offset_m != 0.0:
          dist += noise_offset_m
        if dist < 0:
          continue
        distances[(pos_i.id, pos_j.id)] = float(dist)
    return distances
  
  def generate_known_positions(self, noise_std_m:float=0.0):
    known_positions = {}
    for pos in self.positions:
      if pos.known:
        known_positions[pos.id] = (pos.x_m + np.random.normal(0, noise_std_m), pos.y_m + np.random.normal(0, noise_std_m))
    return known_positions
  
  def get_position_dict(self):
    pos_dict = {}
    for pos in self.positions:
      pos_dict[str(pos.id)] = {'x_m': pos.x_m, 'y_m': pos.y_m}
    return pos_dict
  
  def get_number_of_known_positions(self):
    return sum(1 for pos in self.positions if pos.known)
