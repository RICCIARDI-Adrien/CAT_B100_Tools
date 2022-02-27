/** @file AT_Command.c
 * See AT_Command.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int ATCommandReceiveAnswerLine(TSerialPortID Serial_Port_ID, char *Pointer_String_Answer, unsigned int Maximum_Length)
{
	unsigned int i;
	char Character;

	// Make sure there is at least the room to store one character followed by the terminating zero.
	if (Maximum_Length <= 2) return -1;

	// Read as much characters as allowed
	Maximum_Length--; // Keep one byte for the terminating zero
	for (i = 0; i < Maximum_Length; i++)
	{
		// Wait for next byte to be received
		Character = SerialPortReadByte(Serial_Port_ID);

		// Is the end of the string reached ?
		if ((Character == '\n') && (i > 0) && (Pointer_String_Answer[i - 1] == '\r'))
		{
			// Terminate the string
			Pointer_String_Answer[i - 1] = 0;

			// Is this the standard error string ?
			if ((Maximum_Length >= 6) && (strcmp(Pointer_String_Answer, "ERROR") == 0)) return -2;

			// This is a regular string
			return 0;
		}

		// This is a regular character, append it to the string
		Pointer_String_Answer[i] = Character;
	}

	// There was not enough room in the string buffer
	return -3;
}

int ATCommandSendCommand(TSerialPortID Serial_Port_ID, char *Pointer_String_Command)
{
	unsigned int Length;

	// Send the command
	Length = (unsigned int) strlen(Pointer_String_Command);
	SerialPortWriteBuffer(Serial_Port_ID, Pointer_String_Command, Length);

	// Send the terminating character (this is not the CRLF terminating sequence here, only CR character is sent)
	SerialPortWriteByte(Serial_Port_ID, '\r');

	// Discard the command echoing
	while (SerialPortReadByte(Serial_Port_ID) != '\n') ;

	return 0;
}
