/* 
  Digital VFO with Rotation Dial
 
    File:   VFOsys2.ino
    Author: JF3HZB / T.UEBO
 
    Created on July 1, 2023

  Board manager: esp32 ver. 2.0.9
  Board: ESP32 Dev Module
  Library: LovyanGFX ver. 1.1.16

  Ver. 2.00  July 1, 2023

  Ver. 2.20  Aug. 3, 2024
    1. Changed the font of digital frequency display to Nixie tube like.
    2. Added the function for dial lamp emulation.


*/

#define NAME "VFO System"
#define VERSION "Ver. 2.20"
#define ID "by JF3HZB"

#include "dial.hpp"
#include "si5351.h"
#include "driver/pcnt.h"

LGFX lcd;
LGFX_Sprite sp;
LGFX_Sprite sprites[2];
bool flip;
int sprite_height; 
DIAL dial;

#define PULSE_INPUT_PIN 16 // Rotaty Encoder A
#define PULSE_CTRL_PIN 17  // Rotaty Encoder B


/*-------------------------------------------------------
   Frequency settings
--------------------------------------------------------*/
#define init_frq 7100000       // Initial Frequncy[Hz]
int32_t offset_frq = 9000000;  // Offset Frequency[Hz]
int32_t car_frq = 8998500;     // Carrier Frequncy[Hz]
#define max_frq 200000000      // Max frequency[Hz]
#define min_frq 10000          // Min frequency[Hz]


/*----------------------------------------------------------
  Adaptive step control parameters
-----------------------------------------------------------*/
#define vth  3  	   // Velocity threshold for acceleration
#define Racc 500       // Rate of acceleration
#define Rdec 1000       // Rate of deceleration
#define freq_step 10   // Min step[Hz]
#define MAX_Step 10000 // [Hz]

float L=0.0;
float inv_Tacc=Racc*1.0e-8;
float MaxL = sqrt( (MAX_Step - freq_step)/inv_Tacc );
int32_t afstp;
int16_t RE_Count = 0;


/*----------------------------------------------------------------------------------
    Control flags
-----------------------------------------------------------------------------------*/
uint8_t f_fchange = 1;  // if frequency changed, set this flag to 1
uint8_t f_cchange = 1;  // if Car frequency and/or "f_carON" changed, set this flag to 1
uint8_t f_carON = 0;     // ON(1)/OFF(0) Car signal

int32_t Dial_frq;

int velocity = 0;
int velo[16];
int pt_v = 0;


TaskHandle_t H_Task;

/*--------------------------------------------------
   core 0   Alt_Thread()
---------------------------------------------------*/
void Alt_Thread(void *args) {
  
  delay(200);
  si5351_init();
  Dial_frq = init_frq;
  set_freq(Dial_frq);
  set_car_freq(car_frq, f_carON, 0);

  while (1)
  { 
    // Encoder
    pcnt_get_counter_value(PCNT_UNIT_0, &RE_Count);          
    #ifdef REV_ENCODER
    int count=-RE_Count;
    #else if
    int count=RE_Count;
    #endif
    pcnt_counter_clear(PCNT_UNIT_0);

    velo[pt_v]=abs(count);
    pt_v++; if(pt_v>16) pt_v=0;
    velocity = 0;
    for(int i=0; i<16; i++) velocity+=velo[i];

    // Step control     
    if(count!=0){                           
      afstp=(int32_t)(0.5 + (float)(count)*( (float)freq_step + L*L*inv_Tacc) );          
      if(velocity >= vth ) L+=1.0*(float)( velocity - vth );
      if(L>MaxL) L=MaxL;                                  
    }else{         
      L-=(float)Rdec;
      if(L<0) L=0;
    }

    // Update frequency
    if(afstp!=0){
      int32_t tfrq=Dial_frq;
      tfrq+=afstp;
      tfrq = (tfrq/freq_step)*freq_step;
      if(tfrq>max_frq) tfrq=max_frq;
      if(tfrq<min_frq) tfrq=min_frq;
      Dial_frq=tfrq;
      afstp=0;
      f_fchange = 1;                                                
    }

    // Set frequency to Si5351A 
    if(f_fchange==1){
      f_fchange=0;
      // Lo freq
      set_freq( Dial_frq + offset_frq );                
    }
    if(f_cchange==1){
      f_cchange=0;
      // Car freq            
      set_car_freq(car_frq, f_carON, 0);                         
    }

    delay(10);
  }

}



void setup(void)
{
  xTaskCreatePinnedToCore(Alt_Thread, "Alt_Thread", 8192, NULL, 4, &H_Task, 0);

  Serial.begin(115200);
  //--- Counter setup for Rotary Encoder ---------------------
  pcnt_config_t pcnt_config_A;// structure for A   
  pcnt_config_t pcnt_config_B;// structure for B
  //
  pcnt_config_A.pulse_gpio_num = PULSE_INPUT_PIN;
  pcnt_config_A.ctrl_gpio_num = PULSE_CTRL_PIN;
  pcnt_config_A.lctrl_mode = PCNT_MODE_REVERSE;
  pcnt_config_A.hctrl_mode = PCNT_MODE_KEEP;
  pcnt_config_A.channel = PCNT_CHANNEL_0;
  pcnt_config_A.unit = PCNT_UNIT_0;
  pcnt_config_A.pos_mode = PCNT_COUNT_INC;
  pcnt_config_A.neg_mode = PCNT_COUNT_DEC;
  pcnt_config_A.counter_h_lim = 10000;
  pcnt_config_A.counter_l_lim = -10000;
  //
  pcnt_config_B.pulse_gpio_num = PULSE_CTRL_PIN;
  pcnt_config_B.ctrl_gpio_num = PULSE_INPUT_PIN;
  pcnt_config_B.lctrl_mode = PCNT_MODE_KEEP;
  pcnt_config_B.hctrl_mode = PCNT_MODE_REVERSE;
  pcnt_config_B.channel = PCNT_CHANNEL_1;
  pcnt_config_B.unit = PCNT_UNIT_0;
  pcnt_config_B.pos_mode = PCNT_COUNT_INC;
  pcnt_config_B.neg_mode = PCNT_COUNT_DEC;
  pcnt_config_B.counter_h_lim = 10000;
  pcnt_config_B.counter_l_lim = -10000;
  //
  pcnt_unit_config(&pcnt_config_A);//Initialize A
  pcnt_unit_config(&pcnt_config_B);//Initialize B
  pcnt_counter_pause(PCNT_UNIT_0);
  pcnt_counter_clear(PCNT_UNIT_0);
  pcnt_counter_resume(PCNT_UNIT_0); //Start

  LCD_setup();
  lcd.setTextColor(TFT_CYAN);
  lcd.setFont(&fonts::Font0);
  lcd.setTextSize(1.0f*( lcd.height()/64.0f));
  lcd.setCursor( 0.5f*(lcd.width()-lcd.textWidth(NAME) ), 0.1f*lcd.height() );
  lcd.printf( NAME );
  lcd.setTextSize(1.0f*( lcd.height()/64.0f));
  lcd.setCursor( 0.5f*(lcd.width()-lcd.textWidth(VERSION) ), 0.4f*lcd.height());
  lcd.printf(VERSION); 
  lcd.setTextSize(1.0f*(lcd.height()/64.0f));
  lcd.setCursor( 0.5f*(lcd.width()-lcd.textWidth(ID) ), 0.7f*lcd.height());
  lcd.printf(ID);

  delay(1000);
}


void loop(void)
{
  // Display process -------------------------------------------------------------------
  for (int yoff = 0; yoff < lcd.height(); yoff += sprite_height)
  {
    sprites[flip].clear(BGCol);

    // Draw dial
    dial.draw(Dial_frq, yoff);


    // Draw digital frequency display
    #ifndef Digtal_Freq_OFF
      #ifdef MODE0
      int xdf=-2, ydf=0;// position

      char str[12];
      sprintf(str, "%03d.%03d.%02d",
                    Dial_frq/1000000, (Dial_frq%1000000)/1000, (Dial_frq%1000)/10 );
      for(int i=0; i<12; i++)
      {
        switch(str[i])
        {
          case '0':
          if( (i==0) || (i==1 && str[0]=='0') )
            sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nxb);
          else
            sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx0);
          xdf += nxfontWidth;
          break;
          case '1':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx1); xdf += nxfontWidth; break;
          case '2':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx2); xdf += nxfontWidth; break;
          case '3':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx3); xdf += nxfontWidth; break;
          case '4':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx4); xdf += nxfontWidth; break;
          case '5':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx5); xdf += nxfontWidth; break;
          case '6':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx6); xdf += nxfontWidth; break;
          case '7':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx7); xdf += nxfontWidth; break;
          case '8':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx8); xdf += nxfontWidth; break;
          case '9':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nx9); xdf += nxfontWidth; break;
          case '.':
          sprites[flip].pushImage(xdf, ydf -yoff, nxfontWidth, nxfontHeight, nxd); xdf += nxfontWidth; break;
        }
      }
      #endif
      
    #endif

    // Add draw functions if you need.
    // Note that the y-coordinate is applied with the offset -yoff added.
    //
    // ex.
    // sprites[flip].setFont(&fonts::Font4);
    // sprites[flip].setTextSize(1.0f);
    // sprites[flip].setTextColor(TFT_WHITE);
    // sprites[flip].setCursor(0, 55 -yoff);  //  "-yoff" is Required
    // sprites[flip].print("LSB");
    //
    //
  
    sprites[flip].pushSprite(&lcd, 0, yoff);
    flip = !flip;
  }
  // End of Display process -------------------------------------------------------------

}

