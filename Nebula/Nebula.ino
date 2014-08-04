#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdint.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <Arduino.h>
#include <avr/wdt.h>


#define MPAUSEDIT 10
#define MPAUSEDITEND 10
#define MPAUSEDAH 30
#define MPAUSEDAHEND 10
#define MPAUSECHAR 20
#define MPAUSECHAREND 10
#define MPAUSEMESSAGE 70
#define MPAUSEMESSAGEEND 0
#define MNUL 0x00

//#define MPAUSEDIT 20
//#define MPAUSEDITEND 20
//#define MPAUSEDAH 60
//#define MPAUSEDAHEND 20
//#define MPAUSECHAR 40
//#define MPAUSECHAREND 20
//#define MPAUSEMESSAGE 140
//#define MPAUSEMESSAGEEND 0
//#define MNUL 0x00


#define OFF   0x0
#define ON    0x1
#define LEDPIN PCINT3
#define BUTTONPIN PCINT4
#define BODS 7                   //BOD Sleep bit in MCUCR
#define BODSE 2                  //BOD Sleep enable bit in MCUCR

// PB3 is led control
// PB4 is input button

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

volatile boolean f_wdt = 1;


const uint8_t PROGMEM asciitomorse_table[] = {
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	MNUL, MNUL, MNUL, MNUL,
	0x73, MNUL, 0x55, 0x32, // , _ . /
	0x3F, 0x2F, 0x27, 0x23, // 0 1 2 3
	0x21, 0x20, 0x30, 0x38, // 4 5 6 7
	0x3C, 0x37, MNUL, MNUL, // 8 9 _ _
	MNUL, 0x31, MNUL, 0x4C, // _ = _ ?
	MNUL, 0x05, 0x18, 0x1A, // _ A B C
	0x0C, 0x02, 0x12, 0x0E, // D E F G
	0x10, 0x04, 0x17, 0x0D, // H I J K
	0x14, 0x07, 0x06, 0x0F, // L M N O
	0x16, 0x1D, 0x0A, 0x08, // P Q R S
	0x03, 0x09, 0x11, 0x0B, // T U V W
	0x19, 0x1B, 0x1C, MNUL, // X Y Z _
	MNUL, MNUL, MNUL, MNUL,
	MNUL, 0x05, 0x18, 0x1A, // _ A B C
	0x0C, 0x02, 0x12, 0x0E, // D E F G
	0x10, 0x04, 0x17, 0x0D, // H I J K
	0x14, 0x07, 0x06, 0x0F, // L M N O
	0x16, 0x1D, 0x0A, 0x08, // P Q R S
	0x03, 0x09, 0x11, 0x0B, // T U V W
	0x19, 0x1B, 0x1C, MNUL, // X Y Z _
	MNUL, MNUL, MNUL, 0x99,
};


void print_morsec(uint8_t i);
void print_morses(const char *s);
uint8_t get_morsechar(uint8_t x);
void print_morsemessagepause(void);
void print_morsecharpause(void);
void print_morseunknownpause(void);
void print_morsedit(void);
void print_morsedah(void);
void printMessage(int i);
void messageStart(void);

void powerDown();
void sleepNow();

volatile boolean modeSemaphore = false;
volatile boolean poweredOn = true;
uint8_t modeCounter = 0;

// brown out detector addresses
uint8_t mcucr1, mcucr2;

volatile uint8_t animationStep = 0;          // Used for incrementing animations (0-384)
volatile int stateCount = 0;             // System mode

boolean growing = true;

int frameRepeat = 1;
int frameTrack = 0;
int theRoof = 254;
int theFloor = 1;
int buttonTimer = 0;


int length = 20; // the number of notes
char notes[] PROGMEM = "ee e ce g  f  C  g e";
int beats[] PROGMEM = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 2, 1, 1, 1, 1, 2, 1};
int noteTempo = 50; // 10 is fast, 50 is medium, 100 is slow

uint8_t sinTable[] = { 
  4, 9, 13, 18, 22, 27, 31, 35, 40, 44, 
  49, 53, 57, 62, 66, 70, 75, 79, 83, 87, 
  91, 96, 100, 104, 108, 112, 116, 120, 124, 128, 
  131, 135, 139, 143, 146, 150, 153, 157, 160, 164, 
  167, 171, 174, 177, 180, 183, 186, 190, 192, 195, 
  198, 201, 204, 206, 209, 211, 214, 216, 219, 221, 
  223, 225, 227, 229, 231, 233, 235, 236, 238, 240, 
  241, 243, 244, 245, 246, 247, 248, 249, 250, 251, 
  252, 253, 253, 254, 254, 254, 255, 255, 255, 255, 
  }; 



void setup()
{
  pinMode(BUTTONPIN, INPUT_PULLUP);
  pinMode(LEDPIN, OUTPUT);
  
  // Ensure that device is enabled by pulling control pin high for >500uS
  digitalWrite(LEDPIN, HIGH);  //on
  delayMicroseconds(500);
  
  stateCount = 0;
  
  sbi(GIMSK,PCIE); // Turn on Pin Change interrupt
  sbi(PCMSK,PCINT4); // Which pins are affected by the interrupt


}


ISR(PCINT0_vect) 
{
  modeSemaphore = !modeSemaphore;  

  if(!poweredOn)
    poweredOn = true;
}

void powerDown()
{
  for(int i = 255; i>0; i--)
  {
      for(int r = 0; r<2; r++)
      {
        softPWM(i);
      }
  }
  digitalWrite(LEDPIN, LOW);  // off
  // Force mode semaphore to false to prevent blinking/reactivation 
  // caused by button presses during the fade-out animation
  modeSemaphore = false;
  poweredOn = false;  
  sleepNow();
}



void loop()
{

  if(modeSemaphore)
  {
    digitalWrite(LEDPIN, HIGH);  //on 
    int count = 0;
    
    while(digitalRead(BUTTONPIN)==LOW)
    { 
      delay(1); 
      count++;
    }
    
    if(count>=100)
    {
      powerDown();
    }

    if(count<100 && count>2)
    {  
        digitalWrite(LEDPIN,LOW);  //off
        delay(40);
        digitalWrite(LEDPIN,HIGH);  //on
        delay(10);
        digitalWrite(LEDPIN,LOW);  //off
        delay(40);
        
       stateCount++;
        //Set default range of animationStep 
        theRoof = 254;
        theFloor = 1;
        frameTrack = 0;
      }
      
      modeSemaphore = false; // Disarm the semaphore (button press time < minimum) 
  }

  
  if(stateCount>12)
    stateCount = 0;


  
// You cant delay frame drawing. It breaks the PWM of brightness settings.  
// If insufficient time has elapsed since the last call just return and do nothing.
// Each animation needs to be calibrated using frameDelayTimer to get a good range of speeds.
//    currentMillis = millis();
//  if(currentMillis - previousMillis > globalSpeed)    
//  {
//    previousMillis = currentMillis;
//    pushFrame = true;
//  } 

    if(poweredOn)
    {
    // Go to the current mode
    switch(stateCount) 
      {
        case 0:
        {
          frameRepeat = 1;
          theFloor = 1;
          theRoof = 170;
          signaturePulse();
          break;
        }
        case 1:
        {
          frameRepeat = 1;
          morseCode();
          break;
        }
        case 2:
        {
          theFloor = 1;
          theRoof = 89;
          frameRepeat = 1;
          sinFade(animationStep);
          break;
        }
        case 3:
        {
          theFloor = 1;
          theRoof = 89;
          frameRepeat = 6;
          sinFade(animationStep);
          break;
        }
        case 4:
        {
          theFloor = 1;
          theRoof = 89;
          frameRepeat = 12;
          sinFade(animationStep);
          break;
        }
        case 5:
        {
          frameRepeat = 1;
          theFloor = 0;
          theRoof = 20;
          playMelody(animationStep);
          break;
        }
        case 6:
        {
          frameRepeat = 1;
          softPWM(255);
          break;
        }          
        case 7:
        {
          frameRepeat = 1;
          softPWM(125);
          break;
        }          
        case 8:
        {
          frameRepeat = 1;
          softPWM(25);
          break;
        }            
        case 9:
        {
          frameRepeat = 5;
          // linear fade with a pause in the middle and a fast flash
          if(animationStep == 150)
            frameRepeat = 50;
          if(animationStep > 150)
            frameRepeat = 1;
          linearFade(animationStep);
          break;
        }
        case 10:
        {
          // Must be 1
          frameRepeat = 1;
          bounce(10);
          break;
        }
        case 11:
        {
          frameRepeat = 10;
          if(animationStep > 100)
            frameRepeat = 100-(-100+(animationStep));
          pennerElasticEaseIn(animationStep, 0, 255, 1);
          break;
        }
        case 12:
        {
           // Random walk method. Very nice.
          if(animationStep<150 && growing)
          {
           if(random(10)>7)
            growing = false; 
          } 
          if(animationStep>100 && !growing)
          {
           if(random(10)>5)
            growing = true; 
           if(random(20)>15)  
             animationStep = 255;
          }
          
          // Special case speed control loop
          int k = 0;
          while(k<20)
          {
          softPWM(animationStep);
          k++;
          }
          frameRepeat = -1;
          break;
        }
        default: break;
      }
      
      // Increment animation when appropriate 
      // Animations are slowed only via repeating frames except for a couple special cases  
      if(frameTrack < frameRepeat)
      {
        frameTrack++;
      } else 
      {
        frameTrack = 0;
        if(growing)
        {
          animationStep++;
        } else {
          animationStep--; 
        }
        
      // This is the range of the current mode.
      if(animationStep>theRoof)
      {
         animationStep--;      
         growing = false;
      } 
      if(animationStep<theFloor)
      {
        animationStep++;
        growing = true;
      }
    }
    }
}

void sleepNow()
{


    poweredOn = false;
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Degine sleep type as full power down sleep
    sleep_enable();   
    mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
    mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
    sleep_mode();        //sleep now
    sleep_disable();     //fully awake now 
    poweredOn = true;

}

// Software PWM
// Range of 1-254
// DO NOT USE 0 or 255!
// software PWM function that fakes analog output
// delayMicroseconds breaks below 3 microseconds (known issue)
  void softPWM(int freq) 
  {  
    // Avoids calling delayMicroseconds(0)
    if(freq>254)
      freq = 254;
    if(freq<1)
      freq = 1;

    PORTB |= (1 << LEDPIN);
    delayMicroseconds(freq);
    PORTB &= ~(1 << LEDPIN);
    delayMicroseconds(255-freq);
    
  }


void signaturePulse()
{
  if(animationStep<75)
  {
    sinFade(animationStep);
  } else {
    sinFade(75);
  }
}


void bounce(int aRepeat) 
{
 int rangedSin = sinTable[map(animationStep, theFloor, theRoof, 0, 89)];
 int output = (int)((animationStep^2)*rangedSin);
 
 if(output>255 || output<0)
 {
 return; 
 }
 softPWM(output);
 }




void linearFade(int theAnimation) 
{  
  softPWM(theAnimation);
}


// Inverted to increase low end
// 90 values stored in the sine table
// Brightness is 0-90, bright to dim
void sinFade(int aStep) 
{  
  int sineValue = sinTable[aStep];
  softPWM(255-sineValue);
      
}

void rampUp(int rampSpeed) 
{  
  for(int fade=1;fade<254;fade++) 
  { 
    softPWM(fade);
    if(modeSemaphore)
       break;
  }

}

void pennerElasticEaseIn(int t, int b, int c, int d)
{
//  int easeIt = (int) 255/2*t*t + b;
  int easeIt = (int) -(255)/2 * ((--t)*(t-2) - 1) - b;
  softPWM(easeIt);

  
//  int ts1 = (t/d)*t;
//  int tc1 = ts1*t;  
//  int startEase = (b+c*(111*tc1*ts1 + -285*ts1*ts1 + 260*tc1 + -100*ts1 + 15*t));
//  softPWM(startEase);



//  int ts2 = ((t+1)/d)*(t+1);
//  int tc2 = ts2*(t+1);  
//  int endEase = (b+c*(111*tc2*ts2 + -285*ts2*ts2 + 260*tc2 + -100*ts2 + 15*(t+1)));
//  
//  easePWM(startEase, endEase);

}



void playMelody(int atPosition)
{
  if(atPosition > length-1)
    return;
    
  if (pgm_read_byte(&notes[atPosition]) == ' ') {
//    off();
//    delay(pgm_read_byte(&beats[atPosition]) * noteTempo); // rest
  } else {
    playNote(pgm_read_byte(&notes[atPosition]), pgm_read_byte(&beats[atPosition]) * noteTempo);
  }
}
  

void playNote(char note, int duration) {

    int noteTone = 5;

    switch(note) 
      {
        case 'c':
        {
          noteTone = 1915;
          break;
        }
        case 'd':
        {
          noteTone = 1700;
          break;
        }
        case 'e':
        {
          noteTone = 1519;
          break;
        }
        case 'f':
        {
          noteTone = 1432;
          break;
        }
        case 'g':
        {
          noteTone = 1275;
          break;
        }
        case 'a':
        {
          noteTone = 1136;
          break;
        }
        case 'b':
        {
          noteTone = 1014;
          break;
        }          
        case 'C':
        {
          noteTone = 956;
          break;
        }          
        case ' ':
        {
          noteTone = -1;
          break;
        }          

        default: 
        {
        break;
        noteTone = -1;
        }
      }

      if(noteTone > 0)
      {
        for (long t = 0; t < duration; t += 1) 
          {
            softPWM(map(noteTone, 956, 1915, 1, 255));
          }
      }

}



void morseCode()
{
  messageStart();
  while(1)
  {
  if(modeSemaphore)
    break;

  // NEBULA
  print_morsec(78);
  print_morsec(69);
  print_morsec(66);
  print_morsec(85);
  print_morsec(76);
  print_morsec(65);


  // MAKEFASHION2014
//  print_morsec(77);
//  print_morsec(65);
//  print_morsec(75);
//  print_morsec(69);
//  print_morsec(70);
//  print_morsec(65);
//  print_morsec(83);
//  print_morsec(72);
//  print_morsec(73);
//  print_morsec(79);
//  print_morsec(78);
//  print_morsec(50);
//  print_morsec(48);
//  print_morsec(49);
//  print_morsec(52);
  
  // ILOVEYOU
//  print_morsec(73);
//  print_morsec(76);
//  print_morsec(79);
//  print_morsec(86);
//  print_morsec(69);
//  print_morsec(89);
//  print_morsec(79);
//  print_morsec(85);


  }

}

//get the char from the ascii to morse table
uint8_t get_morsechar(uint8_t x) {
	return pgm_read_byte(&asciitomorse_table[x]);
}


//print out a string pause
void print_morsemessagepause()
{
    if(modeSemaphore)
      return;
    PORTB |= (1 << LEDPIN);
    delay(MPAUSEMESSAGE);
    PORTB &= ~(1 << LEDPIN);
    delay(MPAUSEMESSAGEEND);
}


//print out a char pause
void print_morsecharpause()
{
    if(modeSemaphore)
      return;
    PORTB |= (1 << LEDPIN);
    delay(MPAUSECHAR);
    PORTB &= ~(1 << LEDPIN);
    delay(MPAUSECHAREND);
}


//print out a char pause
void print_morseunknownpause()
{
    if(modeSemaphore)
      return;
  delay(MPAUSECHAR);
  delay(MPAUSECHAREND);
}


void print_morsedit()
{
    if(modeSemaphore)
      return;
    PORTB |= (1 << LEDPIN);
    delay(MPAUSEDIT);
    PORTB &= ~(1 << LEDPIN);
    delay(MPAUSEDITEND);
}


void print_morsedah()
{
    if(modeSemaphore)
      return;
    PORTB |= (1 << LEDPIN);
    delay(MPAUSEDAH);
    PORTB &= ~(1 << LEDPIN);
    delay(MPAUSEDAHEND);
}


//print a morse char
void print_morsec(uint8_t i)
{
      if(modeSemaphore)
       return;
       
	uint8_t m = MNUL;
	m = pgm_read_byte(&asciitomorse_table[i]);
	if(m != (uint8_t)MNUL)
	{
		uint8_t startbit = 0;
		for(int bit=7; bit>=0; bit--)
		{
                  if(modeSemaphore)
                  break;
			if ((m >> bit) & 0x01)
			{
				if(!startbit)
					startbit = 1; //we have found the start bit
				else
					print_morsedah(); //this is not the start bit, print a dah
			}
			else

			{
				if(startbit)
					print_morsedit(); //this is not the start bit, print a dit
			}
		}
	}
      
      if(modeSemaphore)
       return;
		print_morseunknownpause();

}



void messageStart()
{
   if(modeSemaphore)
      return;
   delay(MPAUSEMESSAGE); 
}



