"""
pcbnew compatibility layer.
"""
import pcbnew

# === Constants (with version-adaptive fallbacks) ===
IU_PER_MM = pcbnew.PCB_IU_PER_MM
LAYER_ID_COUNT = pcbnew.PCB_LAYER_ID_COUNT
SHAPE_SEGMENT = getattr(pcbnew, 'SHAPE_T_SEGMENT', getattr(pcbnew, 'S_SEGMENT', None))
SHAPE_CIRCLE = getattr(pcbnew, 'SHAPE_T_CIRCLE', None)
SHAPE_ARC = getattr(pcbnew, 'SHAPE_T_ARC', None)
# kicad9 vs kicad10 via types
VIA_BLIND_BURIED = getattr(pcbnew, 'VIATYPE_BLIND_BURIED', getattr(pcbnew, 'VIATYPE_BLIND', None))
FLAG_DELETED = pcbnew.STRUCT_DELETED

# === Classes (for type checks and construction) ===
PAD = pcbnew.PAD
PCB_TRACK = pcbnew.PCB_TRACK
PCB_ARC = pcbnew.PCB_ARC
PCB_VIA = pcbnew.PCB_VIA
PCB_SHAPE = pcbnew.PCB_SHAPE
FOOTPRINT = pcbnew.FOOTPRINT
ZONE = pcbnew.ZONE
LSET = pcbnew.LSET
NETINFO_ITEM = pcbnew.NETINFO_ITEM
BOARD_ITEM_CONTAINER = pcbnew.BOARD_ITEM_CONTAINER
FP_3DMODEL = pcbnew.FP_3DMODEL
VECTOR3D = pcbnew.VECTOR3D
ZONE_CONNECTION_FULL = pcbnew.ZONE_CONNECTION_FULL

# === Functions ===
LoadBoard = pcbnew.LoadBoard
FootprintLoad = pcbnew.FootprintLoad


# === Coordinate/Angle helpers ===
def vector2i_mm(x, y):
    return pcbnew.VECTOR2I_MM(x, y)


def angle_degrees(value):
    return pcbnew.EDA_ANGLE(value, pcbnew.DEGREES_T)


def angle_radians(value):
    return pcbnew.EDA_ANGLE(value, pcbnew.RADIANS_T)


def is_vector2i(obj):
    return type(obj) == pcbnew.VECTOR2I


def is_wxpoint(obj):
    return hasattr(pcbnew, 'wxPoint') and type(obj) == pcbnew.wxPoint


# Footprint lib path discovery
pcbnew_file = pcbnew.__file__
