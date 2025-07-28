#pragma once

#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

namespace loewy {

/**
 * Hardware abstraction for Loewenzahnhonig Eurorack module
 * - 4 potentiometers (GPIO pins 15-18)
 * - 2 CV inputs with pull-down resistors (GPIO pins 19-20)
 * - Standardized initialization and control processing
 */
class Loewy {
public:
    // Hardware-mapped control enums (actual GPIO pin numbers)
    enum class Pot : uint8_t {
        POT_1 = 15,
        POT_2 = 16, 
        POT_3 = 17,
        POT_4 = 18
    };
    
    enum class CV : uint8_t {
        CV_1 = 19,
        CV_2 = 20
    };
    
    // Control configuration
    struct Config {
        float pot_smoothing;      // 2ms smoothing for pots (mechanical noise)
        float cv_smoothing;       // 0.5ms smoothing for CVs (pull-down resistors reduce noise)
        bool cv_inverted;         // Loewenzahnhonig hardware inverts CV signals
        uint32_t audio_block_size;
        float sample_rate;
        
        Config() : pot_smoothing(0.002f), cv_smoothing(0.0005f), cv_inverted(true), 
                   audio_block_size(32), sample_rate(48000.0f) {}
    };

private:
    DaisySeed hw_;                    // Composition, not inheritance
    AnalogControl pots_[4];          // P1-P4 potentiometers
    AnalogControl cvs_[2];           // CV1-CV2 inputs
    Config config_;
    
    void InitADC();
    void InitControls();

public:
    /**
     * Initialize hardware with optional configuration
     */
    void Init(const Config& config = Config());
    
    /**
     * Process all analog controls (call once per audio callback)
     */
    void ProcessControls();
    
    /**
     * Get potentiometer value (0.0 to 1.0)
     */
    float GetPot(Pot pot);
    
    /**
     * Get CV input value (0.0 to 1.0, inverted by default for Loewenzahnhonig)
     */
    float GetCV(CV cv);
    
    // Convenience methods for direct access
    float GetPot1() { return GetPot(Pot::POT_1); }
    float GetPot2() { return GetPot(Pot::POT_2); }
    float GetPot3() { return GetPot(Pot::POT_3); }
    float GetPot4() { return GetPot(Pot::POT_4); }
    
    float GetCV1() { return GetCV(CV::CV_1); }
    float GetCV2() { return GetCV(CV::CV_2); }
    
    /**
     * Direct hardware access for advanced use cases
     */
    DaisySeed& GetHardware() { return hw_; }
    const DaisySeed& GetHardware() const { return hw_; }
    
    /**
     * Get sample rate
     */
    float GetSampleRate() const { return config_.sample_rate; }
    
    /**
     * Start audio processing with callback
     */
    void StartAudio(AudioHandle::AudioCallback callback) {
        hw_.StartAudio(callback);
    }
};

} // namespace loewy