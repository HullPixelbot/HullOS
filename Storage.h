#define EEPROM_SIZE 1000
#define STORED_PROGRAM_OFFSET 20

#define PROGRAM_STATUS_BYTE_OFFSET 0
#define PROGRAM_STORED_VALUE1 0xaa
#define PROGRAM_STORED_VALUE2 0x55

#define WHEEL_SETTINGS_OFFSET 2

// Stores a program byte into the eeprom at the stated location
// The pos value is the offset in the EEProm into which the program is to be written
// The function returns true if the byte was stored, false if not

bool storeByteIntoEEPROM(char byte, int pos)
{
  if (pos > EEPROM_SIZE)
    return false;
  EEPROM.update(pos, byte);
  return true;
}

//#define DEBUG_STORE_BLOCK_IN_EEPROM

bool storeBlockIntoEEPROM(uint8_t * blockStart, int length, int pos)
{

#ifdef DEBUG_STORE_BLOCK_IN_EEPROM
  Serial.println(F("Store block in eeprom"));
  Serial.print(F("    length:"));
  Serial.print(length);
  Serial.print(F("    pos:"));
  Serial.println(pos);
#endif

  for (int i = 0; i < length; i++)
  {
#ifdef DEBUG_STORE_BLOCK_IN_EEPROM
    Serial.print(F("    Byte: "));
    Serial.println(blockStart[i]);
    Serial.print(F(" pos: "));
    Serial.println(blockStart[i]);
#endif
    if (!storeByteIntoEEPROM(blockStart[i], pos))
    {
      return false;
    }
    pos++;
  }
  return true;
}

bool loadBlockFromEEPROM(uint8_t * blockStart, int length, int pos)
{
  for (int i = 0; i < length; i++)
  {
    if (pos > EEPROM_SIZE)
      return false;
    blockStart[i] = EEPROM.read(pos);
    pos++;
  }
  return true;
}

// Stores a program into the eeprom at the stated location
// The program is a string of text which is zero terminated
// The EEPromStart value is the offset in the EEProm into which the program is to be written
// The function returns true if the program was loaded, false if not

bool storeProgramIntoEEPROM(char * programStart, int EEPromStart)
{
  while (*programStart)
  {
    if (!storeByteIntoEEPROM(*programStart, EEPromStart))
      return false;
    programStart++;
    EEPromStart++;
  }

  // put the terminator on the end of the program
  storeByteIntoEEPROM(*programStart, EEPromStart);
  return true;
}

void setProgramStored()
{
  storeByteIntoEEPROM(PROGRAM_STORED_VALUE1, PROGRAM_STATUS_BYTE_OFFSET);
  storeByteIntoEEPROM(PROGRAM_STORED_VALUE2, PROGRAM_STATUS_BYTE_OFFSET + 1);
}

void clearProgramStoredFlag()
{
  storeByteIntoEEPROM(0, PROGRAM_STATUS_BYTE_OFFSET);
  storeByteIntoEEPROM(0, PROGRAM_STATUS_BYTE_OFFSET + 1);
}

bool isProgramStored()
{
  if ((EEPROM.read(PROGRAM_STATUS_BYTE_OFFSET) == PROGRAM_STORED_VALUE1) &
    (EEPROM.read(PROGRAM_STATUS_BYTE_OFFSET + 1) == PROGRAM_STORED_VALUE2))
    return true;
  else
    return false;
}


