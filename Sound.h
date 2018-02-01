/* This version of the sound uses A0 and A1 for the sound output. A1 is pulled
down to 0 and A1 is used as the sound output pin.*/

void setupSound()
{
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  noTone(A1);
  noTone(A0);
}

void playTone(int frequency, unsigned long duration)
{
  tone(A0, frequency, duration);
}
