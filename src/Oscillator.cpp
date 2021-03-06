#include <cmath>
#include "Oscillator.hpp"
#include "Definitions.hpp"

unsigned long Oscillator::rate = 44100.0;

Oscillator::Oscillator ()
    : _mode (OSCILLATOR_WAVE_SQUARE)
    , _freq (440.0)
    , _pitch (0.0)
    , _phase (0.0)
    , _phaseIncrement (0.0)
    , _muted (false)
    , _lastOut (0.0)
    , _useNaive (false)
{
    setIncrement();
}

void
Oscillator::setMode (enum OscillatorWave mode)
{
    _mode = mode;
}

void
Oscillator::setFreq (double freq)
{
    _freq = freq;
    setIncrement();
}

void
Oscillator::setPitch (double pitch)
{
    _pitch = pitch;
    setIncrement();
}

void
Oscillator::mute ()
{
    _muted = true;
}

void
Oscillator::unmute ()
{
    _muted = false;
}

void
Oscillator::useNaive (bool useNaive)
{
    _useNaive = useNaive;
}

/* Set the sample rate for all oscillators */
void
Oscillator::setRate (unsigned long rate)
{
    Oscillator::rate = (double) rate;
}

void
Oscillator::setIncrement ()
{
    double pitchModAsFrequency = pow(2.0, fabs(_pitch) * 14.0) - 1;
    if (_pitch < 0) {
        pitchModAsFrequency = -pitchModAsFrequency;
    }
    double freq = fmin(fmax(_freq + pitchModAsFrequency, 0), Oscillator::rate / 2.0);
    _phaseIncrement = freq * TWOPI / Oscillator::rate;
}

double
Oscillator::polyBlep (double t)
{
    double dt = _phaseIncrement / TWOPI;
    /* 0 <= t < 1 */
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    /* -1 < t < 0 */
    else if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    /* 0 otherwise */
    else {
        return 0.0;
    }
}

double
Oscillator::next ()
{
    double value = 0.0;
    double t = _phase / TWOPI;

    if (_muted)
        return value;

    if (_useNaive) {
        value = naiveWave();
        goto increment;
    }
    
    if (_mode == OSCILLATOR_WAVE_SINE) {
        value = naiveWave();
    }
    else if (_mode == OSCILLATOR_WAVE_SAW) {
        value = naiveWave();
        value -= polyBlep(t);
    }
    else {
        value = naiveWave();
        value += polyBlep(t);
        value -= polyBlep(fmod(t + 0.5, 1.0));
        if (_mode == OSCILLATOR_WAVE_TRIANGLE) {
            // Leaky integrator: y[n] = A * x[n] + (1 - A) * y[n-1]
            value = _phaseIncrement * value + (1 - _phaseIncrement) * _lastOut;
            _lastOut = value;
        }
    }
    
increment:
    _phase += _phaseIncrement;
    while (_phase >= TWOPI)
        _phase -= TWOPI;
    return value;
}

double
Oscillator::naiveWave ()
{
    double value = 0.0;
    switch (_mode) {
        case OSCILLATOR_WAVE_SINE:
            value = sin(_phase);
            break;

        case OSCILLATOR_WAVE_SAW:
            value = (2.0 * _phase / TWOPI) - 1.0;
            break;

        case OSCILLATOR_WAVE_SQUARE:
            if (_phase < PI) {
                value = 1.0;
            } else {
                value = -1.0;
            }
            break;

        case OSCILLATOR_WAVE_TRIANGLE:
            value = -1.0 + (2.0 * _phase / TWOPI);
            value = 2.0 * (fabs(value) - 0.5);
            break;
    }
    return value;
}
