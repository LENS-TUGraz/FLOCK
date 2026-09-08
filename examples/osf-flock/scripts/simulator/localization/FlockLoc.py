from localization import LocalizationEngine, LocalizationEngineDiagnostics
import json
import time
import serial

import logging

log = logging.getLogger(__name__)
#logging.basicConfig(level=logging.DEBUG)

class FlockLoc(LocalizationEngine):
  def __init__(self, port, baudrate=230400, timeout=1):
    log.info(f"Connecting to Flock device on port {port} with baudrate {baudrate}")
    self.ser = serial.Serial(port, baudrate=baudrate, timeout=timeout)
    cmd = 'reboot\n'
    self.ser.write(cmd.encode('utf-8'))

    time.sleep(5)

    self.ser.reset_input_buffer()

  def localize(self, range_measurements: dict, known_positions: dict) -> tuple[dict, LocalizationEngineDiagnostics]:
    ret = {}

    for key in range_measurements:
      from_id, to_id = key
      distance = range_measurements[key]
      self.set_distance(from_id, to_id, distance)
    
    for key in known_positions:
      x_m, y_m = known_positions[key]
      self.set_position(key, x_m, y_m, type='known')

#    time.sleep(0.1)
    self.ser.write(b'\n')  # Send newline to indicate end of input
    self.ser.readline() # Clear input buffer
    self.ser.reset_input_buffer()

    cmd = 'flock-localize\n'
    log.debug("Sending localization command to Flock device")
    self.ser.write(cmd.encode('utf-8'))
#    time.sleep(0.5)

    retstr_retry = 0
    results = None

    while (retstr_retry < 3) and (results is None):
      retstr = self.ser.readline().decode('utf-8').strip()
      log.debug(f"Received localization results: {retstr}")
      try:
        results = json.loads(retstr)
      except json.JSONDecodeError as e:
        results = None
        log.warning(f"Failed to decode JSON from Flock device: {e}")
        log.warning(f"Received string: {retstr}")
        log.warning(f"Retrying to read localization results ... try {retstr_retry}")
        retstr_retry += 1

    if retstr_retry >= 3:
      return None, None
    if 'nodes' in results:
      for node in results['nodes']:
        if node['type'] == 'ESTIMATE':
          node_id = node['id']
          x_m = node['x_mm'] / 1000.0
          y_m = node['y_mm'] / 1000.0
          ret[str(node_id)] = {'x_m': x_m, 'y_m': y_m}
        if node['type'] == 'ESTIMATE_BAD':
          node_id = node['id']
          x_m = node['x_mm'] / 1000.0
          y_m = node['y_mm'] / 1000.0
          ret[str(node_id)] = {'x_m': x_m, 'y_m': y_m}
      if len(ret) == 0:
        log.warning("No localization results received from Flock device")
        ret = None
    log.debug(f"Localization results: {ret}, diagnostics: {results['diag']}")

    diag = LocalizationEngineDiagnostics(runtime_us=results['diag']['runtime_us'], error_mm=results['diag']['average_error_mm'], success=results['diag']['success'], misc=results['diag'])

    return ret, diag


  def set_distance(self, from_id: int, to_id: int, distance_m: float):
    log.debug(f"Setting distance between node {from_id} and node {to_id} to {distance_m} m")
    distance_mm = int(distance_m * 1000)

    cmd = f'flock-set-dist {from_id} {to_id} {distance_mm}\n'
    self.ser.write(cmd.encode('utf-8'))

  def set_position(self, node_id: int, x_m: float, y_m: float, type: str='known'):
    log.debug(f"Setting position of node {node_id} to ({x_m}, {y_m}) as {type}")
    x_mm = int(x_m * 1000)
    y_mm = int(y_m * 1000)
#    cmd = f'flock-set-pos {node_id} {x_mm} {y_mm} {type}\n'
    cmd = f'flock-set-pos {node_id} {x_mm} {y_mm}\n'
    self.ser.write(cmd.encode('utf-8'))

  def clear(self):
    log.debug("Clearing Flock device data")
    cmd = 'flock-dist-table-clear\n'
    self.ser.write(cmd.encode('utf-8'))
    time.sleep(0.1)

    cmd = 'flock-pos-table-clear\n'
    self.ser.write(cmd.encode('utf-8'))
    time.sleep(0.1)