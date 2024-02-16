# Splooge Reverb

Stereo Reverb for the wgd modular löwenzahnhonig Daisy Seed Eurorack module.

This effect works best if InL is normalled to InR in hardware. 

## Author

Nik Ansell (gamecat69)

### Version History:
0.1		Nik Ansell		Initial plaything
0.2		Nik Ansell		Added Splooge on Pot4/Cv2 + HPF on Wet Tone control (Pot3/Cv1)
0.3     Nik Ansell      Improved Dry/Wet gain balance. Improved Sploodge effect by reducing delay time and reducing pitchshift gain at lower sploodge settings.

## Description

### Controls:

| Control | Function        |
|---------|-----------------|
| P1      | Reverb Feedback     |
| P2      | Dry/Wet      |
| P3      | Wet Tone        |
| P4      | Splooge. Similar to a spring reverb with delay at low settings and high-feedback shimmer effect at high settings when tone is also high.  |
| CV1     | Wet Tone CV     |
| CV2     | Splooge CV |

### Patching ideas:

#### Synth Ambience

- Feedback: 90%
- Wet/Dry: 50%
- Tone: 30%
- Splooge: 60%

Tweak the Splooge to taste. Low is more subdued, while a high value will sound etheral and spacey.

#### Roomy - good for Drums

- Feedback: 50%
- Wet/Dry: 40%
- Tone: 50%
- Splooge: 5%

Tweaking Feedback will adjust the size of the room. Tweaking Tone will change the materials in the room.
Tweaking Spooge will add echos.

#### Warm - good for stringed instruments

- Feedback: 75%
- Wet/Dry: 50%
- Tone: 40%
- Splooge: 0%

This is a good starting point to then tweak as you like.
The idea with this one is to add some low to medium frequency body to sounds, rather than overwhelming it with reverb.


