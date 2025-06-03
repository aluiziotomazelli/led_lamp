//=========================================================================================
//  The basic rotary part is a simple copy and paste from buxtronix library:
//  https://github.com/buxtronix/arduino/tree/master/libraries/Rotary
//  I made only added an acceleration feature, which causes the cursor to accelerate in proportion
//  to the spin speed, the faster the spin, the higher the acceleration.
//
//  The acceleration depends of maximum multiplier and the gap time. Bigger multiplier permits bigger increments,
//  while bigger gap time, permits activate the acceleration with less turning speed.
//
//  In practice it works like this: when slowly turning the encoder, each step will change the value 1 by 1, as the speed of
//  rotation increases, each step will change the value in 2, more speed, in 3, and so, until the maximum
//  set in the multiplier was hit. How quickly the acceleration will be active depends on the gap time.

//========================================================================================

#ifndef Encoder_h
#define Encoder_h

#include "Arduino.h"

// Enable this to emit codes twice per step.
// Comment belw line if your encoder is changing two values per "click turn", or per detention
#define HALF_STEP

// Values returned by 'process'
// No complete step yet.
#define ENC_NONE 0
#define ENC_CW 1
#define ENC_CCW -1

class Encoder {
public:
    EncButton(byte clockPin, byte dataPin);

    void setAccTrueFalse(bool valor);
    void setAccGapTimeMs(int valor);
    void setAccMaxMultiplier(byte valor);

    // Return rotary encoder
    // 0 = no turn
    // +1 (or more, if multiplier is active) = clockwise
    // -1 (or less, if multiplier is active) = Anti-clockwise
    int8_t getEncoderPosition();

private:
    byte clockPin_;
    byte dataPin_;

    //Variables used to acceleration
    bool accIsOn_ = false;
    unsigned long lastReadTimeMs_ = 0;
    unsigned int accGapTimeMs_ = 50;
    byte accMaxMultiplier_ = 5;


    //Variables for the states
    byte encoderState_;
    unsigned long timeMs_;
    State buttonState_;

};

#endif

