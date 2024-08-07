"VFOsys2" is the original version of a digital VFO with analog dial display by Tj Lab(JF3HZB),
which can be compiled on Arduino IDE with Board manager "esp32 ver. 2.0.9" and Library "LovyanGFX ver. 1.1.7 - 1.1.16".

Usable display panel:
 ST7735S (128 x 160,  80 x 160),
 ST7789 (135 x 240, 240 x 240, 240 x 320),
 ILI9341 (240 x 320),
 SSD1331 (64 x 96).


Ver. 2.00  July 1, 2023

Ver. 2.20  Aug. 3, 2024
  1. Changed the font of digital frequency display to Nixie tube like.
  2. Added the function for dial lamp emulation. 
     Lamp emulation parameters:
       #define lampX 160  // Lamp position X
       #define lampY 140  // Lamp position Y
       #define lampD 50   // Spot size(diameter)
       #define depth_LE_effect 7.5 // if 0, without Lamp emulation

Ver. 2.21  Aug. 7, 2024
  Improved display quality

Ver. 2.22  Aug. 8, 2024
  Improved display quality for ST7789
  


JF3HZB  T. Uebo
Tj Lab

https://tj-lab.org



