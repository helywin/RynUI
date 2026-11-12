"""Validate every action/checkpoint receipt before any visual or semantic verdict."""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

from .protocol import ProtocolError, loads, relative_path, validate


class IncompleteRun(ProtocolError):
    """A zero process exit never compensates for missing execution evidence."""


def image_extent(path):
    data = Path(path).read_bytes()
    if data.startswith(b'\x89PNG\r\n\x1a\n') and len(data) >= 33 and data[12:16] == b'IHDR':
        import zlib
        offset = 8
        image_data = bytearray()
        width, height, bits, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', data[16:29])
        channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color)
        if not width or not height or width * height > 64_000_000 or bits != 8 or channels is None or any(
                (compression, filtering, interlace)):
            raise IncompleteRun('unsupported/unbounded PNG pixel format')
        while offset + 12 <= len(data):
            size = struct.unpack('>I', data[offset:offset + 4])[0]
            end = offset + 12 + size
            if end > len(data):
                raise IncompleteRun('truncated PNG chunk')
            tag = data[offset + 4:offset + 8]
            chunk = data[offset + 4:offset + 8 + size]
            crc = struct.unpack('>I', data[offset + 8 + size:end])[0]
            if zlib.crc32(chunk) != crc:
                raise IncompleteRun('PNG chunk checksum differs')
            if tag == b'IDAT':
                image_data.extend(data[offset + 8:offset + 8 + size])
            if tag == b'IEND':
                if size != 0 or end != len(data) or not image_data:
                    raise IncompleteRun('PNG has no complete image data')
                expected = height * (1 + width * channels)
                try:
                    decoder = zlib.decompressobj()
                    pixels = decoder.decompress(image_data, expected + 1)
                    if (len(pixels) != expected or not decoder.eof or decoder.unused_data or
                            decoder.unconsumed_tail or any(pixels[y * (1 + width * channels)] > 4
                                                           for y in range(height))):
                        raise IncompleteRun('PNG pixel data is incomplete or has invalid scanlines')
                except zlib.error as error:
                    raise IncompleteRun('PNG image data cannot be decoded') from error
                return width, height
            offset = end
        raise IncompleteRun('PNG has no complete image end')
    if data.startswith(b'BM') and len(data) >= 54:
        width, height = struct.unpack('<ii', data[18:26])
        planes, bits = struct.unpack('<HH', data[26:30])
        offset = struct.unpack('<I', data[10:14])[0]
        stride = ((width * bits + 31) // 32) * 4
        if width <= 0 or height == 0 or planes != 1 or bits not in (24, 32) or offset < 54:
            raise IncompleteRun('invalid BMP extent/format')
        if len(data) < offset + stride * abs(height):
            raise IncompleteRun('truncated BMP pixel data')
        return width, abs(height)
    raise IncompleteRun('capture has no supported complete PNG/BMP header')


def read_observations(path, case, nonce, *, adapter=None, source_hash=None, preset=None, configuration=None):
    path = Path(path).resolve()
    if not path.is_file() or not path.stat().st_size:
        raise IncompleteRun('adapter returned no observation stream')
    text = path.read_text(encoding='utf-8')
    if not text.endswith('\n'):
        raise IncompleteRun('observation stream is truncated')
    records = []
    for line in text.splitlines():
        if not line:
            raise IncompleteRun('empty observation record')
        records.append(validate('observation', loads(line)))
    return validate_observations(records, path.parent, case, nonce, adapter=adapter, source_hash=source_hash,
                                 preset=preset, configuration=configuration)


def validate_observations(records, directory, case, nonce, *, adapter=None, source_hash=None, preset=None,
                          configuration=None):
    if not records or records[0]['type'] != 'hello' or records[-1]['type'] != 'complete':
        raise IncompleteRun('missing hello/complete')
    if sum(row['type'] == 'hello' for row in records) != 1 or sum(row['type'] == 'complete' for row in records) != 1:
        raise IncompleteRun('duplicate hello/complete')
    for record in records:
        validate('observation', record)
        if record['nonce'] != nonce or record['case'] != case['identity']:
            raise IncompleteRun('observation belongs to another run or case')
        if adapter is not None and record['adapter'] != adapter:
            raise IncompleteRun('adapter identity changed')
    hello = records[0]
    provenance = hello['provenance']
    if source_hash is not None and provenance.get('source', {}).get('source_hash') != source_hash:
        raise ProtocolError('source/binary closure differs; rebuild before executing')
    if preset is not None and provenance.get('preset') != preset:
        raise ProtocolError('native configure preset differs from requested build preset')
    if configuration is not None and provenance.get('configuration') != configuration:
        raise ProtocolError('native configuration differs from requested build preset')
    expected_steps = [action['step'] for action in case['actions']]
    expected_checkpoints = [item['step'] for item in case['checkpoints']]
    receipts = [row for row in records if row['type'] == 'receipt']
    checkpoints = [row for row in records if row['type'] == 'checkpoint']
    if [row['step'] for row in receipts] != expected_steps:
        raise IncompleteRun('sent/dispatched receipt identities or order differ from selected actions')
    if [row['step'] for row in checkpoints] != expected_checkpoints:
        raise IncompleteRun('checkpoint identities or order differ from selected checkpoints')
    if any(not row['receipt']['dispatched'] for row in receipts):
        raise IncompleteRun('at least one selected action was not actually dispatched')
    complete = records[-1]
    if complete['completed_steps'] != expected_steps or complete['completed_checkpoints'] != expected_checkpoints:
        raise IncompleteRun('complete identity sets differ from requested execution')
    # Enforce interleaving, not just two independently correct sets.
    expected_records = [('hello', '')]
    for action in case['actions']:
        expected_records.append(('receipt', action['step']))
        if action['kind'] == 'checkpoint':
            expected_records.append(('checkpoint', action['step']))
    expected_records.append(('complete', ''))
    if [(row['type'], row['step']) for row in records] != expected_records:
        raise IncompleteRun('receipt/checkpoint protocol is out of order')
    capture_nonces = set()
    capture_paths = set()
    previous_callbacks = []
    for record in records:
        callbacks = record['callbacks']
        if callbacks[:len(previous_callbacks)] != previous_callbacks:
            raise IncompleteRun('callback stream was rewritten, truncated or reordered')
        if [row['seq'] for row in callbacks] != list(range(len(callbacks))):
            raise IncompleteRun('callback sequence has gaps or duplicates')
        if any(row['step'] not in expected_steps for row in callbacks):
            raise IncompleteRun('callback has no selected causative action')
        previous_callbacks = callbacks
        if record['type'] != 'checkpoint':
            if record['captures']:
                raise IncompleteRun('capture attached to a non-checkpoint record')
            continue
        declaration = next(item for item in case['checkpoints'] if item['step'] == record['step'])
        if [target['identity'] for target in record['targets']] != declaration['targets']:
            raise IncompleteRun('checkpoint omitted or substituted semantic target')
        if not record['captures']:
            raise IncompleteRun('checkpoint has no current capture')
        metrics = record['resources'].get('metrics')
        if not isinstance(metrics, dict) or any(field not in metrics for field in
                                               ('dpr', 'pixel_width', 'pixel_height', 'logical_width', 'logical_height')):
            raise IncompleteRun('checkpoint has no actual device metrics')
        if metrics['dpr'] <= 0 or metrics['pixel_width'] <= 0 or metrics['pixel_height'] <= 0:
            raise IncompleteRun('actual device metrics have zero extent')
        if abs(metrics['logical_width'] * metrics['dpr'] - metrics['pixel_width']) > .01 or abs(
                metrics['logical_height'] * metrics['dpr'] - metrics['pixel_height']) > .01:
            raise IncompleteRun('logical/physical extent or DPR is inconsistent')
        for capture in record['captures']:
            if capture['nonce'] in capture_nonces or capture['path'] in capture_paths:
                raise IncompleteRun('capture reused a prior nonce/path')
            capture_nonces.add(capture['nonce'])
            capture_paths.add(capture['path'])
            if not capture['fence_completed'] or capture['checkpoint'] != record['step']:
                raise IncompleteRun('capture has no completed current checkpoint fence')
            if capture['nonce'] == nonce:
                raise IncompleteRun('capture needs its own checkpoint nonce')
            if capture['dpr'] != metrics['dpr'] or (capture['width'], capture['height']) != (
                    metrics['pixel_width'], metrics['pixel_height']):
                raise IncompleteRun('capture dimensions/DPR differ from actual device metrics')
            if any(target['scene_revision'] != capture['scene_revision'] for target in record['targets']):
                raise IncompleteRun('capture is from a stale scene revision')
            relative_path(capture['path'])
            image = (Path(directory) / capture['path']).resolve()
            if not image.is_relative_to(Path(directory).resolve()) or not image.is_file():
                raise IncompleteRun('capture is missing or escapes its case directory')
            if hashlib.sha256(image.read_bytes()).hexdigest() != capture['sha256']:
                raise IncompleteRun('capture content hash differs')
            if image_extent(image) != (capture['width'], capture['height']):
                raise IncompleteRun('capture image extent differs from receipt')
    return {'records': records, 'hello': hello, 'receipts': {row['step']: row for row in receipts},
            'checkpoints': {row['step']: row for row in checkpoints}, 'complete': complete,
            'callbacks': previous_callbacks}
