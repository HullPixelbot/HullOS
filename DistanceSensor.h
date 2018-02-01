///////////////////////////////////////////////////////////
/// Distance Reading
///////////////////////////////////////////////////////////

const int trigPin = 3;       // trigger pin for distance 
const int echoPin = 2;       // echo pin for distance

volatile long pulseStartTime;
volatile long pulseWidth;

enum DistanceSensorState
{
  DISTANCE_SENSOR_OFF,
  DISTANCE_SENSOR_ON,
  DISTANCE_SENSOR_BETWEEN_READINGS,
  DISTANCE_SENSOR_AWAITING_READING,
  DISTANCE_SENSOR_READING_READY
};

volatile DistanceSensorState distanceSensorState = DISTANCE_SENSOR_OFF;

volatile int distanceSensorReadingIntervalInMillisecs;

volatile unsigned long timeOfLastDistanceReading;

void pulseEvent()
{
  if (PIND & (1 << echoPin)) {
    // pulse gone high - record start
    pulseStartTime = micros();
  }
  else
  {
    pulseWidth = micros() - pulseStartTime;
    distanceSensorState = DISTANCE_SENSOR_READING_READY;
  }
}

void updateDistanceSensorReadingInterval(int readingIntervalInMillisecs)
{
  distanceSensorReadingIntervalInMillisecs = readingIntervalInMillisecs;
}

inline void startDistanceSensorReading()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  distanceSensorState = DISTANCE_SENSOR_AWAITING_READING;

  digitalWrite(trigPin, LOW);

  // let the signals settle (actually I've no idea why this is needed)

  delay(5);
}

void setupDistanceSensor(int readingIntervalInMillisecs)
{
  if (distanceSensorState != DISTANCE_SENSOR_OFF)
    return;

  updateDistanceSensorReadingInterval(readingIntervalInMillisecs);

  pulseWidth = 0;
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(echoPin), pulseEvent, CHANGE);

  startDistanceSensorReading();
}

inline void startWaitBetweenReadings()
{
  timeOfLastDistanceReading = millis();

  distanceSensorState = DISTANCE_SENSOR_BETWEEN_READINGS;
}

// checks to see if it is time to take another reading

inline void updateSensorBetweenReadings()
{
  unsigned long now = millis();
  unsigned long timeSinceLastReading = ulongDiff(now, timeOfLastDistanceReading);


  if (timeSinceLastReading >= distanceSensorReadingIntervalInMillisecs)
  {
    startDistanceSensorReading();
  }
}

void updateDistanceSensor()
{
  switch (distanceSensorState)
  {
  case DISTANCE_SENSOR_OFF:
    // if the sensor has not been turned on - do nothing
    break;

  case DISTANCE_SENSOR_ON:
    // if the sensor is on, start a reading
    startDistanceSensorReading();
    break;

  case DISTANCE_SENSOR_AWAITING_READING:
    // if the sensor is awaiting a reading - do nothing
    break;

  case DISTANCE_SENSOR_BETWEEN_READINGS:
    // if the sensor is between readings - check the timer
    updateSensorBetweenReadings();
    break;

  case DISTANCE_SENSOR_READING_READY:
    startWaitBetweenReadings();
    break;
  }
}

int getDistanceValueInt()
{
  return (int)(pulseWidth / 5.8);
}

float getDistanceValueFloat()
{
  return (float)pulseWidth / 5.80;
}


void directDistanceReadTest()
{
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  long duration;
  float distance;
  while (true)
  {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    distance = ((float)duration / 2.0) / 29.0;
    Serial.println(distance);
    delay(500);
  }
}

void testDistanceSensor()
{
  setupMotors();
  fastMoveDistanceInMM(1000, 1000);
  setupDistanceSensor(10);
  //  directDistanceReadTest();
  while (true)
  {
    updateDistanceSensor();
    float distance = getDistanceValueFloat();
    Serial.println(distance);
    delay(100);
  }
}
