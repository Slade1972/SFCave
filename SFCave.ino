/*  This is a game based on an old Palm-pilot game that I used to enjoy a long long time ago.
The game was called SFCave - it was simple to learn and not so easy to master, only required
a single button, and I quite enjoyed it. The arduino version is perhaps a little more limited
but I think it's still enjoyable.

The premis is that you are 'flying' through a cave which progressively gets narrower. You cannot
adjust your speed (and it gradually increases as you go), you can only influence your altitude.

Pressing the button sends you upwards, releasing the button lets gravity take you downwards.

There is a little inertia that you have to allow for, and the gravity / antigravity has a tiny bit
of acceleration (it is not constant). The game goes until you crash into the ceiling or floor.

There is no end, the cave will go 'forever'.

Inspired by Sunflat SFCave for Palm   http://www2.sunflat.net/en/games/palmcave.html
Written for Arduino by Stephanie Maks  http://planetstephanie.net/
Ported to Arduboy by Rodney Norton / Slade
Requires an Arduino, a pushbutton, and a 128x64 ab display from Adafruit http://adafruit.com/

Note: written assuming version 1.0.1 or later of Arduino IDE
*/

#define    SFC_INERTIA            3    // higher for more inertia, lower for less inertia, 0 for no inertia
#define    SFC_MIN_WIDTH          4    // minimum width of cave
#define    SFC_MAX_WIDTH         22    // maximum width of cave (total screen width is only 32)
#define    SFC_LEVEL_RATE        75    // every N points the cave gets narrower, every 5N points the screen speeds up

#include <Arduboy2.h>
#include <EEPROM.h>

Arduboy2 ab;


unsigned int sfcScore = 0;
unsigned int sfcHighScore = 0;
char sfcWalls[32][2];
char sfcRibbon[16];
char sfcTrend = 0;
char sfcVelocity = 0;
char sfcWidth = 0;
char sfcMode = 0;
boolean sfcWinner = false;
boolean buttonPressed = false;
int eepromlocation = EEPROM_STORAGE_SPACE_START;


void sfcDrawPixel(int x, int y, boolean n) {
  if ((sfcMode < 2) && ((y < 4) || (y > 27))) return;
  x = x * 4;
  y = y * 2;
  ab.drawPixel(x, y, WHITE);
  ab.drawPixel(x + 1, y + 1, WHITE);
  ab.drawPixel(x + 2, y, WHITE);
  ab.drawPixel(x + 3, y + 1, WHITE);
  if (n) {
    ab.drawPixel(x, y + 1, WHITE);
    ab.drawPixel(x + 1, y, WHITE);
    ab.drawPixel(x + 2, y + 1, WHITE);
    ab.drawPixel(x + 3, y, WHITE);
  }
  else {
    if (y <= 0) {
      ab.drawPixel(x, 0, WHITE);
      ab.drawPixel(x + 2, 0, WHITE);
    }
    else if (y >= 63) {
      ab.drawPixel(x, 63, WHITE);
      ab.drawPixel(x + 2, 63, WHITE);
    }
  }
}

void sfcPaintScreen() {
  char x, y, z, q;
  // the following 5 delays may need to be tweaked to get a desirable / playable speed
  // depending on the speed of your Arduino and if you're using hardware spi, software spi
  // or i2c for the ab display
  // this worked ok for me on 16MHz Leonardo with software SPI for the display
  if (sfcScore > (SFC_LEVEL_RATE * 20)) {
    q = 1;
      delay(20);
  }
  else if (sfcScore > (SFC_LEVEL_RATE * 15)) {
    q = 2;
    delay(30);
  }
  else if (sfcScore > (SFC_LEVEL_RATE * 10)) {
    q = 3;
    delay(40);
  }
  else if (sfcScore > (SFC_LEVEL_RATE * 5)) {
    q = 4;
    delay(50);
  }
  else {
    q = 5;
    delay(60);
  }
  ab.clear();
  for (x = 0; x<32; x++) {
    // draw the top half of the screen
    z = 16 - sfcWalls[x][0];
    for (y = (z - q); y<z; y++) {
      sfcDrawPixel(x, y, false);
    }
    // draw the bottom half of the screen
    z = 16 - sfcWalls[x][1];
    for (y = z; y<(z + q); y++) {
      sfcDrawPixel(x, y, false);
    }
    // draw the ribbon
    if (x < 16) {
      y = 16 - sfcRibbon[x];
      sfcDrawPixel(x, y, true);
    }
  }
  ab.setTextColor(WHITE); //, BLACK
  ab.setCursor(0, 0);
  if (sfcMode != 2) {
    if (sfcScore < 10000) ab.write(48); // zero
    if (sfcScore < 1000)  ab.write(48); // zero
    if (sfcScore < 100)   ab.write(48); // zero
    if (sfcScore < 10)    ab.write(48); // zero
    ab.print(sfcScore, DEC);
    if (sfcWinner) {
      ab.setTextBackground(WHITE);
      ab.setTextColor(BLACK);
      ab.setCursor(0, 0);
      ab.write(32); // space
      ab.write(32); // space
      ab.print(F("High Score!"));
      ab.write(32); // space
      ab.write(32); // space
      ab.write(32); // space
      ab.write(32); // space
      ab.write(32); // space
    }
    ab.setCursor(98, 0);
    if (sfcHighScore < 10000) ab.write(48); // zero
    if (sfcHighScore < 1000)  ab.write(48); // zero
    if (sfcHighScore < 100)   ab.write(48); // zero
    if (sfcHighScore < 10)    ab.write(48); // zero
    ab.print(sfcHighScore, DEC);
    ab.setTextColor(WHITE);
    ab.setTextBackground(BLACK);
  }
  else {
    ab.print(sfcScore, DEC);
  }
  if (sfcMode == 0) {
    ab.setCursor(37, 56);
    ab.print(F("Game Over"));
  }
  else if (sfcMode == 1) {
    ab.setCursor(10, 57);
    ab.print(F("Press A Btn to Start"));
  }
  ab.display();
}

char sfcStepWalls(char lastValue) {
  if (random(100) < 30) sfcTrend = sfcTrend * -1;
  char newValue = lastValue + sfcTrend;
  if (newValue <  (-14 + sfcWidth)) {
    newValue = (-14 + sfcWidth);
    sfcTrend = 1;
  }
  if (newValue > 14) {
    newValue = 14;
    sfcTrend = -1;
  }
  return newValue;
}

void sfcWallsInit() {
  char x;
  // initialize the ribbon
  for (x = 0; x<16; x++) {
    sfcRibbon[x] = 0;
  }
  // initialize the screen at max width
  sfcWidth = SFC_MAX_WIDTH;
  sfcTrend = 1;
  for (x = 0; x<32; x++) {
    if (x == 0) {
      sfcWalls[x][0] = SFC_MAX_WIDTH / 2;
      sfcWalls[x][1] = sfcWalls[x][0] - SFC_MAX_WIDTH;
    }
    else {
      sfcWalls[x][0] = sfcStepWalls(sfcWalls[x - 1][0]);
      sfcWalls[x][1] = sfcWalls[x][0] - sfcWidth;
    }
  }
  sfcWinner = false;
  sfcMode = 1;
  //  sfcScore = 0;
  sfcVelocity = 2; // start off with a bit of velocity
  sfcPaintScreen();
}

void sfcGamePlay() {
  char x, y;
  char newRibbon = sfcRibbon[15];
  boolean testButton = !digitalRead(A_BUTTON);
  // set velocity
  if (testButton) {
    sfcVelocity++;
  }
  else {
    sfcVelocity--;
  }
  if (sfcVelocity > SFC_INERTIA) {
    sfcVelocity = SFC_INERTIA;
    newRibbon++;      // increases difficulty!
  }
  if (sfcVelocity < -SFC_INERTIA) {
    sfcVelocity = -SFC_INERTIA;
    newRibbon--;      // increases difficulty!
  }
  // get next ribbon position
  if (sfcVelocity > 0) newRibbon++;
  if (sfcVelocity < 0) newRibbon--;
  if (newRibbon > 16) newRibbon = 16;
  if (newRibbon < -15) newRibbon = -15;
  // get next screen column
  char newRoof = sfcStepWalls(sfcWalls[31][0]);
  char newFloor = newRoof - sfcWidth;
  // advance the screen one column
  for (x = 0; x<31; x++) {
    sfcWalls[x][0] = sfcWalls[x + 1][0];
    sfcWalls[x][1] = sfcWalls[x + 1][1];
    if (x<15) sfcRibbon[x] = sfcRibbon[x + 1];
  }
  sfcWalls[31][0] = newRoof;
  sfcWalls[31][1] = newFloor;
  sfcRibbon[15] = newRibbon;
  // test for game over
  if (newRibbon >= sfcWalls[15][0]) sfcMode = 0; // crashed!
  if (newRibbon <= sfcWalls[15][1]) sfcMode = 0; // crashed!
  if (sfcMode == 0) {
    buttonPressed = false; // fixes button glitches when game is over
    if (sfcScore > sfcHighScore) {
      sfcHighScore = sfcScore;
      sfcWinner = true;
      // Save High Score here
      EEPROM.put(eepromlocation, sfcHighScore);
    }
    sfcPaintScreen();
    delay(500); // little pause to relax and stop button mashing
  }
  else {
    sfcPaintScreen();
    sfcScore++;
    if ((sfcWidth > SFC_MIN_WIDTH) && ((sfcScore % SFC_LEVEL_RATE) == 0)) sfcWidth--;
  }
}

void setup() {
  ab.begin();
  EEPROM.get(eepromlocation, sfcHighScore);
  ab.display();
  ab.setTextWrap(false);
  ab.initRandomSeed();
  delay(500);
  sfcWallsInit();

}

void loop() {
  if (sfcMode == 2) {
    sfcGamePlay();
  }
  else {
    boolean testButton = !digitalRead(A_BUTTON);
    if (testButton != buttonPressed) {
      if (buttonPressed) {
        if (sfcMode == 0) {
          sfcWallsInit();
        }
        else if (sfcMode == 1) {
          sfcMode = 2;
          sfcScore = 0;
        }
      }
      buttonPressed = testButton;
    }
  }
}

