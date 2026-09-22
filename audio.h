#pragma once

#include <PDM.h>
#include <I2S.h>
#include "kiss_fftr.h"
#include "util.h"

// Use e.g. -DKISSFFT_DATATYPE=int16_t -DFIXED_POINT=1
// TODO: kiss_fft_scalar=int16_t only for now, flexible fft datatype/bitwidth for kissfft?
// TODO: portability?
// FIXME: test i2s

#define DEFAULT_NSAMP 256
#define DEFAULT_SAMPLE_RATE 8000

class AudioProcessing;
class DigitalAudioProcessing;
class FFTProcessing;
static FFTProcessing *sharedFFT = NULL;

volatile int rawBufferFilled = 0;
volatile int rawSamplesRead = 0;

class AudioProcessing {
  int peakAccum = 0;
  BaselineStepper peakStepper;
  int subscribeCount = 0;
  int16_t window[DEFAULT_NSAMP] = {0}; // rolling window of the newest windowSize samples
  int16_t readChunk[DEFAULT_NSAMP];    // scratch for one drain of the driver's buffer
  uint32_t sampleCount = 0;            // samples ever shifted into the window
protected:
  virtual void startStreaming() { }
  virtual void stopStreaming() { }
  // driver-level read; returns bytes read. Clients don't call this, they call update() and share the window.
  virtual size_t read(int16_t *buffer, size_t size) = 0;
public:
  static constexpr int windowSize = DEFAULT_NSAMP;
  int sampleRate;

  int ignoreSamples = 3; // ignore the first n samples of each read
  int peakFrames = 6; // frames across which to compute peak amplitude
  int peakBaselineMagic = 170; // tuning magic
  
  AudioProcessing(int sampleRate) : sampleRate(sampleRate) { }

  bool isStreaming() {
    return (subscribeCount > 0);
  }

  void subscribe() {
    if (subscribeCount++ == 0) {
      startStreaming();
    }
  }

  void unsubscribe() {
    assert(subscribeCount > 0, "not subscribed");
    if (--subscribeCount == 0) {
      stopStreaming();
    }
  }

  // Drain whatever the driver has produced into the tail of the window - call every frame or discard a few overloud frames when starting
  void update() {
    if (!isStreaming()) {
      return;
    }
    int total = 0;
    size_t bytesRead;
    while (total < windowSize && (bytesRead = read(readChunk, sizeof(readChunk))) > 0) {
      int nRead = bytesRead / sizeof(readChunk[0]);
      int n = min(nRead, windowSize);
      memmove(window, window + n, (windowSize - n) * sizeof(window[0]));
      memcpy(window + windowSize - n, readChunk + (nRead - n), n * sizeof(window[0]));
      processAmplitude(readChunk, bytesRead);
      sampleCount += n;
      total += n;
    }
  }

  // the newest windowSize samples, as of the last update()
  const int16_t *samples() {
    return window;
  }

  // samples ever shifted into the window; clients diff this against their last read to see how much is new
  uint32_t samplesSeen() {
    return sampleCount;
  }

  // peak amplitude as of the last update()
  int peakAmplitude() {
    return peakAccum;
  }

private:
  int processAmplitude(int16_t *buffer, size_t size) {
    if (size > ignoreSamples) {
      int16_t min_sample = INT16_MAX;
		  int16_t max_sample = INT16_MIN;
      for (int s = ignoreSamples; s < size / sizeof(buffer[0]); ++s) {
        // logf("buffer[%i] = %i", s, buffer[s]);
        if (buffer[s] > max_sample) max_sample = buffer[s];
        if (buffer[s] < min_sample) min_sample = buffer[s];
      }
      // logf("min_sample = %i, max_sample = %i", min_sample, max_sample);
      int maxAmplitude = max(abs(min_sample), abs(max_sample));
      for (int k = peakStepper.steps(peakBaselineMagic); k > 0; --k) {
        peakAccum = (peakFrames * peakAccum + maxAmplitude) / (peakFrames + 1);
      }
    }
    return peakAccum;
  }
};

class DigitalAudioProcessing : public AudioProcessing {
protected:
  int dataPin;
  int clockPin;
public:
  DigitalAudioProcessing(int dataPin, int clockPin, int sampleRate=DEFAULT_SAMPLE_RATE) 
    : AudioProcessing(sampleRate), dataPin(dataPin), clockPin(clockPin) {
  }

  virtual size_t read(int16_t *buffer, size_t size) = 0;
};

class AudioInputI2S : public DigitalAudioProcessing {
public:
  I2S i2s;
  AudioInputI2S(int dataPin, int clockPin) : DigitalAudioProcessing(dataPin, clockPin) { }
protected:
  virtual void startStreaming() {
    i2s.setDATA(dataPin);
    i2s.setBCLK(clockPin);
    assert(i2s.begin(sampleRate), "Failed to initialize I2S device");
  }
  virtual void stopStreaming() {
    i2s.end();
  }
public:
  virtual size_t read(int16_t *buffer, size_t size) {
    assert(isStreaming(), "can't read unless streaming");
    irq_set_enabled(DMA_IRQ_0, false);
    int hasSamples = i2s.available();
    for (int i = 0; i < min(hasSamples, size); ++i) {
      int32_t l=0,r=0;
      i2s.read32(&l, &r);
      buffer[i] = (l?:r)>>16;
    }
    irq_set_enabled(DMA_IRQ_0, true);
    return min(hasSamples, size);
  }
};

class AudioInputPDM : public DigitalAudioProcessing {
  bool fixSelectHIGH;
public:
  AudioInputPDM(int dataPin, int clockPin, bool fixSelectHIGH=false) : DigitalAudioProcessing(dataPin, clockPin), fixSelectHIGH(fixSelectHIGH) { }
protected:
  virtual void startStreaming() {
    PDM.setDIN(dataPin);
    PDM.setCLK(clockPin);

    if (fixSelectHIGH) {
      // https://github.com/earlephilhower/arduino-pico/issues/3223
      // Our LMD4030 microphone must be sampled >15ns after the CLK 0->1 but before the CLK 0->1 transition
      // Workaround this unusual(?) timing by telling the mic to send data in the HIGH channel
      // framework-arduinopico PDM library only supports mono channel anyway
      pinMode(clockPin+1, OUTPUT);
      digitalWrite(clockPin+1, HIGH);
    }

    assert(1 == PDM.begin(1, sampleRate), "Failed to initialize PDM device");
   }
  virtual void stopStreaming() {
    PDM.end();
  }
public:
  virtual size_t read(int16_t *buffer, size_t size) {
    assert(isStreaming(), "can't read unless streaming");
    int hasBytes = PDM.available();
    size_t bytesRead = PDM.read(buffer, min(hasBytes, size));
    return bytesRead;
  }
};

class ShimAudioProcessing : public AudioProcessing {
  uint32_t prngState = 0xACE1u;
public:
  ShimAudioProcessing(int sampleRate=DEFAULT_SAMPLE_RATE) : AudioProcessing(sampleRate) { }

  virtual size_t read(int16_t *buffer, size_t size) {
    size_t numSamples = size / sizeof(buffer[0]);
    for (size_t i = 0; i < numSamples; ++i) {
      // xorshift32
      prngState ^= prngState << 13;
      prngState ^= prngState >> 17;
      prngState ^= prngState << 5;
      buffer[i] = (int16_t)(prngState & 0xFFFF);
    }
    return size;
  }
};

class AmplitudeReceiver {
  AudioProcessing &audio;
  BaselineStepper levelStepper;
  int level = 0;
public:
  int levelSmoothing = 20; // in levelBaselineFPS steps; ambientLevel settles over a couple of seconds
  static constexpr int levelBaselineFPS = 10;

  AmplitudeReceiver(AudioProcessing &audio) : audio(audio) {
    audio.subscribe();
  }
  ~AmplitudeReceiver() {
    audio.unsubscribe();
  }

  // peak amplitude of the newest audio; call it every frame to keep the source's window current
  int amplitudeFrame() {
    audio.update();
    return audio.peakAmplitude();
  }

  // amplitudeFrame smoothed slowly enough to stand in for how loud the room is; call it every frame
  int ambientLevel() {
    int peak = amplitudeFrame();
    for (int k = levelStepper.steps(levelBaselineFPS); k > 0; --k) {
      level = (levelSmoothing * level + peak) / (levelSmoothing + 1);
    }
    return level;
  }
};


struct FFTFrame {
  size_t size;
  FFTFrame(size_t size) : size(size) {}
  int16_t *spectrum = NULL;
  int16_t *smoothSpectrum = NULL;
  int peak = 0;
};

class FFTProcessing {
  int windowSize;
  int numBins; // spectrumSize
  int *fftBinSizes;
  int16_t *spectrum;
  int16_t *spectrumAccum;
  int spectrumAccumSamples{30};
  uint32_t samplesSeen{0}; // audio.samplesSeen() as of the last transform
  AudioProcessing &audio;
  FFTFrame dataFrame{0};
  bool frameStale{true};
  BaselineStepper spectrumAccumStepper;
  int subscribeCount{0};
  bool initialized{false};
  kiss_fftr_cfg fftCfg{NULL}; // kept for the life of the processor; allocating one recomputes the whole trig table
public:

  FFTProcessing(AudioProcessing &audio, int numBins, int windowSize=DEFAULT_NSAMP) : audio(audio), numBins(numBins), windowSize(windowSize) { }

  void initialize() {
    assert(fftBinSizes == NULL, "fft double initialize");
    assert(windowSize <= AudioProcessing::windowSize, "fft window %i larger than audio window %i", windowSize, AudioProcessing::windowSize);
    fftBinSizes = new int[numBins];
    spectrum = new int16_t[numBins]();
    spectrumAccum = new int16_t[numBins]();
    getFFTBins(numBins, windowSize/2, fftBinSizes);
    fftCfg = kiss_fftr_alloc(windowSize,false,0,0);
    if (hopSamples == 0) {
      hopSamples = windowSize/2; // 50% overlap: one transform per half-window of new audio
    }
    // hand out a valid (silent) frame until enough audio has arrived for the first real transform
    dataFrame = FFTFrame(numBins);
    dataFrame.spectrum = spectrum;
    dataFrame.smoothSpectrum = (spectrumAccumSamples ? spectrumAccum : NULL);
    initialized = true;
  }

  ~FFTProcessing() {
    delete [] fftBinSizes;
    delete [] spectrum;
    delete [] spectrumAccum;
    kiss_fft_free(fftCfg);
  }

  void subscribe() {
    if (subscribeCount++ == 0) {
      audio.subscribe();
    }
  }

  void unsubscribe() {
    assert(subscribeCount > 0, "not subscribed");
    if (--subscribeCount == 0) {
      audio.unsubscribe();
    }
  }

  int spectrumAccumBaselineMagic = 130;

  // How much new audio has to arrive before another transform is worth running. 
  // The window only turns over as fast as the mic fills it
  int hopSamples = 0;

  // bench shim: when >0, spectrum bins are replaced with a deterministic rotating comb at this level so
  // sound patterns can be A/B tested without ambient audio. The FFT still runs for realistic CPU load.
  int benchTestLevel = 0;

  void frameReset() {
    frameStale = true;
  }

  FFTFrame getDataFrame() {
    if (!initialized) {
      initialize();
    }
    audio.update();

    // frame is still the newest data we have
    if (!frameStale || audio.samplesSeen() - samplesSeen < hopSamples) {
      return dataFrame;
    }
    frameStale = false;
    samplesSeen = audio.samplesSeen();

    kiss_fft_scalar fft_in[windowSize];
    kiss_fft_cpx fft_out[windowSize];

    // the newest windowSize samples of the shared window
    const int16_t *samples = audio.samples() + (AudioProcessing::windowSize - windowSize);

    // fill fourier transform input while subtracting DC component
    int64_t sum = 0;
    for (int i = 0; i < windowSize; i++) { sum += samples[i]; }
    int32_t avg = sum/windowSize;
    for (int i = 0; i < windowSize; i++) { fft_in[i] = samples[i] - avg; }
    
    // compute fast fourier transform
    kiss_fftr(fftCfg, fft_in, fft_out);
    
    // any frequency bin over windowSize/2 is aliased (nyquist sampling theorum)
    const int accumSteps = (spectrumAccumSamples ? spectrumAccumStepper.steps(spectrumAccumBaselineMagic) : 0);
    for (int b = 0; b < numBins; b++) {
      int stopIndex = (b < numBins - 1 ? fftBinSizes[b + 1] - 1 : windowSize/2 - 1);
      int64_t powerSum= 0;
      for (int i = fftBinSizes[b]; i <= stopIndex; ++i) {
        int64_t power = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
        powerSum += power;
      }

      // FIXME: specific to one microphone? generalize.
      powerSum /= 16384.;

      spectrum[b] = powerSum;
      if (benchTestLevel > 0) {
        spectrum[b] = ((b + (int)(millis()/300)) % 3 == 0 ? benchTestLevel : 0);
      }
      for (int k = accumSteps; k > 0; --k) {
        spectrumAccum[b] = (spectrumAccum[b] * spectrumAccumSamples + spectrum[b]) / (spectrumAccumSamples + 1);
      }
    }
    dataFrame.peak = audio.peakAmplitude();
    return dataFrame;
  }

  void logFrame(FFTFrame frame) {
    for (int x = 0; x < frame.size; ++x) {
      int16_t level = frame.spectrum[x];
      if (level > 0) {
        Serial.printf("%4i ", level);
      } else {
        Serial.print("  -  ");
      }
    }
    Serial.printf(" : (%4i)", frame.peak);
    Serial.println();
  }

private:
  // https://forum.pjrc.com/threads/32677-Is-there-a-logarithmic-function-for-FFT-bin-selection-for-any-given-of-bands
  static float FindE(int bins, int window) {
    float increment = 0.1, eTest, n;
    int b, count, d;

    for (eTest = 1; eTest < window; eTest += increment) {     // Find E through brute force calculations
      count = 0;
      for (b = 0; b < bins; b++) {                         // Calculate full log values
        n = pow(eTest, b);
        d = int(n + 0.5);
        count += d;
      }
      if (count > window) {     // We calculated over our last bin
        eTest -= increment;   // Revert back to previous calculation increment
        increment /= 10.0;    // Get a finer detailed calculation & increment a decimal point lower
      }
      else if (count == window)
        return eTest;
      if (increment < 0.0000001)        // Ran out of calculations. Return previous E. Last bin will be lower than (bins-1)
        return (eTest - increment);
    }
    return 0;
  }

  void getFFTBins(int numBins, int window, int *fftBins) {
    const int binStartOffset = 2; // the first two FFT bins are garbage (DC bins?)
    float e = FindE(numBins + 1, window - binStartOffset);
    if (e) {
      int count = binStartOffset;
      Serial.printf("E = %4.4f\n", e);
      for (int b = 0; b < numBins; b++) {
        float n = pow(e, b+1);
        int d = int(n + 0.5);
        Serial.printf( "%4d ", count);
        fftBins[b] = count;
        count += d - 1;
        Serial.printf( "%4d\n", count);
        ++count;
      }
    } else {
      Serial.println("Error\n");
    }
  }
};

class FFTReceiver {
protected:
  FFTProcessing &receiverFftProcessing;
public:
  FFTReceiver(FFTProcessing &fft) : receiverFftProcessing(fft) {
    receiverFftProcessing.subscribe();
  }
  ~FFTReceiver() {
    receiverFftProcessing.unsubscribe();
  }

  FFTFrame spectrumFrame() {
    return receiverFftProcessing.getDataFrame();
  }

  void fftLog() {
    receiverFftProcessing.logFrame(spectrumFrame());
  }
};

