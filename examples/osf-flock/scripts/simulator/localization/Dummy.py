from localization.LocalizationEngine import LocalizationEngine  
import time


class Dummy(LocalizationEngine):
    def __init__(self):
        pass

    def localize(self, range_measurements: dict, known_positions: dict):
        # Return empty results
        time.sleep(0.5)
        return None, None
    
    def clear(self):
        pass