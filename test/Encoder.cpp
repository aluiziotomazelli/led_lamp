#include "Arduino.h"
#include "Encoder.h"

#define R_START 0x0

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
#define DIR_CW 0x10
#define DIR_CCW 0x20
const unsigned char ttable[6][4] = {
	// R_START (00)
	{R_START_M, R_CW_BEGIN, R_CCW_BEGIN, R_START},
	// R_CCW_BEGIN
	{R_START_M | DIR_CCW, R_START, R_CCW_BEGIN, R_START},
	// R_CW_BEGIN
	{R_START_M | DIR_CW, R_CW_BEGIN, R_START, R_START},
	// R_START_M (11)
	{R_START_M, R_CCW_BEGIN_M, R_CW_BEGIN_M, R_START},
	// R_CW_BEGIN_M
	{R_START_M, R_START_M, R_CW_BEGIN_M, R_START | DIR_CW},
	// R_CCW_BEGIN_M
	{R_START_M, R_CCW_BEGIN_M, R_START_M, R_START | DIR_CCW},
};
#else
// Use the full-step state table (emits a code at 00 only)
#define DIR_CW 0x10
#define DIR_CCW 0x20

#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

const unsigned char ttable[7][4] = {
	// R_START
	{R_START, R_CW_BEGIN, R_CCW_BEGIN, R_START},
	// R_CW_FINAL
	{R_CW_NEXT, R_START, R_CW_FINAL, R_START | DIR_CW},
	// R_CW_BEGIN
	{R_CW_NEXT, R_CW_BEGIN, R_START, R_START},
	// R_CW_NEXT
	{R_CW_NEXT, R_CW_BEGIN, R_CW_FINAL, R_START},
	// R_CCW_BEGIN
	{R_CCW_NEXT, R_START, R_CCW_BEGIN, R_START},
	// R_CCW_FINAL
	{R_CCW_NEXT, R_CCW_FINAL, R_START, R_START | DIR_CCW},
	// R_CCW_NEXT
	{R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif

EncButton::EncButton(byte clockPin, byte dataPin) :
	clockPin_(clockPin), dataPin_(dataPin) {
	pinMode(clockPin, INPUT_PULLUP);
	digitalWrite(clockPin_, HIGH);
	pinMode(dataPin_, INPUT_PULLUP);
	digitalWrite(dataPin_, HIGH);
	encoderState_ = R_START;
}

// Setter functions
void EncButton::setAccTrueFalse(bool valor) { accIsOn_ = valor; }
void EncButton::setAccGapTimeMs(int valor) { accGapTimeMs_ = valor; }
void EncButton::setAccMaxMultiplier(byte valor) { accMaxMultiplier_ = valor; }

// Main Encoder function
int8_t EncButton::getEncoderPosition() {
	byte pinstate = (digitalRead(clockPin_) << 1) | digitalRead(dataPin_);
	encoderState_ = ttable[encoderState_ & 0xf][pinstate];
	int direction = encoderState_ & 0x30;

	int factor = 1;
	if ((direction != 0) && accIsOn_) {
		unsigned long turnInterval = (millis() - lastReadTimeMs_);
		if (turnInterval < accGapTimeMs_) {
			int z = (accGapTimeMs_ - turnInterval);
			factor = map(z, 1, accGapTimeMs_, 1, (accMaxMultiplier_ + 1));
		}
		lastReadTimeMs_ = millis();
	}
	if (direction == DIR_CW) {
		int x = (1 * factor);
		return x;
	} else if (direction == DIR_CCW) {
		int x = (-1 * factor);
		return x;
	} else
		return 0;
}
