#ifndef AUDIO_DEVICE_HPP
#define AUDIO_DEVICE_HPP

#include <inttypes.h>
#include <cassert>

class AudioDevice {
public:
    AudioDevice ();
    ~AudioDevice ();

    /* 
     * Returns the number of samples per period, i.e. the number of samples to
     * give to the buffer at any one time.
     */
    size_t getPeriodSize ();

    /* get the sound rate in Hz, e.g. 44100 */
    unsigned int getRate ();

    /* 
     * Given a buffer of length divisible by the period size, convert each
     * period size of the buffer into correct bit format, place it in the
     * samples buffer and then play.
     */
    template <typename T>
    void play (T *buffer, size_t length)
    {
        assert(length % this->period_size == 0);
        for (size_t index = 0; index < length; index += period_size) {
            for (size_t i = 0; i < this->num_samples; i++) {
                this->samples[i] = buffer[index + i];
            }
            this->playSamples();
        }
    }

    /* Return the internal samples buffer */
    int16_t* getSamplesBuffer ();
    /* Return the number of samples expected per period */
    size_t getPeriodSamples ();
    /* Return the length of the internal samples buffer in bytes */
    size_t getSamplesBytes ();
    /* attempt to play the samples in the samples buffer */
    void playSamples ();

private:
    int16_t* samples;
    /* length of samples buffer in bytes */
    size_t samples_bytes;
    /* how many samples exist */
    size_t num_samples;
    /* */
    size_t buffer_size;
    /* number of samples per play period */
    size_t period_size;

    void initDevice ();
    void setupHardware ();
    void setupSoftware ();

    void *device_handle;
};

#endif
