#include <alsa/asoundlib.h>
#include "Definitions.hpp"
#include "AudioDevice.hpp"

/*
 * These are static, constant values for setting up the audio device to receive
 * PCM data. In the future these might be made apart of the class proper to
 * allow for customization/modularity. But right now there is no need.
 */

static const char *device = "default";
/* sample format */
static const snd_pcm_format_t format = SND_PCM_FORMAT_S16;
static const snd_pcm_access_t format_access = SND_PCM_ACCESS_RW_INTERLEAVED;
static const size_t format_width = snd_pcm_format_physical_width(format);
/* stream rate */
static const unsigned int rate = 44100;
/* count of channels */
static const unsigned int channels = 2;

void
chk_err (int err, const char* format, ...)
{ 
    va_list args;
    if (err < 0) {
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
    }
}

void AudioDevice::initDevice ()
{
    snd_pcm_t *handle;
    int err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0);
    chk_err(err, "Playback open error: %s\n", snd_strerror(err));
    this->device_handle = handle;
}

void AudioDevice::setupHardware ()
{
    assert(this->device_handle);
    snd_pcm_t *handle = (snd_pcm_t*) this->device_handle;

    int err;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_uframes_t val;

    snd_pcm_hw_params_alloca(&hwparams);

    /* fill hwparams with default values */
    err = snd_pcm_hw_params_any(handle, hwparams);
    chk_err(err, "No hardware configurations available: %s\n", snd_strerror(err));

    /* enable resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, hwparams, 1);
    chk_err(err, "Resampling setup failed for playback: %s\n", snd_strerror(err));

    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, hwparams, format_access);
    chk_err(err, "Access not available for playback: %s\n", snd_strerror(err));

    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, hwparams, format);
    chk_err(err, "Sample format not available for playback: %s\n", snd_strerror(err));

    /* set number of channels */
    err = snd_pcm_hw_params_set_channels(handle, hwparams, channels);
    chk_err(err, "Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));

    /* set the stream rate */
    unsigned rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rrate, 0);
    chk_err(err, "Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
    if (rrate != rate) {
        fprintf(stderr, "Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        exit(1);
    }

    /* Set the period and buffer size fairly low to keep latency low */
    this->buffer_size = 1024;
    this->period_size = 64;

    err = snd_pcm_hw_params_set_buffer_size_near(handle, hwparams, &buffer_size);
    chk_err(err, "Unable to set buffer size for playback: %s\n", snd_strerror(err));
    err = snd_pcm_hw_params_get_buffer_size(hwparams, &val);
    chk_err(err, "Unable to get buffer size for playback: %s\n", snd_strerror(err));
    buffer_size = val;

    err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &period_size, NULL);
    chk_err(err, "Unable to set period size for playback: %s\n", snd_strerror(err));
    err = snd_pcm_hw_params_get_period_size(hwparams, &val, NULL);
    chk_err(err, "Unable to get period size for playback: %s\n", snd_strerror(err));
    period_size = val;

    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, hwparams);
    chk_err(err, "Unable to set hw params for playback: %s\n", snd_strerror(err));
}

void AudioDevice::setupSoftware ()
{
    assert(this->device_handle);
    snd_pcm_t *handle = (snd_pcm_t*) this->device_handle;

    int err;
    snd_pcm_sw_params_t *swparams;

    snd_pcm_sw_params_alloca(&swparams);

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    chk_err(err, "Unable to determine current swparams for playback: %s\n", snd_strerror(err));

    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
    chk_err(err, "Unable to set start threshold mode for playback: %s\n", snd_strerror(err));

    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
    chk_err(err, "Unable to set avail min for playback: %s\n", snd_strerror(err));

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    chk_err(err, "Unable to set sw params for playback: %s\n", snd_strerror(err));
}

AudioDevice::AudioDevice ()
{
    initDevice();
    setupHardware();
    setupSoftware();

    this->samples_bytes = (this->period_size * channels * format_width) / 8;
    this->num_samples = this->samples_bytes / sizeof(int16_t);
    this->samples = (int16_t*) malloc(this->samples_bytes);

    if (DEBUG) {
        printf("Period Size: %ld\n", this->period_size);
        printf("Buffer Size: %ld\n", this->buffer_size);
        printf("Num Channels: %u\n", channels);
        printf("Num Samples: %lu\n", this->num_samples);
    }
}

AudioDevice::~AudioDevice ()
{
    snd_pcm_drain((snd_pcm_t*) this->device_handle);
    snd_pcm_close((snd_pcm_t*) this->device_handle);
    free(this->samples);
}

int16_t*
AudioDevice::getSamplesBuffer ()
{
    return this->samples;
}

size_t
AudioDevice::getPeriodSamples ()
{
    return this->num_samples;
}

size_t
AudioDevice::getPeriodSize ()
{
    return this->period_size;
}

size_t
AudioDevice::getSamplesBytes ()
{
    return this->samples_bytes;
}

unsigned int
AudioDevice::getRate ()
{
    return rate;
}

/* Underrun and suspend recovery */
static int xrun_recovery (snd_pcm_t *handle, int err)
{
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

void
AudioDevice::play (int16_t *buffer, size_t length)
{
    assert(length % this->num_samples == 0);
    for (size_t index = 0; index < length; index += num_samples) {
        for (size_t i = 0; i < this->num_samples; i++) {
            this->samples[i] = buffer[index + i];
        }
        this->playSamples();
    }
}

void
AudioDevice::playSamples ()
{
    assert(this->device_handle);
    snd_pcm_t *handle = (snd_pcm_t*) this->device_handle;

    int frames;
    int16_t *ptr = samples;
    int count = this->period_size;

    while (count > 0) {
        frames = snd_pcm_writei(handle, ptr, count);
        if (frames == -EAGAIN)
            continue;
        if (frames < 0) {
            if (xrun_recovery(handle, frames) < 0) {
                printf("Write error: %s\n", snd_strerror(frames));
                exit(EXIT_FAILURE);
            }
            break;  /* skip one period */
        }
        ptr += frames * channels;
        count -= frames;
    }
}
