/** @file AT_Command.c
 * See AT_Command.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** TODO */
int ATCommandGetHexadecimalNibbleBinaryValue(char Hexadecimal_Nibble)
{
	// Convert any hexadecimal letters to uppercase
	if ((Hexadecimal_Nibble >= 'a') && (Hexadecimal_Nibble <= 'f')) Hexadecimal_Nibble -= 32;

	// Convert digits
	if ((Hexadecimal_Nibble >= '0') && (Hexadecimal_Nibble <= '9')) return Hexadecimal_Nibble - '0';

	// Convert letters
	if ((Hexadecimal_Nibble >= 'A') || (Hexadecimal_Nibble <= 'F')) return Hexadecimal_Nibble - 'A' + 10;

	// This is an invalid hexadecimal character
	return -1;
}

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

int ATCommandConvertHexadecimalToBinary(char *Pointer_String_Hexadecimal, unsigned char *Pointer_Output_Buffer, unsigned int Output_Buffer_Size)
{
	unsigned char Byte;
	unsigned int Converted_Data_Size = 0;

	// Make sure the string size is a multiple of 2, so all hexadecimal bytes are complete
	if ((strlen(Pointer_String_Hexadecimal) % 2) != 0) return -1;

	// Convert all characters to their binary representation
	while ((*Pointer_String_Hexadecimal != 0) && (Converted_Data_Size < Output_Buffer_Size))
	{
		// Convert both hexadecimal nibbles to form a complete byte
		Byte = (ATCommandGetHexadecimalNibbleBinaryValue(*Pointer_String_Hexadecimal) << 4) & 0xF0;
		Pointer_String_Hexadecimal++;
		Byte |= ATCommandGetHexadecimalNibbleBinaryValue(*Pointer_String_Hexadecimal) & 0x0F;
		Pointer_String_Hexadecimal++;

		// Store the byte to the destination buffer
		*Pointer_Output_Buffer = Byte;
		Pointer_Output_Buffer++;
		Converted_Data_Size++;
	}

	// Stop if there no more room in the output buffer
	if (Converted_Data_Size >= Output_Buffer_Size) return -1;

	return (int) Converted_Data_Size;
}
