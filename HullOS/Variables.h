// Performs the variable management 
// Variables can be given names, stored and evaluated
// Simple two operand expressions only

#define NUMBER_OF_VARIABLES 20
#define MAX_VARIABLE_NAME_LENGTH 10


enum parseOperandResult {
	INVALID_VARIABLE_NAME=1,
	NO_ROOM_FOR_VARIABLE=2,
	VARIABLE_NOT_FOUND=3,
	VARIABLE_NAME_TOO_LONG=4,
	VARIABLE_NAME_OK=5,
	OPERAND_OK=6,
	USING_UNASSIGNED_VARIABLE=7,
	INVALID_OPERAND=8,
	INVALID_HARDWARE_READING_NAME=9,
	NO_DIGITS_IN_VALUE=10,
	FIRST_VARIABLE_NOT_FOUND=11,
	FIRST_VARIABLE_USED_BEFORE_DEFINITION=12,
	INVALID_OPERATOR=13,
	SECOND_VARIABLE_NOT_FOUND=14,
	SECOND_VARIABLE_USED_BEFORE_DEFINITION=15,
};

//#define VAR_DEBUG

struct op
{
	char operatorCh;
	int(*evaluator) (int, int);
};

int evaluatePlus(int op1, int op2)
{
	return op1 + op2;
}

struct op addOp = { '+', evaluatePlus };

int evaluateMinus(int op1, int op2)
{
	return op1 - op2;
}

struct op minusOp = { '-', evaluateMinus };

int evaluateTimes(int op1, int op2)
{
	return op1 * op2;
}

struct op timesOp = { '*', evaluateTimes };

int evaluateDivide(int op1, int op2)
{
	return op1 / op2;
}

struct op divideOp = { '/', evaluateDivide };

int evaluateModulus(int op1, int op2)
{
	return op1 % op2;
}

struct op modulusOp = { '%', evaluateModulus };

#define NUMBER_OF_ARITHMETIC_OPERATORS 5

struct op * operators[NUMBER_OF_ARITHMETIC_OPERATORS] = { &addOp, &minusOp, &timesOp, &divideOp, &modulusOp };

bool validOperator(char ch)
{
	for (int i = 0; i < NUMBER_OF_ARITHMETIC_OPERATORS; i++)
	{
		if (ch == operators[i]->operatorCh)
			return true;
	}
	return false;
}

op * findOperator(char ch)
{
	for (int i = 0; i < NUMBER_OF_ARITHMETIC_OPERATORS; i++)
	{
		if (ch == operators[i]->operatorCh)
			return operators[i];
	}
	return NULL;
}

struct logicalOp
{
	char * operatorCh;
	bool(*evaluator) (int, int);
};

bool equalsOp(int op1, int op2)
{
	return op1 == op2;
}

struct logicalOp logicEquals = { "==",equalsOp };

bool notEqualsOp(int op1, int op2)
{
	return op1 != op2;
}

struct logicalOp logicNotEquals = { "!=",notEqualsOp };

bool lessThanOp(int op1, int op2)
{
	return op1 < op2;
}

struct logicalOp logicLessThan = { "<",lessThanOp };

bool greaterThanOp(int op1, int op2)
{
	return op1 > op2;
}

struct logicalOp logicGreaterThan = { ">",greaterThanOp };

bool lessThanEqualsOp(int op1, int op2)
{
	return op1 <= op2;
}

struct logicalOp logicLessThanEquals = { "<=",lessThanEqualsOp };

bool greaterThanEqualsOp(int op1, int op2)
{
	return op1 >= op2;
}

struct logicalOp logicGreaterThanEquals = { ">=",greaterThanEqualsOp };

#define NUMBER_OF_LOGICAL_OPERATORS 6

struct logicalOp * logicalOps[NUMBER_OF_LOGICAL_OPERATORS] = { &logicEquals, &logicNotEquals,
  &logicLessThan, &logicGreaterThan,
  &logicLessThanEquals, &logicGreaterThanEquals };

struct logicalOp * findLogicalOp(char * text)
{
	// Some logical operators are two character
	// If they are, the second character is always equals

	char * firstChar = text;
	char * secondChar = text + 1;

	for (int i = 0; i < NUMBER_OF_LOGICAL_OPERATORS; i++)
	{
		struct logicalOp * op = logicalOps[i];
		int opLength = strlen(op->operatorCh);

		if (*secondChar == '=')
		{
			if (opLength == 1)
				continue;

			// two character operator in program code
			if (op->operatorCh[0] == *firstChar)
			{
				return op;
			}
		}
		else
		{
			if (opLength == 2)
				continue;

			// one character operator in program code
			if (op->operatorCh[0] == *firstChar)
			{
				return op;
			}
		}
	}

	return NULL;
}

struct reading {
	char * name;
	int(*reader)(void);
};

int readDistance()
{
	int result = getDistanceValueInt();
	return result;
}

int readLight()
{
	return analogRead(A2);
}

inline bool isReadingNameStart(char * ch)
{
	return (isalpha(*ch));
}

inline bool isReadingNameChar(char * ch)
{
	return (isAlphaNumeric(*ch));
}

#define READING_START_CHAR '@'

struct reading light = { "light", readLight };

struct reading distance = { "distance", readDistance };

int readMove()
{
	if (motorsMoving())
		return 1;
	else
		return 0;
}

struct reading moving = { "moving", readMove };

int readRandom()
{
	return random(1, 13);
}

struct reading randomReading = { "random", readRandom };

#define NO_OF_HARDWARE_READERS 4

struct reading * readers[NO_OF_HARDWARE_READERS] = { &distance, &light, &moving, &randomReading };

bool validReadingz(char * text)
{
	if (!isReadingNameStart(text))
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("Reading name first character not valid"));
		}
		return;
	}

	for (int i = 0; i < NO_OF_HARDWARE_READERS; i++)
	{
		struct reading * currentReader = readers[i];

		char * currentChar = text;
		char * nameChar = currentReader->name;

		while (true)
		{
			if (!*nameChar)
			{
				// reached the end of the current reader 
				// If the corresponding character in the program is not 
				// a valid part of a name, we can consider this a match
				if (!isReadingNameChar(currentChar))
					return true;
			}

			if (*nameChar != *currentChar)
			{
				// not the same
				// move on to the next one
				break;
			}

			// These two match, move on to the next one

			nameChar++;
			currentChar++;
		}
	}
	// If we get here we've spun through all the names and not found a match
	return false;
}

struct reading * getReading(char * text)
{
	if (!isReadingNameStart(text))
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("Reading name first character not valid"));
		}
		return NULL;
	}

	for (int i = 0; i < NO_OF_HARDWARE_READERS; i++)
	{
		struct reading * currentReader = readers[i];

		char * currentChar = text;
		char * nameChar = currentReader->name;

		while (true)
		{
			if (!*nameChar)
			{
				// reached the end of the current reader 
				// If the corresponding character in the program is not 
				// a valid part of a name, we can consider this a match
				if (!isReadingNameChar(currentChar))
				{
					return currentReader;
				}
			}

			if (*nameChar != *currentChar)
			{
				// not the same
				// move on to the next one
				break;
			}

			// These two match, move on to the next one

			nameChar++;
			currentChar++;
		}
	}
	// If we get here we've spun through all the names and not found a match
	return NULL;
}

struct variable
{
	bool empty;
	bool unassigned;
	// add one to the end for the terminating zero
	char name[MAX_VARIABLE_NAME_LENGTH + 1];
	int value;
};

variable variables[NUMBER_OF_VARIABLES];

void clearVariableSlot(int position)
{
	variables[position].empty = true;
	variables[position].unassigned = true;
	variables[position].value = 0;
	variables[position].name[0] = 0;
}

void clearVariables()
{
	// If the initial value of the variable name is zero, the store is empty

	for (int i = 0; i < NUMBER_OF_VARIABLES; i++)
	{
		clearVariableSlot(i);
	}

}

void setupVariables()
{
	clearVariables();
}

void setVariable(int position, int value)
{
	variables[position].value = value;
	variables[position].unassigned = false;
}

int getVariable(int position)
{
	return variables[position].value;
}

bool isAssigned(int position)
{
	return !variables[position].unassigned;
}

inline bool isVariableNameStart(char * ch)
{
	return (isalpha(*ch));
}

inline bool isVariableNameChar(char * ch)
{
	return (isAlphaNumeric(*ch));
}

inline bool variableSlotEmpty(int position)
{
	return variables[position].empty;
}

int checkIdentifier(char * var)
{
	if (!isVariableNameStart(var))
		return INVALID_VARIABLE_NAME;

	var++;

	int size = 1;

	while (isVariableNameChar(var))
	{
		var++;
		size++;
	}

	if (size > MAX_VARIABLE_NAME_LENGTH)
		return VARIABLE_NAME_TOO_LONG;

	return VARIABLE_NAME_OK;
}

bool matchVariable(int position, char * text)
{
#ifdef VAR_DEBUG
	Serial.print(F("Match variable: "));
	Serial.println(position);
#endif

	if (variableSlotEmpty(position))
	{
		// position is empty - not a match
#ifdef VAR_DEBUG
		Serial.print(F("    Empty slot"));
		Serial.println(position);
#endif

		return false;
	}

#ifdef VAR_DEBUG
	Serial.print(F("    Matching: "));
	Serial.println(position);
#endif

	for (int i = 0; i < MAX_VARIABLE_NAME_LENGTH; i++)
	{
#ifdef VAR_DEBUG
		Serial.print(variables[position].name[i]);
		Serial.print(F(":"));
		Serial.print(*text);
		Serial.print(F("  "));
#endif
		if ((variables[position].name[i] == 0) & !isVariableNameChar(text))
		{
			// variable table has ended at the same time as the variable
			// we have a match
			return true;
		}

		// See if we have failed to match
		if (variables[position].name[i] != *text)
		{
			return false;
		}

		// Move on to next character

		text++;
	}
}

// returns the length of the variable name at the given position in the variable store
// used for calculating pointer updates

int getVariableNameLength(int position)
{
	if (variables[position].empty)
		return 0;

	return strlen(variables[position].name);
}

// A variable name must start with a letter and then contain letters and digits only
// This method searches the variable store for a variable of the given name and then 
// sets position to the variable store offset for that variable. 
// Returns OPERAND_OK if all is well
// The parameter points to the area of memory holding the variable name. The variable name is judged to 
// have ended when a non-text/digit character is found
//

parseOperandResult findVariable(char * name, int *position)
{
#ifdef VAR_DEBUG
	Serial.println(F("Finding variable"));
#endif

	if (!isVariableNameStart(name))
	{
#ifdef VAR_DEBUG
		Serial.println(F("    Invalid variable name"));
#endif
		return INVALID_VARIABLE_NAME;
	}

	for (int i = 0; i < NUMBER_OF_VARIABLES; i++)
	{
#ifdef VAR_DEBUG
		Serial.print(F("    Checking variable: "));
		Serial.println(i);
#endif
		if (matchVariable(i, name))
		{
			*position = i;
			return OPERAND_OK;
		}
	}
	return VARIABLE_NOT_FOUND;
}

// finds an empty location in the variable table and returns the offset into that table
// returns NO_ROOM_FOR_VARIABLE if the table is full

parseOperandResult findVariableSlot(int * result)
{
	for (int i = 0; i < NUMBER_OF_VARIABLES; i++)
	{
		if (variableSlotEmpty(i))
		{
			*result = i;
			return OPERAND_OK;
		}
	}

	return NO_ROOM_FOR_VARIABLE;
}

// returns INVALID_VARIABLE_NAME if the name is invalid 
// returns NO_ROOM_FOR_VARIABLE if the variable cannot be stored
// returns VARIABLE_NAME_TOO_LONG if the name of the variable is longer than the store length

parseOperandResult createVariable(char * namePos, int * varPos)
{
	// Start position for the decode process

	char * decodePos = namePos;
	int position;

#ifdef VAR_DEBUG
	Serial.println(F("Creating variable"));
#endif

	if (findVariableSlot(&position) == NO_ROOM_FOR_VARIABLE)
	{

#ifndef VAR_DEBUG
		Serial.println(F("   no room for variable"));
#endif
		return NO_ROOM_FOR_VARIABLE;
	}

	// Need a valid variable name start - must be a letter
	if (!isVariableNameStart(decodePos))
	{
#ifdef VAR_DEBUG
		Serial.println(F("   invalid variable name"));
#endif
		return INVALID_VARIABLE_NAME;
	}

	int i;

	for (i = 0; i < MAX_VARIABLE_NAME_LENGTH; i++)
	{
		// store the variable name
		variables[position].name[i] = *decodePos;

		decodePos++;

#ifdef VAR_DEBUG
		Serial.print(variables[position].name[i]);
		Serial.print(F(":"));
		Serial.print(*decodePos);
		Serial.print(F("  "));
#endif

		if (!isVariableNameChar(decodePos))
		{
			// If we are 
			if (i < (MAX_VARIABLE_NAME_LENGTH - 1))
				// end of variable name
				// end the name string
				// Note that we declared this one element larger to make room 
				// for the zero
				variables[position].name[i + 1] = 0;
			variables[position].empty = false;
			// return the position value
			*varPos = position;
			return OPERAND_OK;
		}
	}

	// Reached the end of the store without reaching the end of the variable
	clearVariableSlot(position);
	return VARIABLE_NAME_TOO_LONG;
}

// Variable management
// Uses the decode buffer pointers
//

//#define READ_INTEGER_DEBUG

bool readInteger(int * result)
{
#ifdef READ_INTEGER_DEBUG
	Serial.println(".**readInteger");
#endif
	int sign = 1;
	int resultValue = 0;
	bool gotDigit = false;

	if (*decodePos == '-')
	{
#ifdef READ_INTEGER_DEBUG
		Serial.println(".  negative number");
#endif
		sign = -1;
		decodePos++;
	}

	if (*decodePos == '+')
	{
#ifdef READ_INTEGER_DEBUG
		Serial.println(".  positive number");
#endif
		decodePos++;
	}

	while (decodePos != decodeLimit)
	{
		char ch = *decodePos;

#ifdef READ_INTEGER_DEBUG
		Serial.print(".  processing: ");
		Serial.println((char)ch);
#endif

		if (ch<'0' | ch>'9')
		{
#ifdef READ_INTEGER_DEBUG
			Serial.println(".  not a digit ");
#endif
			break;
		}

		resultValue = (resultValue * 10) + (ch - '0');

#ifdef READ_INTEGER_DEBUG
		Serial.print(".  result: ");
		Serial.println(resultValue);
#endif

		gotDigit = true;
		decodePos++;
	}

	resultValue = resultValue * sign;

#ifdef READ_INTEGER_DEBUG
	Serial.print(".  returning: ");
	Serial.println(resultValue);
#endif

	*result = resultValue;
	return gotDigit;
}

// Gets an operand from the current data feed
// This will either be a literal value, the contents of a variable or the contents of a system variable
// it returns an error code

void skipCodeSpaces(void)
{
	while (*decodePos == ' ')
	{
		decodePos++;
	}
}

parseOperandResult parseOperand(int * result)
{
#ifdef VAR_DEBUG
	Serial.println(F("Get operand"));
#endif

	skipCodeSpaces();

	if (isVariableNameStart(decodePos))
	{
#ifdef VAR_DEBUG
		Serial.println(F("    Getting variable operand"));
#endif
		// its a variable
		int position;

		if (findVariable(decodePos, &position) == VARIABLE_NOT_FOUND)
		{
			return VARIABLE_NOT_FOUND;
		}

		// move down to the end of the name
		decodePos = decodePos + getVariableNameLength(position);

		if (!isAssigned(position))
		{
			return USING_UNASSIGNED_VARIABLE;
		}

		*result = variables[position].value;

		return OPERAND_OK;
	}

	// numeric values can start with a digit or a sign character

	if (isdigit(*decodePos) | (*decodePos == '+') | (*decodePos == '-'))
	{
#ifdef VAR_DEBUG
		Serial.print(F("    Getting literal operand"));
#endif
		if (readInteger(result))
		{
			return OPERAND_OK;
		}
		else
		{
			return NO_DIGITS_IN_VALUE;
		}
	}

	if (*decodePos == READING_START_CHAR)
	{
		// Move past the start character

		decodePos++;

		struct reading * reader = getReading(decodePos);

		if (reader == NULL)
		{
			return INVALID_HARDWARE_READING_NAME;
		}

		decodePos = decodePos + strlen(reader->name);

		*result = reader->reader();

		return OPERAND_OK;
	}

	return INVALID_OPERAND;
}

bool getOperand(int * result)
{
	parseOperandResult getResult = parseOperand(result);

	if (getResult != OPERAND_OK)
	{
		Serial.print(F("Operand error: "));
		Serial.println(getResult);
		return false;
	}
	return true;
}

// decodepos points to the first character of a value sequence
// It is either a literal, variable or two operand expression

bool getValue(int * result)
{
	// Now we are at the start of a value to parse

	int firstOperand;

	if (!getOperand(&firstOperand))
	{
		return false;
	}

	skipCodeSpaces();

	if (*decodePos == STATEMENT_TERMINATOR || *decodePos == ',')
	{
#ifdef VAR_DEBUG
		Serial.println("Single operand");
		Serial.println(firstOperand);
#endif
		*result = firstOperand;
		return true;
	}

	if (!validOperator(*decodePos))
	{
		Serial.println("Invalid operator");
		return false;
	}

	op * activeOperator = findOperator(*decodePos);

	decodePos++;

	int secondOperand;

	if (!getOperand(&secondOperand))
	{
		return false;
	}	

#ifdef VAR_DEBUG
	Serial.print(F("    get operand result"));
	Serial.println(getOperandResult);
#endif

	*result = activeOperator->evaluator(firstOperand, secondOperand);

	return true;
}

// called from the command processor
// the global variable decodePos holds the position in the decode array (first character
// of the variable name) and the global variable decodeLimit the end of the array

//#define TEST_CONDITION_DEBUG

bool testCondition(bool * result)
{

	int firstOperand;

	if (!getOperand(&firstOperand))
	{
		return false;
	}

	logicalOp * op = findLogicalOp(decodePos);

	if (op == NULL)
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("invalid logical operator in logical operation"));
		}
		return false;
	}

#ifdef TEST_CONDITION_DEBUG
	Serial.print(F("    operator: "));
	Serial.println(op->operatorCh);
#endif

	decodePos = decodePos + strlen(op->operatorCh);

	int secondOperand;

	if (!getOperand(&secondOperand))
	{
		return false;
	}

#ifdef TEST_CONDITION_DEBUG
	Serial.print(F("    get operand result"));
	Serial.println(getOperandResult);
	Serial.print(F("    second operand"));
	Serial.println(secondOperand);
#endif

	*result = op->evaluator(firstOperand, secondOperand);

#ifdef TEST_CONDITION_DEBUG
	Serial.print(F("    comparision result"));
	Serial.println(*result);
#endif

	return true;
}

void setVariable()
{

#ifdef VAR_DEBUG
	Serial.println(F("setting variable"));

#endif

	int checkResult = checkIdentifier(decodePos);

	switch (checkResult)
	{
	case INVALID_VARIABLE_NAME:
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("VS invalid variable name"));
		}
		return;

	case VARIABLE_NAME_TOO_LONG:
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.print(F("VS variable name too long"));
		}
		return;
	}

	char * variableStart = decodePos;

	// First see if we can find the variable in the store

	int position;
	int findResult = findVariable(decodePos, &position);

	if (findResult == INVALID_VARIABLE_NAME)
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("VS invalid variable name "));
		}
		return;
	}

	if (findResult == VARIABLE_NOT_FOUND)
	{

#ifdef VAR_DEBUG
		Serial.println(F("Variable not found"));
#endif
		if (createVariable(decodePos, &position) == NO_ROOM_FOR_VARIABLE)
		{
			if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
			{
				Serial.println(F("VS no room for variable"));
			}
			return;
		}
	}
#ifdef VAR_DEBUG
	Serial.println(F("Found variable"));
#endif
	// we have the variable
	// move down to the end of the name

	decodePos = decodePos + getVariableNameLength(position);

	if (*decodePos != '=')
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("VS no equals after variable name"));
		}
		return;
	}

	decodePos++;

	int result;

	if (!getValue(&result))
	{
		return;
	}

	setVariable(position, result);

	if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
	{
		Serial.println(F("VSOK"));
	}
}

void viewVariable()
{
	int position;
	if (findVariable(decodePos, &position) == VARIABLE_NOT_FOUND)
	{
		if (diagnosticsOutputLevel & STATEMENT_CONFIRMATION)
		{
			Serial.println(F("VV variable not found"));
			return;
		}
	}

	if (!isAssigned(position))
	{
		Serial.println(F("Unassigned"));
	}
	else
	{
		Serial.println(getVariable(position));
	}
}
