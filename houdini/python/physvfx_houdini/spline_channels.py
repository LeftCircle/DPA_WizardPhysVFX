"""Shared guide channels for the Wisp Spline Houdini interface."""
import math

# name, label, kind, default. Vectors use one ramp to interpolate all three
# components between their separate minimum and maximum vectors.
CHANNELS = (
    ('density', 'Density', 'float', 1.0),
    ('clump', 'Clump', 'float', 1.0),
    ('pscale', 'Particle Scale', 'float', 1.0),
    ('positionOffset', 'Position Offset', 'vector', (0.0, 0.0, 0.0)),
    ('displacement', 'Noise Displacement', 'vector', (1.0, 1.0, 1.0)),
    ('axisT', 'Frame T', 'vector', (1.0, 0.0, 0.0)),
    ('axisN', 'Frame N', 'vector', (0.0, 1.0, 0.0)),
    ('axisB', 'Frame B', 'vector', (0.0, 0.0, 1.0)),
    ('noise1Roughness', 'Noise 1 Roughness', 'float', 0.5),
    ('noise1Octaves', 'Noise 1 Octaves', 'float', 2.0),
    ('noise1Translation', 'Noise 1 Translation', 'vector', (0.0, 0.0, 0.0)),
    ('noise1Frequency', 'Noise 1 Frequency', 'float', 1.0),
    ('noise1FrequencyJump', 'Noise 1 Frequency Jump', 'float', 2.0),
    ('noise2Roughness', 'Noise 2 Roughness', 'float', 0.5),
    ('noise2Octaves', 'Noise 2 Octaves', 'float', 2.0),
    ('noise2Translation', 'Noise 2 Translation', 'vector', (0.0, 0.0, 0.0)),
    ('noise2Frequency', 'Noise 2 Frequency', 'float', 1.0),
    ('noise2FrequencyJump', 'Noise 2 Frequency Jump', 'float', 2.0),
    ('color', 'Color Tint', 'vector', (1.0, 1.0, 1.0)),
)


def default_guides():
    return [dict(position=(0.0, float(i) * 2.0, 0.0),
                 **{name: value for name, _, _, value in CHANNELS}) for i in range(2)]


def resolve_guides(values):
    """Evaluate ramps by arc length and produce the guide values to stamp.

    In ramp mode each segment is subdivided, so nonlinear ramps are captured
    between the artist's guide points. In manual mode use the exact guides.
    """
    guides = values['guides']
    if len(guides) < 2:
        raise ValueError('Wisp spline requires at least two guide particles')
    positions = []
    for guide in guides:
        pos = tuple(float(x) for x in guide['position'])
        if len(pos) != 3 or not all(math.isfinite(x) for x in pos):
            raise ValueError('Guide positions must have three finite components')
        positions.append(pos)
    lengths = [math.dist(a, b) for a, b in zip(positions, positions[1:])]
    if not all(math.isfinite(x) and x > 1e-8 for x in lengths):
        raise ValueError('Neighboring guide positions must be distinct')
    total = sum(lengths)
    mode = values['mode']
    if mode not in ('ramp', 'manual'):
        raise ValueError('Unknown wisp spline guide mode')
    if mode == 'manual':
        samples = [dict(g, position=pos) for g, pos in zip(guides, positions)]
    else:
        offset = float(values.get('shape_ramp_offset', 0.0))
        if not math.isfinite(offset):
            raise ValueError('Shape ramp sample offset must be finite')
        offset %= 1.0
        shape_names = {name for name, _, _, _ in (*CHANNELS[:8], CHANNELS[-1])}
        divisions = values['ramp_subdivisions']
        if isinstance(divisions, bool) or not isinstance(divisions, int) or not 1 <= divisions <= 64:
            raise ValueError('Ramp samples per segment must be between 1 and 64')
        samples, current = [], 0.0
        for i, length in enumerate(lengths):
            for k in range(divisions + (1 if i == len(lengths)-1 else 0)):
                u = k / divisions
                t = (current + length * u) / total
                pos = tuple(a + (b-a)*u for a, b in zip(positions[i], positions[i+1]))
                item = {'position': pos}
                for name, _, kind, _ in CHANNELS:
                    ramp = values.get('splineRamp_' + name)
                    sample_t = (t + offset) % 1.0 if offset and name in shape_names else t
                    blend = float(ramp.lookup(sample_t) if ramp is not None else sample_t)
                    if not math.isfinite(blend):
                        raise ValueError(f'{name} ramp returned a non-finite value')
                    blend = max(0.0, min(1.0, blend))
                    low, high = values['splineMin_' + name], values['splineMax_' + name]
                    if kind == 'vector':
                        item[name] = tuple(float(a) + (float(b)-float(a))*blend for a, b in zip(low, high))
                    else:
                        item[name] = float(low) + (float(high)-float(low))*blend
                samples.append(item)
            current += length
    for sample in samples:
        for name, _, kind, _ in CHANNELS:
            val = sample[name]
            if kind == 'vector':
                val = tuple(float(x) for x in val)
                if len(val) != 3 or not all(math.isfinite(x) for x in val):
                    raise ValueError(f'{name} must contain three finite components')
                if name.startswith('axis') and sum(x*x for x in val) < 1e-20:
                    raise ValueError(f'{name} must be nonzero at every guide')
                if name == 'color' and min(val) < 0:
                    raise ValueError('color components must be non-negative')
            else:
                val = float(val)
                if not math.isfinite(val):
                    raise ValueError(f'{name} must be finite')
                if name == 'density' or name.endswith('Roughness'):
                    if val < 0: raise ValueError(f'{name} must be non-negative')
                elif val <= 0:
                    raise ValueError(f'{name} must be positive')
            sample[name] = val
    for sample in samples:
        sample['position'] = tuple(a+b for a, b in zip(sample['position'], sample['positionOffset']))
        if not all(math.isfinite(x) for x in sample['position']):
            raise ValueError('Position offsets must produce finite guide positions')
    for a, b in zip(samples, samples[1:]):
        if math.dist(a['position'], b['position']) <= 1e-8:
            raise ValueError('Position offsets make neighboring spline samples coincide')
    if mode == 'manual':
        resolved_positions = [s['position'] for s in samples]
    else:
        resolved_positions = [samples[i * divisions]['position'] for i in range(len(positions))]
    return resolved_positions, samples
