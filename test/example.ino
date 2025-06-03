#include "Arduino.h"
#include "EncButton.h"


Encoder encoder(2, 3);


int counter = 0; //used to show encoder turns on serial
int ledPWM = 0; //used to set the led PWM
int ledPin = 5;

void setup()
	{
	Serial.begin(9600);
    
    attachInterrupt(0, rotate, CHANGE);
    attachInterrupt(1, rotate, CHANGE);

	//Setting the acceleration true
	encoder.setAccTrueFalse(true);


	pinMode(ledPin, OUTPUT);     //led connected to a pin 5
	digitalWrite(ledPin, LOW); //initiates the led off

	encoder.setAccMaxMultiplier(8);   //set acceleration multiplier
	encoder.setAccGapTimeMs(50);     //set acc gap time
	}
    
void rotate() {
    char result = encoder.getEncoderPosition();   //get the result from encoder
      if (result != 0) {            //if is different from zero
        counter += result;          //add the results to counter (if result is negative, it will decrees the counter
        Serial.println(counter);    //print in serial
        ledPWM += result;          //add the results to ledPWM
        if (ledPWM >= 255)         //keep the values inside PWM values
            ledPWM = 255;
        if (ledPWM <= 0)
            ledPWM = 0;
        analogWrite(ledPin, ledPWM);    //write the value to the PWMled
       }
}

void loop() {


}









