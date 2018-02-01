
//#define DIAGNOSTICS_ACTIVE 

// Stored program management

#define STATEMENT_CONFIRMATION 1
#define LINE_NUMBERS 2
#define ECHO_DOWNLOADS 4
#define DUMP_DOWNLOADS 8

enum ProgramState
{
	PROGRAM_STOPPED,
	PROGRAM_PAUSED,
	PROGRAM_ACTIVE,
	PROGRAM_AWAITING_MOVE_COMPLETION,
	PROGRAM_AWAITING_DELAY_COMPLETION,
	SYSTEM_CONFIGURATION_CONNECTION // will never enter this state
};

enum DeviceState
{
	EXECUTE_IMMEDIATELY,
	STORE_PROGRAM
};

ProgramState programState = PROGRAM_STOPPED;
DeviceState deviceState = EXECUTE_IMMEDIATELY;

byte diagnosticsOutputLevel = 0;

long delayEndTime;

#define COMMAND_BUFFER_SIZE 60

// Set command terminator to CR

#define STATEMENT_TERMINATOR 0x0D

// Set program terminator to string end
// This is the EOT character
#define PROGRAM_TERMINATOR 0x00

char programCommand[COMMAND_BUFFER_SIZE];
char * commandPos;
char * commandLimit;
char * bufferLimit;
char * decodePos;
char * decodeLimit;

char remoteCommand[COMMAND_BUFFER_SIZE];
char * remotePos;
char * remoteLimit;

#ifdef COMMAND_DEBUG
#define READ_INTEGER_DEBUG
#endif

#include "Variables.h"

///////////////////////////////////////////////////////////
/// Serial comms 
///////////////////////////////////////////////////////////

int CharsAvailable()
{
	return Serial.available();
}

byte GetRawCh()
{
	int ch;
	do
	{
		ch = Serial.read();
	} while (ch < 0);

	return (byte)ch;
}

// Current position in the EEPROM of the execution
int programCounter;

// Start position of the code as stored in the EEPROM
int programBase;

// Write position when downloading and storing program code
int programWriteBase;

// Write position for any incoming program code
int bufferWritePosition;

// Checksum for the download
byte downloadChecksum;

// Dumps the program as stored in the EEPROM

void dumpProgramFromEEPROM(int EEPromStart)
{
	int EEPromPos = EEPromStart;

	Serial.println(F("Program: "));

	char byte;
	while (true)
	{
		byte = EEPROM.read(EEPromPos++);

		if (byte == STATEMENT_TERMINATOR)
			Serial.println();
		else
			Serial.print(byte);

		if (byte == PROGRAM_TERMINATOR)
		{
			Serial.print(F("Program size: "));
			Serial.println(EEPromPos - EEPromStart);
			break;
		}

		if (EEPromPos >= EEPROM_SIZE)
		{
			Serial.println(F("Eeprom end"));
			break;
		}
	}
}

// Starts a program running at the given position

void startProgramExecution(int programPosition)
{
	if (isProgramStored())
	{

#ifdef PROGRAM_DEBUG
		Serial.print(F(".Starting program execution at: "));
		Serial.println(programPosition);
#endif
		clearVariables();
		setAllLightsOff();
		programCounter = programPosition;
		programBase = programPosition;
		programState = PROGRAM_ACTIVE;
	}
}

// RH - remote halt

void haltProgramExecution()
{
#ifdef PROGRAM_DEBUG
	Serial.print(F(".Ending program execution at: "));
	Serial.println(programCounter);
#endif

	motorStop();

	programState = PROGRAM_STOPPED;
}

// RP - pause program

void pauseProgramExecution()
{
#ifdef PROGRAM_DEBUG
	Serial.print(".Pausing program execution at: ");
	Serial.println(programCounter);
#endif

	programState = PROGRAM_PAUSED;

#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("RPOK"));
	}

#endif

}

// RR - resume running program

void resumeProgramExecution()
{
#ifdef PROGRAM_DEBUG
	Serial.print(".Resuming program execution at: ");
	Serial.println(programCounter);
#endif

	if (programState == PROGRAM_PAUSED)
	{
		// Can resume the program
		programState = PROGRAM_ACTIVE;

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("RROK"));
		}
#endif


	}
	else
	{
#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("RRFail:"));
			Serial.println(programState);
		}
#endif
	}
}

enum lineStorageState
{
	LINE_START,
	GOT_R,
	STORING,
	SKIPPING
};

lineStorageState lineStoreState;

void resetLineStorageState()
{
	lineStoreState = LINE_START;
}

void storeProgramByte(byte b)
{
	storeByteIntoEEPROM(b, programWriteBase++);
}

void clearStoredProgram()
{
	clearProgramStoredFlag();
	storeByteIntoEEPROM(PROGRAM_TERMINATOR, STORED_PROGRAM_OFFSET);
}

// Called to start the download of program code
// each byte that arrives down the serial port is now stored in program memory
//
void startDownloadingCode(int downloadPosition)
{
#ifdef PROGRAM_DEBUG
	Serial.println(".Starting code download");
#endif

	// Stop the current program
	haltProgramExecution();

	// clear the existing program so that
	// partially stored programs never get executed on power up

	clearStoredProgram();

	deviceState = STORE_PROGRAM;

	programWriteBase = downloadPosition;

	resetLineStorageState();

	startBusyPixel(128, 128, 128);

#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("RMOK"));
	}

#endif

}

int decodeScriptChar(char b, void(*output) (byte));

// Called when a byte is received from the host when in program storage mode
// Adds it to the stored program, updates the stored position and the counter
// If the byte is the terminator byte (zero) it changes to the "wait for checksum" state
// for the program checksum

//#define STORE_RECEIVED_BYTE_DEBUG

void endProgramReceive()
{
	stopBusyPixel();

	// enable immediate command receipt

	deviceState = EXECUTE_IMMEDIATELY;
}

void storeReceivedByte(byte b)
{
	// ignore odd characters - except for CR

	if (b < 32 | b>128)
	{
		if (b != STATEMENT_TERMINATOR)
			return;
	}

	switch (lineStoreState)
	{

	case LINE_START:
		// at the start of a line - look for an R command

		if (b == 'r' | b == 'R')
		{
			lineStoreState = GOT_R;
		}
		else
		{
			lineStoreState = STORING;
		}
		break;

	case GOT_R:
		// Last character was an R - is this an X or an A?

		switch (b)
		{
		case 'x':
		case 'X':
			endProgramReceive();

			// put the terminator on the end

			storeProgramByte(PROGRAM_TERMINATOR);

			setProgramStored();

#ifdef DIAGNOSTICS_ACTIVE

			if (diagnosticsOutputLevel & DUMP_DOWNLOADS)
			{
				dumpProgramFromEEPROM(STORED_PROGRAM_OFFSET);
			}

#endif

			startProgramExecution(STORED_PROGRAM_OFFSET);

			break;

		case 'A':
		case 'a':
			Serial.println("RA");
			endProgramReceive();

			clearStoredProgram();

			break;

		default:

			// Not an X jor A - but we never store R commands
			// skip to the next line
			lineStoreState = SKIPPING;
		}

		break;

	case SKIPPING:
		// we are skipping an R command - look for a statement terminator
		if (b == STATEMENT_TERMINATOR)
		{
			// Got a terminator, look for the command character
			lineStoreState = LINE_START;
		}
		break;

	case STORING:
		// break out- storing takes place next
		break;
	}

	if (lineStoreState == STORING)
	{
		// get here if we are storing or just got a line start

		// if we get here we store the byte
		storeProgramByte(b);

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & ECHO_DOWNLOADS)
		{
			Serial.print((char)b);
		}

#endif

		if (b == STATEMENT_TERMINATOR)
		{
			// Got a terminator, look for the command character

#ifdef DIAGNOSTICS_ACTIVE

			if (diagnosticsOutputLevel & ECHO_DOWNLOADS)
			{
				Serial.println();
			}

#endif
			lineStoreState = LINE_START;
			// look busy
			updateBusyPixel();
		}
	}
}

void resetCommand()
{
#ifdef COMMAND_DEBUG
	Serial.println(".**resetCommand");
#endif
	commandPos = programCommand;
	bufferLimit = commandPos + COMMAND_BUFFER_SIZE;
}

#ifdef COMMAND_DEBUG
#define MOVE_FORWARDS_DEBUG
#endif

// Command MFddd,ttt - move distance ddd over time ttt (ttt expressed in "ticks" - tenths of a second)
// Return OK

void remoteMoveForwards()
{
	int forwardMoveDistance;

#ifdef MOVE_FORWARDS_DEBUG
	Serial.println(".**moveForwards");
#endif

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no dist"));
		}

#endif

		return;
	}

	if (!getValue(&forwardMoveDistance))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MFOK"));
		}

#endif

		fastMoveDistanceInMM(forwardMoveDistance, forwardMoveDistance);
		return;
	}

	decodePos++; // move past the separator

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no time"));
		}

#endif
		return;
	}

	int forwardMoveTime;

	if (!getValue(&forwardMoveTime))
	{
		return;
	}

	int moveResult = timedMoveDistanceInMM(forwardMoveDistance, forwardMoveDistance, (float)forwardMoveTime / 10.0);

#ifdef DIAGNOSTICS_ACTIVE

	if (moveResult == 0)
	{


		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MFOK"));
		}

	}
	else
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MFFail"));
		}
	}

#endif

}

// Command MAradius,angle,time - move arc. 
// radius - radius of the arc to move
// angle of the arc to move
// time - time for the move
//
// Return OK

//#define MOVE_ANGLE_DEBUG

void remoteMoveAngle()
{
#ifdef MOVE_ANGLE_DEBUG
	Serial.println(".**moveAngle");
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MAFail: no radius"));
		}

#endif

		return;
	}

	int radius;

	if (!getValue(&radius))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MAFail: no angle"));
		}

#endif

		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MAFail: no angle"));
		}

#endif
		return;
	}

	int angle;

	if (!getValue(&angle))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MAOK"));
		}

#endif

		fastMoveArcRobot(radius, angle);
		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MAFail: no time"));
		}

#endif

		return;
	}

	int time;

	if (!getValue(&time))
	{
		return;
	}

#ifdef MOVE_ANGLE_DEBUG
	Serial.print("    radius: ");
	Serial.print(radius);
	Serial.print(" angle: ");
	Serial.print(angle);
	Serial.print(" time: ");
	Serial.println(time);
#endif

	int reply = timedMoveArcRobot(radius, angle, time / 10.0);

#ifdef DIAGNOSTICS_ACTIVE

	if (reply == 0)
	{

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MAOK"));
		}
	}
	else
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MAFail"));
		}
	}

#endif
}

// Command MMld,rd,time - move motors. 
// ld - left distance
// rd - right distance
// time - time for the move
//
// Return OK

//#define MOVE_MOTORS_DEBUG

void remoteMoveMotors()
{
#ifdef MOVE_MOTORS_DEBUG
	Serial.println(".**movemMotors");
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MMFail: no left distance"));
		}

#endif

		return;
	}

	int leftDistance;

	if (!getValue(&leftDistance))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MMFail: no right distance"));
		}

#endif

		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MMFail: no right distance"));
		}

#endif

		return;
	}

	int rightDistance;

	if (!getValue(&rightDistance))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MMOK"));
		}

#endif

		fastMoveDistanceInMM(leftDistance, rightDistance);
		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MMFail: no time"));
		}

#endif

		return;
	}

	int time;

	if (!getValue(&time))
	{
		return;
	}

#ifdef MOVE_MOTORS_DEBUG
	Serial.print("    ld: ");
	Serial.print(leftDistance);
	Serial.print(" rd: ");
	Serial.print(rightDistance);
	Serial.print(" time: ");
	Serial.println(time);
#endif

	int reply = timedMoveDistanceInMM(leftDistance, rightDistance, time / 10.0);

#ifdef DIAGNOSTICS_ACTIVE

	if (reply == 0)
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MMOK"));
		}
	}
	else
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MMFail: "));
			Serial.println(reply);
		}
	}

#endif

}

// Command MWll,rr,ss - wheel configuration
// ll - diameter of left wheel
// rr - diameter of right wheel
// ss - spacing of wheels
//
// all dimensions in mm
#// Return OK

//#define CONFIG_WHEELS_DEBUG

void remoteConfigWheels()
{
#ifdef CONFIG_WHEELS_DEBUG
	Serial.println(F(".**remoteConfigWheels"));
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MWFail: no left diameter"));
		}

#endif

		return;
	}

	int leftDiameter;

	if (!getValue(&leftDiameter))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MWFail: no right diameter"));
		}

#endif

		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MWFail: no right diameter"));
		}

#endif

			return;
	}

	int rightDiameter;

	if (!getValue(&rightDiameter))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MWFail: no wheel spacing"));
		}
#endif

		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MWFail: no wheel spacing"));
		}

#endif

		return;
	}

	int spacing;

	if (!getValue(&spacing))
	{
		return;
	}

#ifdef CONFIG_WHEELS_DEBUG
	Serial.print("    ld: ");
	Serial.print(leftDiameter);
	Serial.print(" rd: ");
	Serial.print(rightDiameter);
	Serial.print(" separation: ");
	Serial.println(spacing);
#endif

	setActiveWheelSettings(leftDiameter, rightDiameter, spacing);


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("MWOK"));
	}
#endif

}

void remoteViewWheelConfig()
{
	dumpActiveWheelSettings();
}

#ifdef COMMAND_DEBUG
#define ROTATE_DEBUG
#endif

// Command MRddd,ttt - rotate distance in time ttt (ttt is given in "ticks", where a tick is a tenth of a second
// Command MR    - rotate previous distance, or 0 if no previous rotate
// Return OK

void remoteRotateRobot()
{
	int rotateAngle;

#ifdef ROTATE_DEBUG
	Serial.println(F(".**rotateRobot"));
#endif

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MRFail: no angle"));
		}

#endif

		return;
	}

	rotateAngle;

	if (!getValue(&rotateAngle))
	{
		return;
	}

#ifdef ROTATE_DEBUG
	Serial.print(".  Rotating: ");
	Serial.println(rotateAngle);
#endif

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MROK"));
		}

#endif

		fastRotateRobot(rotateAngle);
		return;
	}

	decodePos++; // move past the separator

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MRFail: no time"));
		}
#endif
		return;
	}

	int rotateTimeInTicks;

	if (!getValue(&rotateTimeInTicks))
	{
		return;
	}

	int moveResult = timedRotateRobot(rotateAngle, rotateTimeInTicks / 10.0);

	if (moveResult == 0)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("MROK"));
		}
#endif
	}
	else
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("MRFail"));
		}

#endif

	}
}

#ifdef COMMAND_DEBUG
#define CHECK_MOVING_DEBUG
#endif

// Command MC - check if the robot is still moving
// Return "moving" or "stopped"

void checkMoving()
{
#ifdef CHECK_MOVING_DEBUG
	Serial.println(".**CheckMoving: ");
#endif

	if (motorsMoving())
	{
#ifdef CHECK_MOVING_DEBUG
		Serial.println(".  moving");
#endif
		Serial.println("MCMove");
	}
	else
	{
#ifdef CHECK_MOVING_DEBUG
		Serial.println(".  stopped");
#endif
		Serial.println("MCstopped");
	}
}

#ifdef COMMAND_DEBUG
#define REMOTE_STOP_DEBUG
#endif

// Command MS - stops the robot
// Return OK
void remoteStopRobot()
{
#ifdef REMOTE_STOP_DEBUG
	Serial.println(".**remoteStopRobot: ");
#endif

	motorStop();


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println("MSOK");
	}

#endif

}


void remoteMoveControl()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		Serial.println(F("FAIL: mising move control command character"));

#endif

		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(".**remoteMoveControl: ");
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(".  Move Command code : ");
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'A':
	case 'a':
		remoteMoveAngle();
		break;
	case 'F':
	case 'f':
		remoteMoveForwards();
		break;
	case 'R':
	case 'r':
		remoteRotateRobot();
		break;
	case 'M':
	case 'm':
		remoteMoveMotors();
		break;
	case 'C':
	case 'c':
		checkMoving();
		break;
	case 'S':
	case 's':
		remoteStopRobot();
		break;
	case 'V':
	case 'v':
		remoteViewWheelConfig();
		break;
	case 'W':
	case 'w':
		remoteConfigWheels();
		break;
	}
}

#ifdef COMMAND_DEBUG
#define PIXEL_COLOUR_DEBUG
#endif

bool readColour(byte *r, byte *g, byte*b)
{
	int result;

	if (!getValue(&result))
	{
		return;
	}

	*r = (byte)result;

#ifdef PIXEL_COLOUR_DEBUG
	Serial.print(".  Red: ");
	Serial.println(*r);
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising colour values in readColor"));
		}

#endif
		return false;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising colours after red in readColor"));
		}

#endif

		return false;
	}

	if (!getValue(&result))
	{
		return;
	}

	*g = (byte)result;

#ifdef PIXEL_COLOUR_DEBUG
	Serial.print(".  Green: ");
	Serial.println(*g);
#endif

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising colours after green in readColor"));
		}

#endif

		return false;
	}

	if (!getValue(&result))
	{
		return;
	}

	*b = (byte)result;

#ifdef PIXEL_COLOUR_DEBUG
	Serial.print(".  Blue: ");
	Serial.println(*b);
#endif

	return true;
}

// Command PCrrr,ggg,bbb - set a coloured candle with the red, green 
// and blue components as given
// Return OK

void remoteColouredCandle()
{
#ifdef PIXEL_COLOUR_DEBUG
	Serial.println(".**remoteColouredCandle: ");
#endif

	byte r, g, b;


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print("PC");
	}

#endif

	if (readColour(&r, &g, &b))
	{
		flickeringColouredLights(r, g, b, 0, 200);

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("OK"));
		}
#endif
	}
}

// Command PNname - set a coloured candle with the name as given
// Return OK

void remoteSetColorByName()
{
#ifdef PIXEL_COLOUR_DEBUG
	Serial.println(".**remoteColouredName: ");
#endif

	byte r=0, g=0, b=0;


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print("PN");
	}

#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising colour in set colour by name"));
		}

#endif

		return;
	}

	char inputCh = toLowerCase(*decodePos);

	switch (inputCh) 
	{
	case'r':
		r = 255;
		break;
	case'g':
		g = 255;
		break;
	case'b':
		b = 255;
		break;
	case'c':
		g = 255;
		b = 255;
		break;
	case'm':
		r = 255;
		b = 255;
		break;
	case'y':
		g = 255;
		r = 255;
		break;
	case'w':
		r = 255;
		g = 255;
		b = 255;
		break;
	case'k':
		break;
	default:

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: invalid colour in set colour by name"));
		}
#endif
		return;
	}

	flickeringColouredLights(r, g, b, 0, 200);


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("OK"));
	}

#endif

}

//#define REMOTE_PIXEL_COLOR_FADE_DEBUG

void remoteFadeToColor()
{
#ifdef PIXEL_COLOUR_DEBUG
	Serial.println(".**remoteFadeToColour: ");
#endif

	int result;

	if (!getValue(&result))
	{
		return;
	}

	byte no = (byte)result;

	if (no < 1)no = 1;
	if (no > 20)no = 20;

	no = 21 - no;

#ifdef REMOTE_PIXEL_COLOR_FADE_DEBUG
	Serial.print(".  Setting: ");
	Serial.println(no);
#endif

	decodePos++;


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print("PX");
	}

#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("Fail: mising colours after speed"));
		}

#endif

		return;
	}

	byte r, g, b;


#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("PX"));
	}

#endif

	if (readColour(&r, &g, &b))
	{
		transitionToColor(no, r, g, b);

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("OK"));
		}

#endif

	}
}

// PFddd - set flicker speed to value given

void remoteSetFlickerSpeed()
{
#ifdef PIXEL_COLOUR_DEBUG
	Serial.println(".**remoteSetFlickerSpeed: ");
#endif

	int result;

	if (!getValue(&result))
	{
		return;
	}

	byte no = (byte)result;


#ifdef PIXEL_COLOUR_DEBUG
	Serial.print(".  Setting: ");
	Serial.println(no);
#endif

	setFlickerUpdateSpeed(no);

#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("PFOK"));
	}

#endif

}

// PIppp,rrr,ggg,bbb
// Set individual pixel colour

void remoteSetIndividualPixel()
{

#ifdef PIXEL_COLOUR_DEBUG
	Serial.println(".**remoteSetIndividualPixel: ");
#endif

	int result;

	if (!getValue(&result))
	{
		return;
	}

	byte no = (byte)result;

#ifdef PIXEL_COLOUR_DEBUG
	Serial.print(".  Setting: ");
	Serial.println(no);
#endif

	decodePos++;

#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print("PI");
	}

#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("Fail: mising colours after pixel"));
		}

#endif

		return;
	}

	byte r, g, b;

	if (readColour(&r, &g, &b))
	{
		setLightColor(r, g, b, no);

#ifdef DIAGNOSTICS_ACTIVE

		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println("OK");
		}

#endif

	}
}

void remoteSetPixelsOff()
{

#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println("POOK");
	}

#endif

	setAllLightsOff();
}

void remoteSetRandomColors()
{

#ifdef DIAGNOSTICS_ACTIVE

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println("PROK");
	}

#endif

	randomiseLights();
}

void remotePixelControl()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{

#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: mising pixel control command character"));
#endif
		return;
	}

#ifdef PIXEL_COLOUR_DEBUG
	Serial.println(".**remotePixelControl: ");
#endif

	char commandCh = *decodePos;

#ifdef PIXEL_COLOUR_DEBUG
	Serial.print(".  Pixel Command code : ");
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'a':
	case 'A':
		flickerOn();
		break;
	case 's':
	case 'S':
		flickerOff();
		break;
	case 'i':
	case 'I':
		remoteSetIndividualPixel();
		break;
	case 'o':
	case 'O':
		remoteSetPixelsOff();
		break;
	case 'c':
	case 'C':
		remoteColouredCandle();
		break;
	case 'f':
	case 'F':
		remoteSetFlickerSpeed();
		break;
	case 'x':
	case 'X':
		remoteFadeToColor();
		break;
	case 'r':
	case 'R':
		remoteSetRandomColors();
		break;
	case 'n':
	case 'N':
		remoteSetColorByName();
		break;
	}
}

// Command CDddd - delay time
// Command CD    - previous delay
// Return OK

#ifdef COMMAND_DEBUG
#define COMMAND_DELAY_DEBUG
#endif

void remoteDelay()
{
	int delayValueInTenthsIOfASecond;

#ifdef COMMAND_DELAY_DEBUG
	Serial.println(".**remoteDelay");
#endif

	if (*decodePos == STATEMENT_TERMINATOR)
	{

#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("CDFail: no delay"));
		}
#endif

		return;
	}

	if (!getValue(&delayValueInTenthsIOfASecond))
	{
		return;
	}

#ifdef COMMAND_DELAY_DEBUG
	Serial.print(".  Delaying: ");
	Serial.println(delayValueInTenthsIOfASecond);
#endif


#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("CDOK"));
	}
#endif

	delayEndTime = millis() + delayValueInTenthsIOfASecond * 100;

	programState = PROGRAM_AWAITING_DELAY_COMPLETION;
}

// Command CLxxxx - program label
// Ignored at execution, specifies the destination of a branch
// Return OK

void declareLabel()
{
#ifdef COMMAND_DELAY_DEBUG
	Serial.println(".**declareLabel");
#endif

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println("CLOK");
	}
#endif
}

int findNextStatement(int programPosition)
{

	while (true)
	{
		char ch = EEPROM.read(programPosition);

		if (ch == PROGRAM_TERMINATOR | programPosition == EEPROM_SIZE)
			return -1;

		if (ch == STATEMENT_TERMINATOR)
		{
			programPosition++;
			if (programPosition == EEPROM_SIZE)
				return -1;
			else
				return programPosition;
		}
		programPosition++;
	}
}

// Find a label in the program
// Returns the offset into the program where the label is declared
// The first parameter is the first character of the label 
// (i.e. the character after the instruction code that specifies the destination)
// This might not always be the same command (it might be a branch or a subroutine call)
// The second parameter is the start position of the search in the program. 
// This is always the start of a statement, and usually the start of the program, to allow
// branches up the code. 

//#define FIND_LABEL_IN_PROGRAM_DEBUG

int findLabelInProgram(char * label, int programPosition)
{
	// Assume we are starting at the beginning of the program

	while (true)
	{
		// Spin down the statements

		int statementStart = programPosition;

		char programByte = EEPROM.read(programPosition++);

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
		Serial.print("Statement at: ");
		Serial.print(statementStart);
		Serial.print(" starting: ");
		Serial.println(programByte);
#endif
		if (programByte != 'C' & programByte != 'c')
		{

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
			Serial.println("Not a statement that starts with C");
#endif

			programPosition = findNextStatement(programPosition);

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
			Serial.print("Spin to statement at: ");
			Serial.println(programPosition);
#endif

			// Check to see if we have reached the end of the program in EEPROM
			if (programPosition == -1)
			{
				// Give up if the end of the code has been reached
				return -1;
			}
			else
			{
				// Check this statement
				continue;
			}
		}

		// If we get here we have found a C

		programByte = EEPROM.read(programPosition++);

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG

		Serial.print("Second statement character: ");
		Serial.println(programByte);

#endif

		// if we get here we have a control command - see if the command is a label
		if (programByte != 'L' & programByte != 'l')
		{

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
			Serial.println("Not a loop command");
#endif

			programPosition = findNextStatement(programPosition);
			if (programPosition == -1)
			{
				return -1;
			}
			else
			{
				continue;
			}
		}

		//if we get here we have a CL command

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
		Serial.println("Got a CL command");
#endif

		// Set start position for label comparison
		char * labelTest = label;

		// Now spin down the label looking for a match

		while (*labelTest != STATEMENT_TERMINATOR & programPosition < EEPROM_SIZE)
		{
			programByte = EEPROM.read(programPosition);

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
			Serial.print("Destination byte: ");
			Serial.print(*labelTest);
			Serial.print(" Program byte: ");
			Serial.println(programByte);
#endif

			if (*labelTest == programByte)
			{

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
				Serial.println("Got a match");
#endif
				// Move on to the next byte
				labelTest++;
				programPosition++;
			}
			else
			{
#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
				Serial.println("Fail");
#endif
				break;
			}
		}

		// get here when we reach the end of the statement or we have a mismatch

		// Get the byte at the end of the destination statement

		programByte = EEPROM.read(programPosition);

		if (*labelTest == programByte)
		{
			// If the end of the label matches the end of the statement code we have a match
			// Note that this means that if the last line of the program is a label we can only 
			// find this line if it has a statement terminator on the end. 
			// Which is fine by me. 

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
			Serial.println("label match");
#endif
			return statementStart;
		}
		else
		{
			programPosition = findNextStatement(programPosition);

#ifdef FIND_LABEL_IN_PROGRAM_DEBUG
			Serial.print("Spin to statement at: ");
			Serial.println(programPosition);
#endif

			// Check to see if we have reached the end of the program in EEPROM
			if (programPosition == -1)
			{
				// Give up if the end of the code has been reached
				return -1;
			}
			else
			{
				// Check this statement
				continue;
			}
		}
	}
}

// Command CJxxxx - jump to label
// Jumps to the specified label 
// Return CJOK if the label is found, error if not. 

void jumpToLabel()
{
#ifdef JUMP_TO_LABEL_DEBUG
	Serial.println(".**jump to label");
#endif

	char * labelPos = decodePos;
	char * labelSearch = decodePos;

	int labelStatementPos = findLabelInProgram(decodePos, programBase);

#ifdef JUMP_TO_LABEL_DEBUG
	Serial.print("Label statement pos: ");
	Serial.println(labelStatementPos);
#endif

	if (labelStatementPos >= 0)
	{
		// the label has been found - jump to it
		programCounter = labelStatementPos;

#ifdef JUMP_TO_LABEL_DEBUG
		Serial.print("New Program Counter: ");
		Serial.println(programCounter);
#endif

#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println("CJOK");
		}
#endif
	}
	else
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println("CJFAIL: no dest");
		}
#endif
	}
}

//#define JUMP_TO_LABEL_COIN_DEBUG

// Command CCxxxx - jump to label on a coin toss
// Jumps to the specified label 
// Return CCOK if the label is found, error if not. 

void jumpToLabelCoinToss()
{
#ifdef JUMP_TO_LABEL_COIN_DEBUG
	Serial.println(F(".**jump to label coin toss")); send

#endif

	char * labelPos = decodePos;
	char * labelSearch = decodePos;

	int labelStatementPos = findLabelInProgram(decodePos, programBase);

#ifdef JUMP_TO_LABEL_COIN_DEBUG
	Serial.print("  Label statement pos: ");
	Serial.println(labelStatementPos);
#endif

	if (labelStatementPos >= 0)
	{
		// the label has been found - jump to it

		if (random(0, 2) == 0)
		{
			programCounter = labelStatementPos;
#ifdef DIAGNOSTICS_ACTIVE
			if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
			{
				Serial.print(F("CCjump"));
			}
#endif
		}
		else
		{
#ifdef DIAGNOSTICS_ACTIVE
			if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
			{
				Serial.print(F("CCcontinue"));
			}
#endif
		}

#ifdef JUMP_TO_LABEL_COIN_DEBUG
		Serial.print(F("New Program Counter: "));
		Serial.println(programCounter);
#endif
	}
	else
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("CCFail: no dest"));
		}
#endif
	}
}



// Command CA - pause when motors active
// Return CAOK when the pause is started

void pauseWhenMotorsActive()
{
#ifdef PAUSE_MOTORS_ACTIVE_DEBUG
	Serial.println(".**pause while the motors are active");
#endif

	// Only wait for completion if the program is actually running

	if(programState == PROGRAM_ACTIVE)
		programState = PROGRAM_AWAITING_MOVE_COMPLETION;

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("CAOK"));
	}
#endif
}

// Command Ccddd,ccc
// Jump to label if condition true

//#define COMMAND_MEASURE_DEBUG

void measureDistanceAndJump()
{

#ifdef COMMAND_MEASURE_DEBUG
	Serial.println(F(".**measure disance and jump to label"));
#endif

	int distance;

	if (!getValue(&distance))
	{
		return;
	}

#ifdef COMMAND_MEASURE_DEBUG
	Serial.print(F(".  Distance: "));
	Serial.println(distance);
#endif

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("CM"));
	}
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising dest"));
		}
#endif
		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising dest"));
		}
#endif
		return;
	}

	int labelStatementPos = findLabelInProgram(decodePos, programBase);

#ifdef COMMAND_MEASURE_DEBUG
	Serial.print("Label statement pos: ");
	Serial.println(labelStatementPos);
#endif

	if (labelStatementPos < 0)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: label not found"));
#endif
		return;
	}

	int measuredDistance = getDistanceValueInt();

#ifdef COMMAND_MEASURE_DEBUG
	Serial.print(F("Measured Distance: "));
	Serial.println(measuredDistance);
#endif

	if (measuredDistance < distance)
	{
#ifdef COMMAND_MEASURE_DEBUG
		Serial.println(F("Distance smaller - taking jump"));
#endif
		// the label has been found - jump to it
		programCounter = labelStatementPos;

#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("jump"));
		}
#endif
	}
	else
	{
#ifdef COMMAND_MEASURE_DEBUG
		Serial.println("Distance larger - continuing");
#endif
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("continue"));
		}
#endif
	}

	// otherwise do nothing
}

//#define COMPARE_CONDITION_DEBUG

void compareAndJump(bool jumpIfTrue)
{

#ifdef COMPARE_CONDITION_DEBUG
	Serial.println(F(".**test condition and jump to label"));
#endif

	bool result;

	if (!testCondition(&result))
	{
		return;
	}

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		if (jumpIfTrue)
			Serial.print(F("CC"));
		else
			Serial.print(F("CN"));
	}
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising dest"));
		}
#endif
		return;
	}

	decodePos++;

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising dest"));
		}
#endif
		return;
	}

	int labelStatementPos = findLabelInProgram(decodePos, programBase);

#ifdef COMPARE_CONDITION_DEBUG
	Serial.print("Label statement pos: ");
	Serial.println(labelStatementPos);
#endif

	if (labelStatementPos < 0)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: label not found"));
#endif
		return;
	}

	if (result == jumpIfTrue)
	{
#ifdef COMPARE_CONDITION_DEBUG
		Serial.println(F("Condition true - taking jump"));
#endif
		// the label has been found - jump to it
		programCounter = labelStatementPos;

#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("jump"));
		}
#endif
	}
	else
	{
#ifdef COMPARE_CONDITION_DEBUG
		Serial.println("condition failed - continuing");
#endif
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("continue"));
		}
#endif
	}

	// otherwise do nothing
}


// Command CIccc
// Jump to label if the motors are not running

//#define JUMP_MOTORS_INACTIVE_DEBUG

void jumpWhenMotorsInactive()
{

#ifdef JUMP_MOTORS_INACTIVE_DEBUG
	Serial.println(F(".**jump to label if motors inactive"));
#endif

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("CI"));
	}
#endif

	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: mising dest"));
		}
#endif
		return;
	}

	int labelStatementPos = findLabelInProgram(decodePos, programBase);

#ifdef JUMP_MOTORS_INACTIVE_DEBUG
	Serial.print("Label statement pos: ");
	Serial.println(labelStatementPos);
#endif

	if (labelStatementPos < 0)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: label not found"));
		}
#endif
		return;
	}

	if (!motorsMoving())
	{
#ifdef JUMP_MOTORS_INACTIVE_DEBUG
		Serial.println("Motors inactive - taking jump");
#endif
		// the label has been found - jump to it
		programCounter = labelStatementPos;
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("jump"));
		}
#endif
	}
	else
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("continue"));
		}
#endif
#ifdef JUMP_MOTORS_INACTIVE_DEBUG
		Serial.println(F("Motors running - continuing"));
#endif
	}

	// otherwise do nothing
}

void programControl()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: missing program control command character"));
#endif
		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(F(".**remoteProgramControl: "));
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".   Program command : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'I':
	case 'i':
		jumpWhenMotorsInactive();
		break;
	case 'A':
	case 'a':
		pauseWhenMotorsActive();
		break;
	case 'D':
	case 'd':
		remoteDelay();
		break;
	case 'L':
	case 'l':
		declareLabel();
		break;
	case 'J':
	case 'j':
		jumpToLabel();
		break;
	case 'M':
	case 'm':
		measureDistanceAndJump();
		break;
	case 'C':
	case 'c':
		jumpToLabelCoinToss();
		break;
	case 'T':
	case 't':
		compareAndJump(true);
		break;
	case 'F':
	case 'f':
		compareAndJump(false);
		break;
	}
}

//#define REMOTE_DOWNLOAD_DEBUG

// RM - start remote download

void remoteDownload()
{

#ifdef REMOTE_DOWNLOAD_DEBUG
	Serial.println(F(".**remote download"));
#endif

	if (deviceState != EXECUTE_IMMEDIATELY)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("RMFAIL: not accepting commands"));
#endif
		return;
	}

	startDownloadingCode(STORED_PROGRAM_OFFSET);
}

void startProgramCommand()
{
	startProgramExecution(STORED_PROGRAM_OFFSET);

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("RSOK"));
	}
#endif
}


void haltProgramExecutionCommand()
{
	haltProgramExecution();

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("RHOK"));
	}
#endif
}

void clearProgramStoreCommand()
{
	haltProgramExecution();

	clearStoredProgram();

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("RCOK"));
	}
#endif
}


void remoteManagement()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: missing remote control command character"));
#endif
		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(F(".**remoteProgramDownload: "));
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".   Download command : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'M':
	case 'm':
		remoteDownload();
		break;
	case 'S':
	case 's':
		startProgramCommand();
		break;
	case 'H':
	case 'h':
		haltProgramExecutionCommand();
		break;
	case 'P':
	case 'p':
		pauseProgramExecution();
		break;
	case 'R':
	case 'r':
		resumeProgramExecution();
		break;
	case 'C':
	case 'c':
		clearProgramStoreCommand();
		break;
	}
}

const String version = "Version R1.0";

// IV - information display version

void displayVersion()
{
#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("IVOK"));
	}
#endif

	Serial.println(version);
}

void displayDistance()
{
#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("IDOK"));
	}
#endif
	Serial.println(getDistanceValueInt());
}

void printStatus()
{
#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("ISOK"));
	}
#endif
	Serial.print(programState);
	Serial.println(diagnosticsOutputLevel);

}

// IMddd - set the debugging diagnostics level

//#define SET_MESSAGING_DEBUG 

void setMessaging()
{

#ifdef SET_MESSAGING_DEBUG
	Serial.println(F(".**informationlevelset: "));
#endif
	int result;

	if (!getValue(&result))
	{
		return;
	}

	byte no = (byte)result;

#ifdef SET_MESSAGING_DEBUG
	Serial.print(F(".  Setting: "));
	Serial.println(no);
#endif

	diagnosticsOutputLevel = no;

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("IMOK"));
	}
#endif
}

void printProgram()
{
	dumpProgramFromEEPROM(STORED_PROGRAM_OFFSET);

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("IPOK"));
	}
#endif
}

void information()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: missing information command character"));
#endif
		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(F(".**remoteProgramDownload: "));
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".   Download command : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'V':
	case 'v':
		displayVersion();
		break;
	case 'D':
	case 'd':
		displayDistance();
		break;
	case 'S':
	case 's':
		printStatus();
		break;
	case 'M':
	case 'm':
		setMessaging();
		break;
	case 'P':
	case 'p':
		printProgram();
		break;
	}
}


void doClearVariables()
{
	clearVariables();

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.print(F("VCOK"));
	}
}

void variableManagement()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: missing variable command character"));
#endif
		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(F(".**variable management: "));
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".   Download command : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'C':
	case 'c':
		doClearVariables();
		break;

	case 'S':
	case 's':
		setVariable();
		break;

	case 'V':
	case 'v':
		viewVariable();
		break;
	}
}

// Command STfreq,time - play tone with freq for given time
// waits for tone to complete playing if wait is true
// Return OK

void doTone()
{
	int frequency;

#ifdef PLAY_TONE_DEBUG
	Serial.println(".**play tone");
#endif

	if (*decodePos == STATEMENT_TERMINATOR)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no tone frequency"));
		}
#endif
		return;
	}

	if (!getValue(&frequency))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no tone frequency"));
		}
#endif
		return;
	}

	decodePos++; // move past the separator

	if (*decodePos == STATEMENT_TERMINATOR)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no tone duration"));
		}
#endif
		return;
	}

	int duration;

	if (!getValue(&duration))
	{
		return;
	}

	if (*decodePos == STATEMENT_TERMINATOR)
	{
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no wait "));
		}
#endif
		return;
	}

	decodePos++;

	switch (*decodePos)
	{
	case 'W':
	case 'w':
		delayEndTime = millis() + duration;
		programState = PROGRAM_AWAITING_DELAY_COMPLETION;
		// Note that this code intentionally falls through the end of the case
	case 'n':
	case 'N':
		playTone(frequency, duration);
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("SOUNDOK"));
		}
#endif
		break;
	default:
		break;

#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("FAIL: no wait "));
		}
#endif
	}
}

void remoteSoundPlay()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: missing sound command character"));
#endif
		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(F(".**sound playback: "));
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".   Download command : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'T':
	case 't':
		doTone();
		break;
	}
}

void doRemoteWriteText()
{
	while (*decodePos != STATEMENT_TERMINATOR & decodePos != decodeLimit) {
		Serial.print(*decodePos);
		decodePos++;
	}
}

void doRemoteWriteLine()
{
	Serial.println();
}

void doRemotePrintValue()
{
	int valueToPrint;

	if(getValue(&valueToPrint))
	{
		Serial.print(valueToPrint);
	}
}

void remoteWriteOutput()
{
	if (*decodePos == STATEMENT_TERMINATOR | decodePos == decodeLimit)
	{
#ifdef DIAGNOSTICS_ACTIVE
		Serial.println(F("FAIL: missing write output command character"));
#endif
		return;
	}

#ifdef COMMAND_DEBUG
	Serial.println(F(".**write output: "));
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".   Download command : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case 'T':
	case 't':
		doRemoteWriteText();
		break;

	case 'L':
	case 'l':
		doRemoteWriteLine();
		break;

	case 'V':
	case 'v':
		doRemotePrintValue();
		break;
	}
}


void actOnCommand(char * commandDecodePos, char * comandDecodeLimit)
{
	decodePos = commandDecodePos;
	decodeLimit = comandDecodeLimit;

	*decodeLimit = 0;

#ifdef COMMAND_DEBUG
	Serial.print(F(".**processCommand:"));
	Serial.println((char *)decodePos);
#endif

	char commandCh = *decodePos;

#ifdef COMMAND_DEBUG
	Serial.print(F(".  Command code : "));
	Serial.println(commandCh);
#endif

	decodePos++;

	switch (commandCh)
	{
	case '#':
		// Ignore comments
		break;
	case 'I':
	case 'i':
		information();
		break;
	case 'M':
	case 'm':
		remoteMoveControl();
		break;
	case 'P':
	case 'p':
		remotePixelControl();
		break;
	case 'C':
	case 'c':
		programControl();
		break;
	case 'R':
	case 'r':
		remoteManagement();
		break;
	case 'V':
	case 'v':
		variableManagement();
		break;
	case 's':
	case 'S':
		remoteSoundPlay();
		break;

	case 'w':
	case 'W':
		remoteWriteOutput();
		break;
	default:
#ifdef COMMAND_DEBUG
		Serial.println(F(".  Invalid command : "));
#endif
#ifdef DIAGNOSTICS_ACTIVE
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("Invalid Command: "));
			Serial.print(commandCh);
			Serial.print(F(" code: "));
			Serial.println((int)commandCh);
		}
#endif
		break;
	}
}

void processCommandByte(byte b)
{
	if (commandPos == bufferLimit)
	{
#ifdef COMMAND_DEBUG
		Serial.println(F(".  Command buffer full - resetting"));
#endif
		resetCommand();
		return;
	}

	*commandPos = b;

	commandPos++;

	if (b == STATEMENT_TERMINATOR)
	{
#ifdef COMMAND_DEBUG
		Serial.println(F(".  Command end"));
#endif
		actOnCommand(programCommand, commandPos);
		resetCommand();
		return;
	}
}

void resetSerialBuffer()
{
	remotePos = remoteCommand;
	remoteLimit = remoteCommand + COMMAND_BUFFER_SIZE;
}

void interpretSerialByte(byte b)
{
	if (remotePos == remoteLimit)
	{
		resetSerialBuffer();
		return;
	}

	*remotePos = b;
	remotePos++;

	if (b == STATEMENT_TERMINATOR)
	{
#ifdef COMMAND_DEBUG
		Serial.println(F(".  Command end"));
#endif
		actOnCommand(remoteCommand, remotePos);
		resetSerialBuffer();
		return;
	}
}

void processSerialByte(byte b)
{
#ifdef COMMAND_DEBUG
	Serial.print(F(".**processSerialByte: "));
	Serial.println((char)b);
#endif

	switch (deviceState)
	{
	case EXECUTE_IMMEDIATELY:
		decodeScriptChar(b, interpretSerialByte);
		break;
	case STORE_PROGRAM:
		decodeScriptChar(b, storeReceivedByte);
		break;
	}
}


void setupRemoteControl()
{
#ifdef COMMAND_DEBUG
	Serial.println(F(".**setupRemoteControl"));
#endif
	resetCommand();
	resetSerialBuffer();
}

// Executes the statement in the EEPROM at the current program counter
// The statement is assembled into a buffer by interpretCommandByte

bool exeuteProgramStatement()
{
	char programByte;

#ifdef PROGRAM_DEBUG
	Serial.println(F(".Executing statement"));
#endif

#ifdef DIAGNOSTICS_ACTIVE
	if (diagnosticsOutputLevel & LINE_NUMBERS)
	{
		Serial.print(F("Offset: "));
		Serial.println((int)programCounter);
	}
#endif

	while (true)
	{
		programByte = EEPROM.read(programCounter++);

		if (programCounter >= EEPROM_SIZE || programByte == PROGRAM_TERMINATOR)
		{
			haltProgramExecution();
			return false;
		}

#ifdef PROGRAM_DEBUG
		Serial.print(F(".    program byte: "));
		Serial.println(programByte);
#endif

		processCommandByte(programByte);

		if (programByte == STATEMENT_TERMINATOR)
			return true;

	}
}

#ifdef TEST_PROGRAM

//const char SAMPLE_CODE[] PROGMEM = { "PC255,0,0\rCD5\rCLtop\rPC0,0,255\rCD5\rPC0,255,255\rCD5\rPC255,0,255\rCD5\rCJtop\r" };
const char SAMPLE_CODE[] PROGMEM = { "CLtop\rCM10,close\rPC255,0,0\rCJtop\rCLclose\rPC0,0,255\rCJtop\r" };

void loadTestProgram(int offset)
{
	int inPos = 0;
	int outPos = offset;

	int len = strlen_P(SAMPLE_CODE);
	int i;
	char myChar;

	for (i = 0; i < len; i++)
	{
		myChar = pgm_read_byte_near(SAMPLE_CODE + i);
		EEPROM.write(outPos++, myChar);
	}

	EEPROM.write(outPos, 0);

	dumpProgramFromEEPROM(offset);
}

#endif

void updateRobot()
{

	// If we recieve serial data the program that is running
	// must stop. 
	while (CharsAvailable())
	{
		byte b = GetRawCh();
		processSerialByte(b);
	}

	switch (programState)
	{
	case PROGRAM_STOPPED:
	case PROGRAM_PAUSED:
		break;
	case PROGRAM_ACTIVE:
		exeuteProgramStatement();
		break;
	case PROGRAM_AWAITING_MOVE_COMPLETION:
		if (!motorsMoving())
		{
			programState = PROGRAM_ACTIVE;
		}
		break;
	case PROGRAM_AWAITING_DELAY_COMPLETION:
		if (millis() > delayEndTime)
		{
			programState = PROGRAM_ACTIVE;
		}
		break;
	}
}

bool commandsNeedFullSpeed()
{
	return deviceState != EXECUTE_IMMEDIATELY;
}
