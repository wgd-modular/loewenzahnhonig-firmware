#include "loewy.h"

namespace loewy {

void Loewy::Init(const Config& config) {
    config_ = config;
    
    hw_.Configure();
    hw_.Init();
    hw_.SetAudioBlockSize(config_.audio_block_size);
    
    InitADC();
    InitControls();
}

void Loewy::InitADC() {
    AdcChannelConfig adc_config[6];
    
    // Configure pins 15-20 (4 pots + 2 CVs)
    for(int i = 0; i < 6; i++) {
        adc_config[i].InitSingle(hw_.GetPin(15 + i));
    }
    
    hw_.adc.Init(adc_config, 6);
    hw_.adc.Start();
}

void Loewy::InitControls() {
    for(int i = 0; i < 4; i++) {
        pots_[i].Init(hw_.adc.GetPtr(i), 
                      config_.sample_rate, 
                      false, false,
                      config_.pot_smoothing);
    }
    
    for(int i = 0; i < 2; i++) {
        cvs_[i].Init(hw_.adc.GetPtr(4 + i), 
                     config_.sample_rate,
                     false, 
                     config_.cv_inverted,  // Loewenzahnhonig hardware inverts CVs
                     config_.cv_smoothing);
    }
}

void Loewy::ProcessControls() {
    for(int i = 0; i < 4; i++) {
        pots_[i].Process();
    }
    for(int i = 0; i < 2; i++) {
        cvs_[i].Process();
    }
}

float Loewy::GetPot(Pot pot) {
    int index = static_cast<int>(pot) - 15;
    if(index < 0 || index >= 4) return 0.0f;
    return pots_[index].Value();
}

float Loewy::GetCV(CV cv) {
    int index = static_cast<int>(cv) - 19;
    if(index < 0 || index >= 2) return 0.0f;
    return cvs_[index].Value();
}

} // namespace loewy