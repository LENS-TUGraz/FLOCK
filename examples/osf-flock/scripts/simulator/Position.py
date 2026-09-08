from dataclasses import dataclass

@dataclass
class Position:
  id: int
  x_m: float
  y_m: float
  known: bool = False
  
  def __str__(self):
    return f"Position(id={self.id}, x_m={self.x_m}, y_m={self.y_m}, known={self.known})"
  
  def to_dict(self):
    return {
      'id': self.id,
      'x_m': self.x_m,
      'y_m': self.y_m,
      'known': self.known
    }
  
  @staticmethod
  def from_dict(d):
    return Position(
      id=d['id'],
      x_m=d['x_m'],
      y_m=d['y_m'],
      known=d.get('known', False)
    )