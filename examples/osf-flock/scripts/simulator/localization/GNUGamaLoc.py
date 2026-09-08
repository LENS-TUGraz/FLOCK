from localization import LocalizationEngine, LocalizationEngineDiagnostics
import networkx as nx
from xml.dom import minidom 
import xml.etree.ElementTree as ET
import os
import numpy as np
import time

import logging
log = logging.getLogger(__name__)

def xml_to_dict(element):
  """
  Recursively converts an XML element and its children to a dictionary.
  ** Thanks ChatGPT **
  """
  # Base case: if the element has no children, return its text
  if len(element) == 0:
    return element.text.strip() if element.text else None

  # If the element has children, create a dictionary to store them
  result_dict = {}
  for child in element:
    child_dict = xml_to_dict(child)
    tag = child.tag.replace("{http://www.gnu.org/software/gama/gama-local-adjustment}", '')

    # If the tag is already in the dictionary, convert it to a list
    if tag in result_dict:
      if not isinstance(result_dict[tag], list):
        result_dict[tag] = [result_dict[tag]]
      result_dict[tag].append(child_dict)
    else:
      result_dict[tag] = child_dict

  # Include element's attributes, if any
  if element.attrib:
    result_dict['@attributes'] = element.attrib

  return result_dict


class GNUGamaLoc(LocalizationEngine):
  _G: nx.DiGraph = nx.DiGraph()
  __input_file: str = "gama_input.xml"
  __output_file: str = "gama_output.xml"

  def __init__(self):
    pass

  def localize(self,  range_measurements: dict, known_positions: dict) -> tuple[dict, LocalizationEngineDiagnostics]:
    for node_id, position in known_positions.items():
      self._add_position(node_id, position)

    for ids, dist in range_measurements.items():
      from_id, to_id = ids
      self._add_distance(from_id, to_id, dist)


    # generate xml file
    log.debug("generate xml imput")
    root = minidom.Document()
 
    gama_local = root.createElement('gama-local')
    gama_local.setAttribute('xmlns', "http://www.gnu.org/software/gama/gama-local")
    root.appendChild(gama_local)

    network = root.createElement('network')
    network.setAttribute('axes-xy', "sw")
    gama_local.appendChild(network)

    description = root.createElement('description')
    description_txt = root.createTextNode('auto-generated')
    description.appendChild(description_txt)
    network.appendChild(description)

    parameters = root.createElement('parameters')
    parameters.setAttribute('sigma-act', 'aposteriori')
    network.appendChild(parameters)

    point_observations= root.createElement('points-observations')
    network.appendChild(point_observations)

    # add all known positions
    for node in self._G.nodes(data=True):
      point = root.createElement('point')
      point.setAttribute('id', str(node[1]['id']))

      x = node[1]['pos_x']
      y = node[1]['pos_y']
      z = node[1]['pos_z']

      adj_str = ''

      if x is not None:
        point.setAttribute('x', str(x))
        adj_str += 'X'
      else:
        adj_str += 'x'
      
      if y is not None:
        point.setAttribute('y', str(y))
        adj_str += 'Y'
      else:
        adj_str += 'y'
      
      if z is not None:
        point.setAttribute('z', str(z))
        adj_str += 'Z'
      else:
        adj_str += 'z'


      if adj_str == '':
        adj_str = 'xyz'

      point.setAttribute('adj', adj_str)
      point_observations.appendChild(point) 
    
    # add all known distances
    for node in self._G.nodes(data=True):
      # skip if node has no outgoing edges
      if len(self._G.out_edges(node[0])) == 0:
        continue

      obs = root.createElement('obs')
      obs.setAttribute('from', str(node[1]['id']))
      for edge in self._G.out_edges(node[0], data=True):
        distance = root.createElement('distance')
        distance.setAttribute('to', str(edge[1]))
        distance.setAttribute('val', str(edge[2]['distance_m']))
#        distance.setAttribute('stdev', str(edge[2]['distance_std_dev_mm']))
        distance.setAttribute('stdev', '0.15')
        obs.appendChild(distance)

      point_observations.appendChild(obs)

    log.debug('writing input file')
    # write input file
    gama_cfg = root.toprettyxml(indent="  ")
    with open(self.__input_file, 'w') as f:
      f.write(gama_cfg)


    # calling gama
    log.debug('calling gama')
    start_time = time.time_ns()
    cmd = "gama-local " + self.__input_file + " --xml " + self.__output_file
    log.debug(cmd)
    os.system(cmd)
    end_time = time.time_ns()
    elapsed_time = end_time - start_time
    diag = LocalizationEngineDiagnostics(runtime_us=elapsed_time / 1e3)


    # read results
    log.debug('parsing results')
    results = ET.parse(self.__output_file)
    root = results.getroot()

    # did gama fail?
    for error in root.iter('{http://www.gnu.org/software/gama/gama-local-adjustment}error'):
      error_msgs = ""

      for description in error.iter('{http://www.gnu.org/software/gama/gama-local-adjustment}description'):
        error_msgs += (description.text) + " "
        log.error(description.text)
      
      return None, None

    points = {}
    info = xml_to_dict(root)

    if 'point' in info['coordinates']['fixed']:
      for fixed in info['coordinates']['fixed']['point']:
        id = str(fixed['id'])
        x = None
        y = None
        z = None
        
        if 'x' in fixed:
          x = fixed['x']
        if 'X' in fixed:
          x = fixed['X']
        if 'y' in fixed:
          y = fixed['y']
        if 'Y' in fixed:
          y = fixed['Y']
        if 'z' in fixed:
          z = fixed['z']  
        if 'Z' in fixed:
          z = fixed['Z']
        
        if z is None:
          z = 0.0

        x_f = float(x) if x is not None else None
        y_f = float(y) if y is not None else None
        points[id] = {'x_m': x_f, 'y_m': y_f, 'z_m': float(z)}

    if 'point' in info['coordinates']['adjusted']:
      for adjusted in info['coordinates']['adjusted']['point']:
        id = str(adjusted['id'])
        x = None
        y = None
        z = None
        
        if 'x' in adjusted:
          x = adjusted['x']
        if 'X' in adjusted:
          x = adjusted['X']
        if 'y' in adjusted:
          y = adjusted['y']
        if 'Y' in adjusted:
          y = adjusted['Y']
        if 'z' in adjusted:
          z = adjusted['z']
        if 'Z' in adjusted:
          z = adjusted['Z']

        if z is None:
          z = 0.0

        x_f = float(x) if x is not None else None
        y_f = float(y) if y is not None else None
        points[id] = {'x_m': x_f, 'y_m': y_f, 'z_m': float(z)}
#        points[id]['timestamp_s'] = self._last_timestamp

    if 'ellipse' in info['coordinates']['std-error-ellipses']:
      for elip in info['coordinates']['std-error-ellipses']['ellipse']:
        id = elip['id']
        major = elip['major']
        minor = elip['minor']
        alpha = elip['alpha']

        mp = float(np.sqrt(float(major)**2+float(minor)**2))
        mxy = float(mp/np.sqrt(2))
#
        points[id]['mp'] = mp
        points[id]['mxy'] = mxy
        points[id]['major'] = major
        points[id]['minor'] = minor
        points[id]['alpha'] = alpha

    # convert point to output format
    for point in points:
      p = points[point]

      if (p['x_m'] is None) or (p['y_m'] is None):
        continue

    return points, diag


  def clear(self):
    self._G = nx.DiGraph()

  def _add_position(self, node_id: int, position: tuple):
    log.debug(f"Adding position for node {node_id}: {position}")
    # Add node with position attribute
    if not self._G.has_node(node_id):
      self._G.add_node(node_id, id=str(node_id), pos_x=position[0], pos_y=position[1], pos_z=0.0)
    else:
      self._G.nodes[node_id]['id'] = str(node_id)
      self._G.nodes[node_id]['pos_x'] = position[0]
      self._G.nodes[node_id]['pos_y'] = position[1]
      self._G.nodes[node_id]['pos_z'] = 0.0
    

  def _add_distance(self, from_id: int, to_id: int, distance_m: float):
    log.debug(f"Adding distance from node {from_id} to node {to_id}: {distance_m} m")
    # Check if nodes exist
    if not self._G.has_node(from_id):
      self._G.add_node(from_id, id=str(from_id), pos_x=None, pos_y=None, pos_z=None)
    if not self._G.has_node(to_id):
      self._G.add_node(to_id, id=str(to_id), pos_x=None, pos_y=None, pos_z=None)

    # Add edge with distance attribute
    self._G.add_edge(from_id, to_id, distance_m=distance_m)

