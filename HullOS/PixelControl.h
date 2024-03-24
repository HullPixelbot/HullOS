#include <Adafruit_NeoPixel.h>

#define PIXELS 12
#define VERSION 1

#define NO_OF_LIGHTS 12

#define TICK_INTERVAL 20

unsigned long tickEnd;

#define NO_OF_GAPS 32

#define NEOPIN 12

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

//#define SERIAL_VERBOSE

//#define DISPLAY_LIGHT_SETTINGS

byte lightBrightness = 100;

// can be used to stop flickering lights
bool flickerActive;

// bool to force an update if the lights have changed
bool forceLightUpdate;


typedef enum lightStates
{
	lightStateOff,
	lightStateColourBounce,
	lightStateFlickerFixed,
	lightStateFlickerRandom,
	lightStateSteady
};

struct Light {
	byte r, rMax, rMin;
	int8_t rUpdate;
	byte g, gMax, gMin;
	int8_t gUpdate;
	byte b, bMax, bMin;
	int8_t bUpdate;
	byte colourSpeed;
	byte flickerBrightness;
	int8_t flickerUpdate;
	byte flickerSpeed;
	byte flickerMin;
	byte flickerMax;
	int pos, posMax, posMin;
	int8_t moveDist;
	byte moveSpeed;
	lightStates lightState;
} lights[NO_OF_LIGHTS];

int tickCount;

bool randomColourTransitions = false;

byte oldr = 0, oldg = 0, oldb = 0;

void resetOldFlickerValues()
{
	oldr = 0;
	oldb = 0;
	oldg = 0;
}

void setLightColor(byte r, byte g, byte b)
{
	byte i;
	for (i = 0; i < NO_OF_LIGHTS; i++)
	{
		lights[i].pos = (int)((float)i / NO_OF_LIGHTS * (PIXELS*NO_OF_GAPS));
		lights[i].r = r;
		lights[i].rMax = r;
		lights[i].rMin = r;
		lights[i].g = g;
		lights[i].gMax = g;
		lights[i].gMin = g;
		lights[i].b = b;
		lights[i].bMax = b;
		lights[i].bMin = b;
		lights[i].lightState = lightStateSteady;
	}
}

void setAllLilac()
{
	setLightColor(220, 208, 255);
}

void randomiseLight(byte lightNo)
{
	lights[lightNo].pos = (int)((float)lightNo / NO_OF_LIGHTS * (PIXELS*NO_OF_GAPS));

	lights[lightNo].r = (byte)random(0, 256);
	lights[lightNo].rMax = (byte)random(lights[lightNo].r, 256);
	lights[lightNo].rMin = (byte)random(0, lights[lightNo].r);

	lights[lightNo].g = (byte)random(0, 256);
	lights[lightNo].gMax = (byte)random(lights[lightNo].g, 256);
	lights[lightNo].gMin = (byte)random(0, lights[lightNo].g);

	lights[lightNo].b = (byte)random(0, 256);
	lights[lightNo].bMax = (byte)random(lights[lightNo].b, 256);
	lights[lightNo].bMin = (byte)random(0, lights[lightNo].b);

	lights[lightNo].rUpdate = (int8_t)random(-3, 4);
	lights[lightNo].gUpdate = (int8_t)random(-3, 4);
	lights[lightNo].bUpdate = (int8_t)random(-3, 4);

	lights[lightNo].colourSpeed = (byte)random(100, 256);

	lights[lightNo].moveDist = (int8_t)random(-3, 4);
	lights[lightNo].moveSpeed = (byte)random(1, 100);
	lights[lightNo].posMax = (int)random(0, PIXELS*NO_OF_GAPS);
	lights[lightNo].posMin = (int)random(0, lights[lightNo].posMax);
	lights[lightNo].lightState = lightStateColourBounce;
}

void randomiseLights()
{
	byte i;
	for (i = 0; i < NO_OF_LIGHTS; i++)
	{
		randomiseLight(i);
	}
	// force an update if we go into candle mode later
	resetOldFlickerValues();
	forceLightUpdate = true;
}

void setAllLightsOff()
{
	for (byte i = 0; i < NO_OF_LIGHTS; i++)
	{
		lights[i].lightState = lightStateOff;
	}
	// force an update if we go into candle mode later
	resetOldFlickerValues();
	forceLightUpdate = true;
}

void startLights()
{
#ifdef SERIAL_VERBOSE
	Serial.println(".Starting lights");
#endif  
	// Turn on flickering
	flickerActive = true;
	// Turn off force update
	forceLightUpdate = false;
	strip.begin();
	strip.show(); // Initialize all pixels to 'off'
	setAllLightsOff();
	strip.show();
}

void do_lightsOff()
{
#ifdef SERIAL_VERBOSE
	Serial.println("Lights Off command");
#endif  
	setAllLightsOff();
}

void copyBlock(byte * dest, byte * src, int length)
{
	for (int i = 0; i < length; i++)
	{
		*dest = *src;
		dest++;
		src++;
	}
}

void renderLight(int lightNo)
{
	if (lights[lightNo].lightState == lightStateOff) return;

	int pos = lights[lightNo].pos;
	byte firstLight = pos / NO_OF_GAPS;
	byte secondLight = firstLight + 1;
	if (secondLight == PIXELS) secondLight = 0;
	byte positionInGap = pos % NO_OF_GAPS;
	float secondFactor = (float)positionInGap / NO_OF_GAPS;
	float firstFactor = 1 - secondFactor;
	float flickerFactor = (float)lights[lightNo].flickerBrightness / 255;
	float brightnessFactor = (float)lightBrightness / 255;

#ifdef DISPLAY_LIGHT_SETTINGS
	Serial.print("Rendering Light ");
	Serial.println(lightNo);
	Serial.print("Colour: ");
	Serial.print(lights[lightNo].r);
	Serial.print(" ");
	Serial.print(lights[lightNo].g);
	Serial.print(" ");
	Serial.println(lights[lightNo].b);
	Serial.print("firstLight:  ");
	Serial.println(firstLight);
	Serial.print("secondLight:  ");
	Serial.println(secondLight);
	Serial.print("Position:  ");
	Serial.println(lights[lightNo].pos);
	Serial.print("positionInGap:  ");
	Serial.println(positionInGap);
	Serial.print("secondFactor:  ");
	Serial.println(secondFactor);
	Serial.print("firstFactor:  ");
	Serial.println(firstFactor);
	Serial.print("flickerFactor:  ");
	Serial.println(flickerFactor);
	byte rv = lights[lightNo].r*firstFactor*flickerFactor*brightnessFactor;
	Serial.print("First factor: ");
	Serial.print(firstFactor);
	Serial.print("Flicker factor: ");
	Serial.print(flickerFactor);
	Serial.print("Brightness factor: ");
	Serial.print(brightnessFactor);
	Serial.print("Brightness: ");
	Serial.println(rv);
	//delay(2000);

#endif 

	strip.setPixelColor(firstLight,
		(byte)(lights[lightNo].r*firstFactor*flickerFactor*brightnessFactor),
		(byte)(lights[lightNo].g*firstFactor*flickerFactor*brightnessFactor),
		(byte)(lights[lightNo].b*firstFactor*flickerFactor*brightnessFactor));

	if (positionInGap != 0) {
		strip.setPixelColor(secondLight,
			(byte)(lights[lightNo].r*secondFactor*brightnessFactor),
			(byte)(lights[lightNo].g*secondFactor*brightnessFactor),
			(byte)(lights[lightNo].b*secondFactor*brightnessFactor));
	}
}

void steadyLight(int position, struct Light * l)
{
	(*l).pos = position;
	(*l).moveSpeed = 0;
	(*l).rUpdate = 0;
	(*l).gUpdate = 0;
	(*l).bUpdate = 0;
	(*l).colourSpeed = 0;
	(*l).flickerSpeed = 0;
	(*l).flickerBrightness = 255;
	(*l).lightState = lightStateSteady;
}

void colouredSteadyLight(byte r, byte g, byte b, int position, struct Light * l)
{
	(*l).r = r;
	(*l).g = g;
	(*l).b = b;
	steadyLight(position, l);
}

void setLightColor(byte r, byte g, byte b, byte lightNo)
{
	colouredSteadyLight(r, g, b, lightNo*NO_OF_GAPS, &lights[lightNo]);
	forceLightUpdate = true;
}

void flickeringLight(byte flickerBrightness, byte flickerUpdate, byte flickerMin, byte flickerMax, byte flickerSpeed, int position, struct Light * l)
{
	(*l).flickerBrightness = flickerBrightness;
	(*l).flickerUpdate = flickerUpdate;
	(*l).flickerMin = flickerMin;
	(*l).flickerMax = flickerMax;
	(*l).flickerSpeed = flickerSpeed;
	(*l).pos = position;
	(*l).moveSpeed = 0;
	(*l).rUpdate = 0;
	(*l).gUpdate = 0;
	(*l).bUpdate = 0;
	(*l).colourSpeed = 0;
	(*l).lightState = lightStateFlickerFixed;
}

void colouredFlickeringLight(byte r, byte g, byte b, byte flickerBrightness, byte flickerUpdate, byte flickerMin, byte flickerMax, byte flickerSpeed, int position, struct Light * l)
{
	(*l).r = r;
	(*l).g = g;
	(*l).b = b;
	flickeringLight(flickerBrightness, flickerUpdate, flickerMin, flickerMax, flickerSpeed, position, l);
}

void flickerOn()
{
	flickerActive = true;
}

void flickerOff()
{
	flickerActive = false;
}

void renderLights()
{
	for (uint16_t i = 0; i < strip.numPixels(); i++) {
		strip.setPixelColor(i, 0, 0, 0);
	}

	for (int i = 0; i < NO_OF_LIGHTS; i++)
	{
		renderLight(i);
	}

	if (forceLightUpdate)
	{
		strip.show();
		forceLightUpdate = false;
	}

	if (flickerActive)
	{
		strip.show();
	}
}

void updateLightColours(byte i)
{
	if (lights[i].colourSpeed == 0)
		return;

	if ((tickCount % lights[i].colourSpeed) != 0)
		return;

	/// going to 'bounce' the colours when they hit the endstops
	int temp;

	//if(i==0){

	//  Serial.print("r"); Serial.println(lights[i].r);
	//  Serial.print("rUpdate"); Serial.println(lights[i].rUpdate);
	//  Serial.print("rMax"); Serial.println(lights[i].rMax);
	//  Serial.print("rMin"); Serial.println(lights[i].rMin);
	//  delay(2000);
	//}

	if (lights[i].rUpdate != 0)
	{
		// calculate the update value - use an int becuase we need negative and > 255
		temp = lights[i].r;
		temp += lights[i].rUpdate;

		if (lights[i].rMax == lights[i].rMin)
		{
			// doing a transition
			if (lights[i].rUpdate < 0)
			{
				// heading down towards the limit
				if (temp <= lights[i].rMin)
				{
					// hit the end condition
					// clamp the value
					lights[i].r = lights[i].rMin;
					// stop any further updates
					lights[i].rUpdate = 0;
				}
				else {
					lights[i].r = temp;
				}
			}
			else
			{
				// heading up towards the limit
				if (temp >= lights[i].rMax)
				{
					// hit the end condition
					// clamp the value
					lights[i].r = lights[i].rMax;
					// stop any further updates
					lights[i].rUpdate = 0;
				}
				else {
					lights[i].r = temp;
				}
			}
		}
		else
		{
			// performing a normal animation 
			// reverse the direction when the limits are reached
			if (temp <= lights[i].rMin)
			{
				lights[i].r = lights[i].rMin;
				lights[i].rUpdate = -lights[i].rUpdate;
			}
			else {
				if (temp >= lights[i].rMax)
				{
					lights[i].r = lights[i].rMax;
					lights[i].rUpdate = -lights[i].rUpdate;
				}
			}
		}
	}

	if (lights[i].gUpdate != 0)
	{
		// calculate the update value - use an int becuase we need negative and > 255
		temp = lights[i].g;
		temp += lights[i].gUpdate;

		if (lights[i].gMax == lights[i].gMin)
		{
			// doing a transition
			if (lights[i].gUpdate < 0)
			{
				// heading down towards the limit
				if (temp <= lights[i].gMin)
				{
					// hit the end condition
					// clamp the value
					lights[i].g = lights[i].gMin;
					// stop any further updates
					lights[i].gUpdate = 0;
				}
				else {
					lights[i].g = temp;
				}
			}
			else
			{
				// heading up towards the limit
				if (temp >= lights[i].gMax)
				{
					// hit the end condition
					// clamp the value
					lights[i].g = lights[i].gMax;
					// stop any further updates
					lights[i].gUpdate = 0;
				}
				else {
					lights[i].g = temp;
				}
			}
		}
		else
		{
			// performing a normal animation 
			// reverse the direction when the limits are reached
			if (temp <= lights[i].gMin)
			{
				lights[i].g = lights[i].gMin;
				lights[i].gUpdate = -lights[i].gUpdate;
			}
			else {
				if (temp >= lights[i].gMax)
				{
					lights[i].g = lights[i].gMax;
					lights[i].gUpdate = -lights[i].gUpdate;
				}
			}
		}
	}

	if (lights[i].bUpdate != 0)
	{
		// calculate the update value - use an int becuase we need negative and > 255
		temp = lights[i].b;
		temp += lights[i].bUpdate;

		if (lights[i].bMax == lights[i].bMin)
		{
			// doing a transition
			if (lights[i].bUpdate < 0)
			{
				// heading down towards the limit
				if (temp <= lights[i].bMin)
				{
					// hit the end condition
					// clamp the value
					lights[i].b = lights[i].bMin;
					// stop any further updates
					lights[i].bUpdate = 0;
				}
				else {
					lights[i].b = temp;
				}
			}
			else
			{
				// heading up towards the limit
				if (temp >= lights[i].bMax)
				{
					// hit the end condition
					// clamp the value
					lights[i].b = lights[i].bMax;
					// stop any further updates
					lights[i].bUpdate = 0;
				}
				else {
					lights[i].b = temp;
				}
			}
		}
		else
		{
			// performing a normal animation 
			// reverse the direction when the limits are reached
			if (temp <= lights[i].bMin)
			{
				lights[i].b = lights[i].bMin;
				lights[i].bUpdate = -lights[i].bUpdate;
			}
			else {
				if (temp >= lights[i].bMax)
				{
					lights[i].b = lights[i].bMax;
					lights[i].bUpdate = -lights[i].bUpdate;
				}
			}
		}
	}
}

bool transitionComplete()
{
	for (byte i = 0; i < NO_OF_LIGHTS; i++)
	{
		if (lights[i].rUpdate != 0 || lights[i].gUpdate != 0 || lights[i].bUpdate != 0)
			return 0;
	}
	return 1;
}

void updateLightPosition(byte i)
{
	if (lights[i].moveSpeed == 0)
		return;

	if ((tickCount % lights[i].moveSpeed) != 0)
		return;

	lights[i].pos += lights[i].moveSpeed;

	if (lights[i].pos >= PIXELS * NO_OF_GAPS)
	{
		lights[i].pos -= (PIXELS*NO_OF_GAPS);
	}
	else {
		if (lights[i].pos < 0)
		{
			lights[i].pos += (PIXELS*NO_OF_GAPS);
		}
	}
}

int flickerUpdateSpeed = 8;

void setFlickerUpdateSpeed(byte speed)
{
	// clamp the value
	if (speed > 20)
		speed = 20;
	if (speed < 1)
		speed = 1;

	// set the new flicker update speed
	flickerUpdateSpeed = 21 - speed;

}

void updateLightFlicker(byte i)
{
	if (lights[i].flickerSpeed == 0)
		return;

	if (lights[i].flickerUpdate == 0)
		return; // quit if not flickering

	if ((tickCount % lights[i].flickerSpeed) != 0)
		return;

	/// going to 'bounce' the flicker when it hits the endstops
	int temp = lights[i].flickerBrightness;

	temp += lights[i].flickerUpdate;

	if (temp <= lights[i].flickerMin)
	{
		// clamp the value at the lower limit
		lights[i].flickerBrightness = lights[i].flickerMin;

		// reverse direction
		lights[i].flickerUpdate = -lights[i].flickerUpdate;

		// The faster the flicker rate, the more likely a change
		if (random(0, flickerUpdateSpeed / 2) < 2)
		{
			// change the flicker speed
			lights[i].flickerUpdate = random(1, (int)(lights[i].flickerMax - lights[i].flickerMin) / flickerUpdateSpeed);
		}
	}
	else {
		if (temp >= lights[i].flickerMax)
		{
			// just clamp and reverse
			lights[i].flickerBrightness = lights[i].flickerMax;
			lights[i].flickerUpdate = -lights[i].flickerUpdate;
		}
		else
		{
			lights[i].flickerBrightness = (byte)temp;
		}
	}
}

// Start the transition of a light to a new colour

void startLightTransition(byte lightNo, byte speed, byte colourSpeed, byte r, byte g, byte b)
{

	colouredFlickeringLight(
		lights[lightNo].r, lights[lightNo].g, lights[lightNo].b,     // colour
		random(1, lights[lightNo].flickerMax - lights[lightNo].flickerMin),//60,            // flicker brightness
		random(1, (int)(lights[lightNo].flickerMax - lights[lightNo].flickerMin) / flickerUpdateSpeed),            // flicker update step
		lights[lightNo].flickerMin,            // flicker minimum
		lights[lightNo].flickerMax,           // flicker maximum
		1,             // number of ticks per flicker update - flicker speed
		NO_OF_GAPS*lightNo,  // position on the ring
		&lights[lightNo]);   // ligit to make flicker

	lights[lightNo].colourSpeed = colourSpeed;
	lights[lightNo].rMin = r;
	lights[lightNo].rMax = r;

	if (lights[lightNo].r > r)
	{
		lights[lightNo].rUpdate = (int8_t)-((lights[lightNo].r - r) / speed);
		if (lights[lightNo].rUpdate == 0) lights[lightNo].rUpdate = -1;
	}
	else
	{
		lights[lightNo].rUpdate = (int8_t)((r - lights[lightNo].r) / speed);
		if (lights[lightNo].rUpdate == 0) lights[lightNo].rUpdate = 1;
	}

	lights[lightNo].gMin = g;
	lights[lightNo].gMax = g;

	if (lights[lightNo].g > g)
	{
		lights[lightNo].gUpdate = (int8_t)-((lights[lightNo].g - g) / speed);
		if (lights[lightNo].gUpdate == 0) lights[lightNo].gUpdate = -1;
	}
	else
	{
		lights[lightNo].gUpdate = (int8_t)((g - lights[lightNo].g) / speed);
		if (lights[lightNo].gUpdate == 0) lights[lightNo].gUpdate = 1;
	}

	lights[lightNo].bMin = b;
	lights[lightNo].bMax = b;

	if (lights[lightNo].b > b)
	{
		lights[lightNo].bUpdate = (int8_t)-((lights[lightNo].b - b) / speed);
		if (lights[lightNo].bUpdate == 0) lights[lightNo].bUpdate = -1;
	}
	else
	{
		lights[lightNo].bUpdate = (int8_t)((b - lights[lightNo].b) / speed);
		if (lights[lightNo].bUpdate == 0) lights[lightNo].bUpdate = 1;
	}
}

/* Colour Codes

Alice Blue,240,248,255,#f0f8ff
Antique White,250,235,215,#faebd7
Aqua,0,255,255,#00ffff
Aquamarine,127,255,212,#7fffd4
Azure,240,255,255,#f0ffff
Beige,245,245,220,#f5f5dc
Bisque,255,228,196,#ffe4c4
Black* 0,0,0,#000000
Blanched Almond,255,235,205,#ffebcd
Blue*,0,0,255,#0000ff
Blue-Violet,138,43,226,#8a2be2
Brown,165,42,42,#a52a2a
Burlywood,222,184,135,#deb887
Cadet Blue,95,158,160,#5f9ea0
Chartreuse,127,255,0,#7fff00
Chocolate,210,105,30,#d2691e
Coral,255,127,80,#ff7f50
Cornflower Blue,100,149,237,#6495ed
Cornsilk,255,248,220,#fff8dc
Cyan,0,255,255,#00ffff
Dark Blue,0,0,139,#00008b
Dark Cyan,0,139,139,#008b8b
Dark Goldenrod,184,134,11,#b8860b
Dark Gray,169,169,169,#a9a9a9
Dark Green,0,100,0,#006400
Dark Khaki,189,183,107,#bdb76b
Dark Magenta,139,0,139,#8b008b
Dark Olive Green,85,107,47,#556b2f
Dark Orange,255,140,0,#ff8c00
Dark Orchid,153,50,204,#9932cc
Dark Red,139,0,0,#8b0000
Dark Salmon,233,150,122,#e9967a
Dark Sea Green,143,188,143,#8fbc8f
Dark Slate Blue,72,61,139,#483d8b
Dark Slate Gray,47,79,79,#2f4f4f
Dark Turquoise,0,206,209,#00ced1
Dark Violet,148,0,211,#9400d3
Deep Pink,255,20,147,#ff1493
Deep Sky Blue,0,191,255,#00bfff
Dim Gray,105,105,105,#696969
Dodger Blue,30,144,255,#1e90ff
Firebrick,178,34,34,#b22222
Floral White,255,250,240,#fffaf0
Forest Green,34,139,34,#228b22
Fuschia* 255,0,255,#ff00ff
Gainsboro,220,220,220,#dcdcdc
Ghost White,255,250,250,#f8f8ff
Gold,255,215,0,#ffd700
Goldenrod,218,165,32,#daa520
Gray*,128,128,128,#808080
Green* 0,128,0,#008000
Green-Yellow,173,255,47,#adff2f
Honeydew,240,255,240,#f0fff0
Hot Pink,255,105,180,#ff69b4
Indian Red,205,92,92,#cd5c5c
Ivory,255,255,240,#fffff0
Khaki,240,230,140,#f0e68c
Lavender,230,230,250,#e6e6fa
Lavender Blush,255,240,245,#fff0f5
Lawn Green,124,252,0,#7cfc00
Lemon Chiffon,255,250,205,#fffacd
Light Blue,173,216,230,#add8e6
Light Coral,240,128,128,#f08080
Light Cyan,224,255,255,#e0ffff
Light Goldenrod,238,221,130,#eedd82
Light Goldenrod Yellow,250,250,210,#fafad2
Light Gray,211,211,211,#d3d3d3
Light Green,144,238,144,#90ee90
Light Pink,255,182,193,#ffb6c1
Light Salmon,255,160,122,#ffa07a
Light Sea Green,32,178,170,#20b2aa
Light Sky Blue,135,206,250,#87cefa
Light Slate Blue,132,112,255,#8470ff
Light Slate Gray,119,136,153,#778899
Light Steel Blue,176,196,222,#b0c4de
Light Yellow,255,255,224,#ffffe0
Lime*,0,255,0,#00ff00
Lime Green,50,205,50,#32cd32
Linen,250,240,230,#faf0e6
Magenta,255,0,255,#ff00ff
Maroon* 128,0,0,#800000
Medium Aquamarine,102,205,170,#66cdaa
Medium Blue,0,0,205,#0000cd
Medium Orchid,186,85,211,#ba55d3
Medium Purple,147,112,219,#9370db
Medium Sea Green,60,179,113,#3cb371
Medium Slate Blue,123,104,238,#7b68ee
Medium Spring Green,0,250,154,#00fa9a
Medium Turquoise,72,209,204,#48d1cc
Medium Violet-Red,199,21,133,#c71585
Midnight Blue,25,25,112,#191970
Mint Cream,245,255,250,#f5fffa
Misty Rose,255,228,225,#e1e4e1
Moccasin,255,228,181,#ffe4b5
Navajo White,255,222,173,#ffdead
Navy*,0,0,128,#000080
Old Lace,253,245,230,#fdf5e6
Olive* 128,128,0,#808000
Olive Drab,107,142,35,#6b8e23
Orange,255,165,0,#ffa500
Orange-Red,255,69,0,#ff4500
Orchid,218,112,214,#da70d6
Pale Goldenrod,238,232,170,#eee8aa
Pale Green,152,251,152,#98fb98
Pale Turquoise,175,238,238,#afeeee
Pale Violet-Red,219,112,147,#db7093
Papaya Whip,255,239,213,#ffefd5
Peach Puff,255,218,185,#ffdab9
Peru,205,133,63,#cd853f
Pink,255,192,203,#ffc0cb
Plum,221,160,221,#dda0dd
Powder Blue,176,224,230,#b0e0e6
Purple* 128,0,128,#800080
Red*,255,0,0,#ff0000
Rosy Brown,188,143,143,#bc8f8f
Royal Blue,65,105,225,#4169e1
Saddle Brown,139,69,19,#8b4513
Salmon,250,128,114,#fa8072
Sandy Brown,244,164,96,#f4a460
Sea Green,46,139,87,#2e8b57
Seashell,255,245,238,#fff5ee
Sienna,160,82,45,#a0522d
Silver* 192,192,192,#c0c0c0
Sky Blue,135,206,235,#87ceeb
Slate Blue,106,90,205,#6a5acd
Slate Gray,112,128,144,#708090
Snow,255,250,250,#fffafa
Spring Green,0,255,127,#00ff7f
Steel Blue,70,130,180,#4682b4
Tan,210,180,140,#d2b48c
Teal*,0,128,128,#008080
Thistle,216,191,216,#d8bfd8
Tomato,255,99,71,#ff6347
Turquoise,64,224,208,#40e0d0
Violet,238,130,238,#ee82ee
Violet-Red,208,32,144,#d02090
Wheat,245,222,179,#f5deb3
White* 255,255,255,#ffffff
White Smoke,245,245,245,#f5f5f5
Yellow* 255,255,0,#ffff00
Yellow-Green,154,205,50,#9acd32,
*/

enum lightColor
{
	red,
	blue,
	green,
	lilac,
	cyan,
	pink,
	lavender,
	plum,
	lime,
	orange,
	powder_blue,
	purple,
	teal
};

void selectColour(lightColor color, byte *r, byte *g, byte *b)
{
	switch (color)
	{
	case red: // red
#ifdef SERIAL_VERBOSE
		Serial.println("Red");
#endif
		(*r) = 255; (*g) = 0; (*b) = 0;
		break;
	case blue: // blue
#ifdef SERIAL_VERBOSE
		Serial.println("Blue");
#endif
		(*r) = 0; (*g) = 0; (*b) = 255;
		break;
	case green: // green
#ifdef SERIAL_VERBOSE
		Serial.println("Green");
#endif
		(*r) = 0; (*g) = 255; (*b) = 0;
		break;
	case lilac: // lilac
#ifdef SERIAL_VERBOSE
		Serial.println("Lilac");
#endif
		(*r) = 220; (*g) = 208; (*b) = 255;
		break;
	case cyan: // cyan
#ifdef SERIAL_VERBOSE
		Serial.println("cyan");
#endif
		(*r) = 0; (*g) = 255; (*b) = 255;
		break;
	case pink: // hot pink
#ifdef SERIAL_VERBOSE
		Serial.println("hot pink");
#endif
		(*r) = 255; (*g) = 105; (*b) = 180;
		break;
	case lavender: // lavender
#ifdef SERIAL_VERBOSE
		Serial.println("lavender");
#endif
		(*r) = 230; (*g) = 230; (*b) = 250;
		break;
	case plum: // plum
#ifdef SERIAL_VERBOSE
		Serial.println("plum");
#endif
		(*r) = 221; (*g) = 160; (*b) = 221;
		break;
	case lime: // lime
#ifdef SERIAL_VERBOSE
		Serial.println("lime");
#endif
		(*r) = 50; (*g) = 205; (*b) = 50;
		break;
	case orange: // orange
#ifdef SERIAL_VERBOSE
		Serial.println("orange");
#endif
		(*r) = 255; (*g) = 165; (*b) = 0;
		break;
	case powder_blue: // powder blue
#ifdef SERIAL_VERBOSE
		Serial.println("powder blue");
#endif
		(*r) = 176; (*g) = 244; (*b) = 230;
		break;
	case purple: // purple
#ifdef SERIAL_VERBOSE
		Serial.println("purple");
#endif
		(*r) = 128; (*g) = 0; (*b) = 128;
		break;
	case teal: // teal
#ifdef SERIAL_VERBOSE
		Serial.println("teal");
#endif
		(*r) = 0; (*g) = 128; (*b) = 128;
		break;
	}
}

void pickRandomColour(byte *r, byte *g, byte *b)
{
	switch (random(0, 13))
	{
	case 0: // red
#ifdef SERIAL_VERBOSE
		Serial.println("Red");
#endif
		(*r) = 255; (*g) = 0; (*b) = 0;
		break;
	case 1: // blue
#ifdef SERIAL_VERBOSE
		Serial.println("Blue");
#endif
		(*r) = 0; (*g) = 0; (*b) = 255;
		break;
	case 2: // green
#ifdef SERIAL_VERBOSE
		Serial.println("Green");
#endif
		(*r) = 0; (*g) = 255; (*b) = 0;
		break;
	case 3: // lilac
#ifdef SERIAL_VERBOSE
		Serial.println("Lilac");
#endif
		(*r) = 220; (*g) = 208; (*b) = 255;
		break;
	case 4: // cyan
#ifdef SERIAL_VERBOSE
		Serial.println("cyan");
#endif
		(*r) = 0; (*g) = 255; (*b) = 255;
		break;
	case 5: // hot pink
#ifdef SERIAL_VERBOSE
		Serial.println("hot pink");
#endif
		(*r) = 255; (*g) = 105; (*b) = 180;
		break;
	case 6: // lavender
#ifdef SERIAL_VERBOSE
		Serial.println("lavender");
#endif
		(*r) = 230; (*g) = 230; (*b) = 250;
		break;
	case 7: // plum
#ifdef SERIAL_VERBOSE
		Serial.println("plum");
#endif
		(*r) = 221; (*g) = 160; (*b) = 221;
		break;
	case 8: // lime
#ifdef SERIAL_VERBOSE
		Serial.println("lime");
#endif
		(*r) = 50; (*g) = 205; (*b) = 50;
		break;
	case 9: // orange
#ifdef SERIAL_VERBOSE
		Serial.println("orange");
#endif
		(*r) = 255; (*g) = 165; (*b) = 0;
		break;
	case 10: // powder blue
#ifdef SERIAL_VERBOSE
		Serial.println("powder blue");
#endif
		(*r) = 176; (*g) = 244; (*b) = 230;
		break;
	case 11: // purple
#ifdef SERIAL_VERBOSE
		Serial.println("purple");
#endif
		(*r) = 128; (*g) = 0; (*b) = 128;
		break;
	case 12: // teal
#ifdef SERIAL_VERBOSE
		Serial.println("teal");
#endif
		(*r) = 0; (*g) = 128; (*b) = 128;
		break;
	}
}


void transitionToColor(byte speed, byte r, byte g, byte b)
{
	for (byte i = 0; i < NO_OF_LIGHTS; i++)
		startLightTransition(i, 50, speed, r, g, b);
}

void transitionToRandomColor()
{
	byte r, g, b;

	pickRandomColour(&r, &g, &b);

	for (byte i = 0; i < NO_OF_LIGHTS; i++)
		startLightTransition(i, 50, 20, r, g, b);
}

void flickeringColouredLights(byte r, byte g, byte b, byte min, byte max)
{
	if (r == oldr & g == oldg & b == oldb)
	{
		return;
	}

	for (byte i = 0; i < NO_OF_LIGHTS; i++) {
		colouredFlickeringLight(
			r, g, b,     // colour
			random(1, max - min),//60,            // flicker brightness
			random(1, (int)(max - min) / flickerUpdateSpeed),            // flicker update step
			min,            // flicker minimum
			max,           // flicker maximum
			1,             // number of ticks per flicker update - flicker speed
			NO_OF_GAPS*i,  // position on the ring
			&lights[i]);   // ligit to make flicker
	}
	oldr = r;
	oldb = b;
	oldg = g;
	forceLightUpdate = true;
}



void flickeringColouredLights(lightColor color, byte min, byte max)
{
	byte r, g, b;
	selectColour(color, &r, &g, &b);
	flickeringColouredLights(r, g, b, min, max);
}

void do_set_fade_colour(byte * buffer)
{
#ifdef SERIAL_VERBOSE
	Serial.print("Got Fade Colour r:");
	Serial.print(buffer[0]);
	Serial.print(" g:");
	Serial.print(buffer[1]);
	Serial.print(" b:");
	Serial.print(buffer[2]);
	Serial.print(" ticks per update:");
	Serial.print(buffer[3]);
	Serial.print(" no of steps to colour:");
	Serial.print(buffer[4]);
#endif 
	for (byte i = 0; i < NO_OF_LIGHTS; i++)
		startLightTransition(i, buffer[3], buffer[4], buffer[0], buffer[1], buffer[2]);
}

void do_set_brightness(byte * buffer)
{
#ifdef SERIAL_VERBOSE
	Serial.print("Got brightness:");
	Serial.println(buffer[0]);
#endif

	lightBrightness = buffer[0];
}

void do_set_flickering_colour(byte * buffer)
{
#ifdef SERIAL_VERBOSE
	Serial.print("Send Flickering Colour r:");
	Serial.print(buffer[0]);
	Serial.print(" g:");
	Serial.print(buffer[1]);
	Serial.print(" b:");
	Serial.println(buffer[2]);
#endif 
	flickeringColouredLights(buffer[0], buffer[1], buffer[2], 0, 255);
}

void do_start_flickering()
{
	for (byte i = 0; i < NO_OF_LIGHTS; i++) {
		flickeringLight(
			60,            // flicker brightness
			20,            // flicker update step
			0,            // flicker minimum
			255,           // flicker maximum
			1,             // number of ticks per flicker update
			lights[i].pos,  // position on the ring
			&lights[i]);   // ligit to make flicker
	}
}

void do_stop_flickering()
{
	for (byte i = 0; i < NO_OF_LIGHTS; i++) {
		steadyLight(
			lights[i].pos,  // position on the ring
			&lights[i]);   // ligit to make steady
	}
}

void do_start_sparkle()
{
	randomiseLights();
}

void updateLights()
{
	for (byte i = 0; i < NO_OF_LIGHTS; i++)
	{
		updateLightColours(i);
		updateLightPosition(i);
		updateLightFlicker(i);
	}

	renderLights();
}

void updateLightsAndDelay(bool wantDelay)
{
	tickEnd = millis() + TICK_INTERVAL;

	tickCount++;

	updateLights();

	if (transitionComplete())
	{
		if (randomColourTransitions)
			transitionToRandomColor();
	}

	if (wantDelay)
	{
		while (millis() < tickEnd) {
			delay(1);
		}
	}
}

// Pixel position for busy display
byte pixelPos = 0;
byte busyRed, busyGreen, busyBlue;

void updateBusyPixel()
{

	// turn off the current pixel dot
	setLightColor(0, 0, 0, pixelPos);

	pixelPos++;

	if (pixelPos == PIXELS)
		pixelPos = 0;

	setLightColor(busyRed, busyGreen, busyBlue, pixelPos);

	updateLights();
}

void startBusyPixel(byte red, byte green, byte blue)
{
	busyRed = red;
	busyBlue = blue;
	busyGreen = green;
	setAllLightsOff();
	pixelPos = 0;
	updateBusyPixel();
}

void stopBusyPixel()
{
	setAllLightsOff();
	oldr = 0;
	oldg = 0;
	oldb = 0;
}

void displayBusyPixelWait(int ticks, int onTime, byte red, byte green, byte blue)
{
	startBusyPixel(red, green, blue);
	for (int i = 0; i < ticks; i++)
	{
		updateBusyPixel();
		delay(onTime);
	}
	stopBusyPixel();
}
