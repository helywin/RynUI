"""Stable five-field device identity. Raw counters remain in capture receipts."""
import math
from .protocol import ProtocolError

DEVICE_FIELDS=('logical_width','logical_height','pixel_width','pixel_height','dpr')


def stable_device_metrics(raw):
    if not isinstance(raw,dict) or any(type(raw.get(k)) not in (int,float) or
            not math.isfinite(raw[k]) or raw[k]<=0 for k in DEVICE_FIELDS):
        raise ProtocolError('actual five-field device metrics unavailable')
    projected={k:raw[k] for k in DEVICE_FIELDS}
    if any(abs(raw['logical_'+axis]*raw['dpr']-raw['pixel_'+axis])>.01 for axis in ('width','height')):
        raise ProtocolError('actual logical/pixel/DPR metrics disagree')
    return projected
