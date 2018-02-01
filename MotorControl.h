///////////////////////////////////////////////////////////
/// Motor control
///////////////////////////////////////////////////////////

#include <TimerOne.h>
#include <limits.h>
#include <math.h>

const byte leftMotorWaveformLookup[8] = { B10000000, B11000000, B01000000, B01100000, B00100000, B00110000, B00010000, B10010000 };
const byte rightMotorWaveformLookup[8] = { B01000, B01100, B00100, B00110, B00010, B00011, B00001, B01001 };

volatile char leftMotorWaveformPos = 0;
volatile char leftMotorWaveformDelta = 0;

volatile char rightMotorWaveformPos = 0;
volatile char rightMotorWaveformDelta = 0;

volatile unsigned long leftStepCounter = 0;
volatile unsigned long leftNumberOfStepsToMove = 1000;

volatile unsigned long rightStepCounter = 0;
volatile unsigned long rightNumberOfStepsToMove = 1000;

volatile unsigned long leftIntervalBetweenSteps;
volatile unsigned long rightIntervalBetweenSteps;

volatile unsigned long leftTimeOfLastStep;
volatile unsigned long rightTimeOfLastStep;

volatile unsigned long leftTimeOfNextStep;
volatile unsigned long rightTimeOfNextStep;

inline void leftStep()
{
  // If we are not moving, don't do anything
  if (leftMotorWaveformDelta == 0)return;

  // Move the motor one step

#ifdef WEMOS
  setLeft(lmotorPos);
#else
  PORTD = (PORTD & 0x0F) + leftMotorWaveformLookup[leftMotorWaveformPos];
#endif

  // Update and wrap the waveform position
  leftMotorWaveformPos += leftMotorWaveformDelta;
  if (leftMotorWaveformPos == 8) leftMotorWaveformPos = 0;
  if (leftMotorWaveformPos < 0) leftMotorWaveformPos = 7;

  // If we are not counting steps - just return

  // Check for end of move
  if (++leftStepCounter >= leftNumberOfStepsToMove)
  {
    leftMotorWaveformDelta = 0;
    PORTD = (PORTD & 0x0F);
  }
}

inline void rightStep()
{
  if (rightMotorWaveformDelta == 0)return;

#ifdef WEMOS
  setRight(rmotorPos);
#else
  PORTB = (PORTB & 0xF0) + rightMotorWaveformLookup[rightMotorWaveformPos];
#endif

  rightMotorWaveformPos -= rightMotorWaveformDelta;
  if (rightMotorWaveformPos == 8) rightMotorWaveformPos = 0;
  if (rightMotorWaveformPos < 0) rightMotorWaveformPos = 7;

  if (++rightStepCounter >= rightNumberOfStepsToMove)
  {
    rightMotorWaveformDelta = 0;
    PORTB = (PORTB & 0xF0);
  }
}

float turningCircle;
const int countsperrev = 512 * 8; // number of microsteps per full revolution

float leftStepsPerMM;
float rightStepsPerMM;

struct wheelSettings
{
  int leftWheelDiameter;
  int rightWheelDiameter;
  int wheelSpacing;
  char check;
};

wheelSettings activeWheelSettings;

#define WHEEL_SETTINGS_STORED 0x55

void storeActiveWheelSettings()
{
  storeBlockIntoEEPROM((uint8_t *)& activeWheelSettings, sizeof(struct wheelSettings), WHEEL_SETTINGS_OFFSET);
}

void setActiveWheelSettings(int leftDiam, int rightDiam, int spacing)
{
  activeWheelSettings.leftWheelDiameter = leftDiam;
  activeWheelSettings.rightWheelDiameter = rightDiam;
  activeWheelSettings.wheelSpacing = spacing;
  storeActiveWheelSettings();
}

void dumpActiveWheelSettings()
{
  Serial.println(F("Wheel settings"));
  Serial.print(F("Left diameter: "));
  Serial.println(activeWheelSettings.leftWheelDiameter);
  Serial.print(F("Right diameter: "));
  Serial.println(activeWheelSettings.rightWheelDiameter);
  Serial.print(F("Wheel spacing: "));
  Serial.println(activeWheelSettings.wheelSpacing);
}

//#define DEBUG_LOAD_ACTIVE_WHEEL_SETTINGS

void loadActiveWheelSettings()
{

#ifdef DEBUG_LOAD_ACTIVE_WHEEL_SETTINGS
  Serial.println(F("Loading active wheel settings"));
#endif

  loadBlockFromEEPROM((uint8_t *)& activeWheelSettings, sizeof(struct wheelSettings), WHEEL_SETTINGS_OFFSET);

  if (activeWheelSettings.check != WHEEL_SETTINGS_STORED)
  {

#ifdef DEBUG_LOAD_ACTIVE_WHEEL_SETTINGS
    Serial.println(F("No settings found - using default"));
#endif

    activeWheelSettings.leftWheelDiameter = 69;
    activeWheelSettings.rightWheelDiameter = 69;
    activeWheelSettings.wheelSpacing = 110;
    activeWheelSettings.check = WHEEL_SETTINGS_STORED;
    storeActiveWheelSettings();
  }

#ifdef DEBUG_LOAD_ACTIVE_WHEEL_SETTINGS
  Serial.print(F("left radius: "));
  Serial.print(activeWheelSettings.leftWheelRadius);
  Serial.print(F(" right radius: "));
  Serial.print(activeWheelSettings.rightWheelRadius);
  Serial.print(F(" wheel spacing: "));
  Serial.println(activeWheelSettings.wheelSpacing);
#endif
}

float leftWheelCircumference;
float rightWheelCircumference;

void setupWheelSettings()
{
  leftWheelCircumference = PI * activeWheelSettings.leftWheelDiameter;
  rightWheelCircumference = PI * activeWheelSettings.rightWheelDiameter;
  turningCircle = activeWheelSettings.wheelSpacing * PI;

  leftStepsPerMM = countsperrev / leftWheelCircumference;
  rightStepsPerMM = countsperrev / rightWheelCircumference;
}

void setupMotors()
{
  DDRD = 0xFF;
  DDRB = 0x0F;

  loadActiveWheelSettings();

  setupWheelSettings();

  Timer1.initialize(1000);
}

inline unsigned long ulongDiff(unsigned long end, unsigned long start)
{
  if (end >= start)
  {
    return end - start;
  }
  else
  {
    return ULONG_MAX - start + end + 1;
  }
}

volatile unsigned long currentMicros;
volatile unsigned long leftTimeSinceLastStep;
volatile unsigned long rightTimeSinceLastStep;
volatile unsigned long timeToLeft;
volatile unsigned long timeToRight;


// If we try to trigger an interrupt too soon after the current one
// this causes problems. If the time to the next interrupt is less than 
// this figure we act on it now and then add the latency to the time of the 
// next tick

const unsigned long interruptLatencyInMicroSecs = 150;

// The lowest interval between steps that is allowed
// used to calculate timed moves

const unsigned long minInterruptIntervalInMicroSecs = 1200;

void motorUpdate()
{
  // This method runs when a move interrupt has fired
  // The interrupts are timed to fire when the next move is due
  // The interrupt will be delayed if the time of the next one 
  // is less than the set latency 

  currentMicros = micros();

  if (leftMotorWaveformDelta != 0)
  {
    // left is moving - see if it is time to do a step
    leftTimeSinceLastStep = ulongDiff(currentMicros, leftTimeOfLastStep);
    if (leftTimeSinceLastStep >= leftIntervalBetweenSteps)
    {
      leftStep();
      leftTimeOfLastStep = currentMicros - (leftTimeSinceLastStep - leftIntervalBetweenSteps);
      leftTimeOfNextStep = currentMicros + leftIntervalBetweenSteps;
    }
  }

  if (rightMotorWaveformDelta != 0)
  {
    rightTimeSinceLastStep = ulongDiff(currentMicros, rightTimeOfLastStep);

    if (rightTimeSinceLastStep >= rightIntervalBetweenSteps)
    {
      rightStep();
      rightTimeOfLastStep = currentMicros - (rightTimeSinceLastStep - rightIntervalBetweenSteps);
      rightTimeOfNextStep = currentMicros + rightIntervalBetweenSteps;
    }
  }

  if ((leftMotorWaveformDelta != 0) & (rightMotorWaveformDelta != 0))
  {
    timeToLeft = ulongDiff(leftTimeOfNextStep, currentMicros);
    timeToRight = ulongDiff(rightTimeOfNextStep, currentMicros);

    if (timeToLeft < timeToRight)
    {
      // If the time for the interrupt is too short - 
      // inccrease the value to the tolerance
      if (timeToLeft < interruptLatencyInMicroSecs)
        timeToLeft = interruptLatencyInMicroSecs;
      Timer1.setPeriod(timeToLeft);
    }
    else
    {
      if (timeToRight < interruptLatencyInMicroSecs)
        timeToRight = interruptLatencyInMicroSecs;
      Timer1.setPeriod(timeToRight);
    }
    return;
  }
  else
  {
    if (leftMotorWaveformDelta != 0)
    {
      timeToLeft = ulongDiff(leftTimeOfNextStep, currentMicros);
      if (timeToLeft < interruptLatencyInMicroSecs)
        timeToLeft = interruptLatencyInMicroSecs;
      Timer1.setPeriod(timeToLeft);
      return;
    }
    else
    {
      timeToRight = ulongDiff(rightTimeOfNextStep, currentMicros);
      if (timeToRight < interruptLatencyInMicroSecs)
        timeToRight = interruptLatencyInMicroSecs;
      Timer1.setPeriod(timeToRight);
      return;
    }
  }

  // if we get here both motors have stopped
  // turn off the interrupts
  Timer1.detachInterrupt();
}

inline void startMotor(unsigned long stepLimit, unsigned long microSecsPerPulse, bool forward,
  volatile unsigned long * motorStepLimit, volatile unsigned long * motorPulseInterval,
  volatile char * motorDelta, volatile char * motorPos)
{
  // If we are not moving - set the delta to zero and return

  if (stepLimit == 0)
  {
    motorDelta = 0;
    return;
  }

  *motorStepLimit = stepLimit;

  *motorPulseInterval = microSecsPerPulse;

  if (forward)
  {
    *motorDelta = 1;
  }
  else
  {
    *motorDelta = -1;
  }

  *motorPos = 0;
}

void startMotors(
  unsigned long leftSteps, unsigned long rightSteps,
  unsigned long leftMicroSecsPerPulse, unsigned long rightMicroSecsPerPulse,
  bool leftForward, bool rightForward)
{
  // Set up the counters for the left and right motor moves

  startMotor(leftSteps, leftMicroSecsPerPulse, leftForward,
    &leftNumberOfStepsToMove, &leftIntervalBetweenSteps, &leftMotorWaveformDelta, &leftMotorWaveformPos);

  startMotor(rightSteps, rightMicroSecsPerPulse, rightForward,
    &rightNumberOfStepsToMove, &rightIntervalBetweenSteps, &rightMotorWaveformDelta, &rightMotorWaveformPos);

  // Now set up the interrupts 

  unsigned long microSecondsAtLastInterrupt = micros();

  leftTimeOfLastStep = microSecondsAtLastInterrupt;
  rightTimeOfLastStep = microSecondsAtLastInterrupt;

  leftStepCounter = 0;
  rightStepCounter = 0;

  // These calculations might wrap round - but that's OK because the difference 
  // calculation in the interrupt handler will deal with this

  leftTimeOfNextStep = microSecondsAtLastInterrupt + leftMicroSecsPerPulse;
  rightTimeOfNextStep = microSecondsAtLastInterrupt + rightMicroSecsPerPulse;

  if ((leftMotorWaveformDelta != 0) & (rightMotorWaveformDelta != 0))
  {
    if (leftMicroSecsPerPulse < rightMicroSecsPerPulse)
    {
      Timer1.attachInterrupt(motorUpdate, leftMicroSecsPerPulse);
    }
    else
    {
      Timer1.attachInterrupt(motorUpdate, rightMicroSecsPerPulse);
    }
    return;
  }
  else
  {
    if (leftMotorWaveformDelta != 0)
    {
      Timer1.attachInterrupt(motorUpdate, leftMicroSecsPerPulse);
      return;
    }
    else
    {
      Timer1.attachInterrupt(motorUpdate, rightMicroSecsPerPulse);
      return;
    }
  }
}

typedef enum MoveFailReason
{
  Move_OK,
  Left_Distance_Too_Large,
  Right_Distance_Too_Large,
  Left_And_Right_Distance_Too_Large
};

//#define DEBUG_TIMED_MOVE

MoveFailReason timedMoveSteps(long leftStepsToMove, long rightStepsToMove, float timeToMoveInSeconds)
{
#ifdef DEBUG_TIMED_MOVE
  Serial.println("timedMoveSteps");
  Serial.print("    Left steps to move: ");
  Serial.print(leftStepsToMove);
  Serial.print(" Right steps to move: ");
  Serial.print(rightStepsToMove);
  Serial.print(" Time to move in seconds: ");
  Serial.println(timeToMoveInSeconds);
#endif

  long leftInterruptIntervalInMicroSeconds;

  if (leftStepsToMove != 0)
  {
    leftInterruptIntervalInMicroSeconds = (long) ((timeToMoveInSeconds / (double)abs(leftStepsToMove)) * 1000000L + 0.5);
  }
  else
  {
    leftInterruptIntervalInMicroSeconds = minInterruptIntervalInMicroSecs;
  }

  long rightInterruptIntervalInMicroseconds;

  if (rightStepsToMove != 0)
  {
    rightInterruptIntervalInMicroseconds = (long)((timeToMoveInSeconds / (double)abs(rightStepsToMove)) * 1000000L +0.5);
  }
  else
  {
    rightInterruptIntervalInMicroseconds = minInterruptIntervalInMicroSecs;
  }

#ifdef DEBUG_TIMED_MOVE
  Serial.print("    Left interval in microseconds: ");
  Serial.print(leftInterruptIntervalInMicroSeconds);
  Serial.print(" Right interval in microseconds: ");
  Serial.println(rightInterruptIntervalInMicroseconds);
#endif

  // There's a minium gap allowed between intervals. This is set by the top speed of the motors
  // Presently set at 1000 microseconds

  if (leftInterruptIntervalInMicroSeconds < minInterruptIntervalInMicroSecs & rightInterruptIntervalInMicroseconds < minInterruptIntervalInMicroSecs)
  {
    return Left_And_Right_Distance_Too_Large;
  }

  if (leftInterruptIntervalInMicroSeconds < minInterruptIntervalInMicroSecs & rightInterruptIntervalInMicroseconds > minInterruptIntervalInMicroSecs)
  {
    return Left_Distance_Too_Large;
  }

  if (rightInterruptIntervalInMicroseconds < minInterruptIntervalInMicroSecs & leftInterruptIntervalInMicroSeconds > minInterruptIntervalInMicroSecs)
  {
    return Right_Distance_Too_Large;
  }

  // If we get here we can move at this speed.

  startMotors(abs(leftStepsToMove), abs(rightStepsToMove),
    leftInterruptIntervalInMicroSeconds, rightInterruptIntervalInMicroseconds,
    leftStepsToMove > 0, rightStepsToMove > 0);

  return Move_OK;
}

//#define DEBUG_FAST_MOVE_STEPS

float fastMoveSteps(long leftStepsToMove, long rightStepsToMove)
{

#ifdef DEBUG_FAST_MOVE_STEPS
  Serial.println("fastMoveSteps");
  Serial.print("    Left steps to move: ");
  Serial.print(leftStepsToMove);
  Serial.print(" Right steps to move: ");
  Serial.println(rightStepsToMove);
#endif

  // work out how long it will take to move in seconds

  float timeForLeftMoveInSeconds;

  if (leftStepsToMove != 0)
  {
    timeForLeftMoveInSeconds = ((float)abs(leftStepsToMove) * (float)minInterruptIntervalInMicroSecs) / 1000000.0;
  }
  else
  {
    timeForLeftMoveInSeconds = 0;
  }

  float timeForRightMoveInSeconds;

  if (rightStepsToMove != 0)
  {
    timeForRightMoveInSeconds = ((float)abs(rightStepsToMove) * (float)minInterruptIntervalInMicroSecs) / 1000000.0;
  }
  else
  {
    timeForRightMoveInSeconds = 0;
  }

#ifdef DEBUG_FAST_MOVE_STEPS
  Serial.print("    Left time to move: ");
  Serial.print(timeForLeftMoveInSeconds);
  Serial.print(" Right time to move: ");
  Serial.println(timeForRightMoveInSeconds);
#endif

  // Allow time for the slowest mover
  if (timeForLeftMoveInSeconds > timeForRightMoveInSeconds)
  {
    timedMoveSteps(leftStepsToMove, rightStepsToMove, timeForLeftMoveInSeconds);
    return timeForLeftMoveInSeconds;
  }
  else
  {
    timedMoveSteps(leftStepsToMove, rightStepsToMove, timeForRightMoveInSeconds);
    return timeForRightMoveInSeconds;
  }
}

//#define TIMED_MOVE_MM_DEBUG

int timedMoveDistanceInMM(float leftMMs, float rightMMs, float timeToMoveInSeconds)
{

#ifdef TIMED_MOVE_MM_DEBUG
  Serial.println("timedMoveDistanceInMM");
  Serial.print("    Left mms to move: ");
  Serial.print(leftMMs);
  Serial.print(" Right mms to move: ");
  Serial.print(rightMMs);
  Serial.print(" Time to move in seconds: ");
  Serial.println(timeToMoveInSeconds);
#endif

  long leftSteps = (long)(leftMMs * leftStepsPerMM + 0.5);
  long rightSteps = (long)(rightMMs * rightStepsPerMM + 0.5);

#ifdef TIMED_MOVE_MM_DEBUG
  Serial.print("    Left steps to move: ");
  Serial.print(leftSteps);
  Serial.print(" Right steps to move: ");
  Serial.println(rightSteps);
#endif

  return timedMoveSteps(leftSteps, rightSteps, timeToMoveInSeconds);
}

//#define FAST_MOVE_MM_DEBUG

int fastMoveDistanceInMM(float leftMMs, float rightMMs)
{

#ifdef FAST_MOVE_MM_DEBUG
  Serial.println("fastMoveDistanceInMM");
  Serial.print("    Left mms to move: ");
  Serial.print(leftMMs);
  Serial.print(" Right mms to move: ");
  Serial.println(rightMMs);
  Serial.print("    Left mms per step: ");
  Serial.print(leftStepsPerMM);
  Serial.print(" Right mms per step: ");
  Serial.println(rightStepsPerMM);

#endif

  long leftSteps = (long)(leftMMs * leftStepsPerMM + 0.5);
  long rightSteps = (long)(rightMMs * rightStepsPerMM + 0.5);

#ifdef FAST_MOVE_MM_DEBUG
  Serial.print("    Left steps to move: ");
  Serial.print(leftSteps);
  Serial.print(" Right steps to move: ");
  Serial.println(rightSteps);
#endif

  int result = fastMoveSteps(leftSteps, rightSteps);

  return result;
}

void rightStop()
{
  PORTB = (PORTB & 0xF0);
  rightMotorWaveformDelta = 0;
  rightStepCounter = 0;
}

void leftStop()
{
  PORTD = (PORTD & 0x0F);
  leftMotorWaveformDelta = 0;
  leftStepCounter = 0;
}


void motorStop()
{
  leftStop();
  rightStop();
}

bool motorsMoving()
{
  if (rightMotorWaveformDelta != 0) return true;
  if (leftMotorWaveformDelta != 0) return true;
  return false;
}

void waitForMotorsStop()
{
  while (motorsMoving())
    delay(1);
}

//#define DEBUG_FAST_ROTATE

void fastRotateRobot(float angle)
{
  float noOfTurns = angle / 360.0;
  float distanceToRotate = noOfTurns*turningCircle;

  fastMoveDistanceInMM(distanceToRotate, -distanceToRotate);

#ifdef DEBUG_FAST_ROTATE
  Serial.print(". angle: ");
  Serial.print(angle);
  Serial.print(" noOfTurns: ");
  Serial.print(noOfTurns);
  Serial.print(" distanceToRotate: ");
  Serial.println(distanceToRotate);
#endif
}

//#define DEBUG_TIMED_ROTATE

int timedRotateRobot(float angle,float timeToMoveInSeconds)
{
  float noOfTurns = angle / 360.0;
  float distanceToRotate = noOfTurns*turningCircle;

#ifdef DEBUG_TIMED_ROTATE
  Serial.print(". angle: ");
  Serial.print(angle);
  Serial.print(" noOfTurns: ");
  Serial.print(noOfTurns);
  Serial.print(" time: ");
  Serial.print(timeToMoveInSeconds);
  Serial.print(" distanceToRotate: ");
  Serial.println(distanceToRotate);
#endif

  return timedMoveDistanceInMM(distanceToRotate, -distanceToRotate, timeToMoveInSeconds);
}

//#define DEBUG_FAST_ARC

void fastMoveArcRobot(float radius,float angle)
{
  float noOfTurns = angle / 360.0;
  float absRadius = abs(radius);

  float leftDistanceToMove = noOfTurns*((absRadius +(activeWheelSettings.wheelSpacing /2.0))*2.0*PI);
  float rightDistanceToMove = noOfTurns*((absRadius - (activeWheelSettings.wheelSpacing / 2.0))*2.0*PI);

#ifdef DEBUG_FAST_ARC
  Serial.println("fastMoveArcRobot");
  Serial.print(" radius: ");
  Serial.print(radius);
  Serial.print(" angle: ");
  Serial.print(angle);
  Serial.print(" leftDistanceToMove: ");
  Serial.print(leftDistanceToMove);
  Serial.print(" rightDistanceToMove: ");
  Serial.println(rightDistanceToMove);
#endif

  if (radius >= 0)
  {
    fastMoveDistanceInMM(leftDistanceToMove, rightDistanceToMove);
  }
  else
  {
    fastMoveDistanceInMM(rightDistanceToMove, leftDistanceToMove);
  }
}

//#define DEBUG_TIMED_ARC

int timedMoveArcRobot(float radius, float angle, float timeToMoveInSeconds)
{
  float noOfTurns = angle / 360.0;
  float absRadius = abs(radius);

  float leftDistanceToMove = noOfTurns*((absRadius + (activeWheelSettings.wheelSpacing / 2.0))*2.0*PI);
  float rightDistanceToMove = noOfTurns*((absRadius - (activeWheelSettings.wheelSpacing / 2.0))*2.0*PI);

#ifdef DEBUG_TIMED_ARC
  Serial.println("timedMoveArcRobot");
  Serial.print(" radius: ");
  Serial.print(radius);
  Serial.print(" angle: ");
  Serial.print(angle);
  Serial.print(" time: ");
  Serial.print(timeToMoveInSeconds);
  Serial.print(" leftDistanceToMove: ");
  Serial.print(leftDistanceToMove);
  Serial.print(" rightDistanceToMove: ");
  Serial.println(rightDistanceToMove);
#endif

  if (radius >= 0)
  {
    return timedMoveDistanceInMM(leftDistanceToMove, rightDistanceToMove, timeToMoveInSeconds);
  }
  else
  {
    return timedMoveDistanceInMM(rightDistanceToMove, leftDistanceToMove, timeToMoveInSeconds);
  }
}

