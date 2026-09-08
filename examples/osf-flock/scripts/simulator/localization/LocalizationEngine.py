from abc import ABC, abstractmethod
from dataclasses import dataclass

@dataclass
class LocalizationEngineDiagnostics:
    success: bool = False
    error_mm: float = 0.0
    runtime_us: float = 0.0
    misc: dict = None

class LocalizationEngine(ABC):
    @abstractmethod
    def localize(self, range_measurements: dict, known_positions: dict) -> tuple[dict, LocalizationEngineDiagnostics]:
        pass

    @abstractmethod
    def clear(self):
        pass
    