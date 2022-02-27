/** @file SMS.c
 * See SMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <SMS.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** TODO
 * @return -1 if the SMS with the specified number could not be read.
 */
static int SMSDownloadSingleRecord(TSerialPortID Serial_Port_ID, int SMS_Number, char *Pointer_String_Content, unsigned int Maximum_Content_Length)
{
	static char String_Temporary[2048];
	static unsigned char Temporary_Buffer[2048];
	char String_Command[64], String_Information[64], Character;
	unsigned char *Pointer_Temporary_Buffer, Mask, Leftover_Bits, Byte;
	int Converted_Data_Size, i, Index;

	// Send the command
	sprintf(String_Command, "AT+EMGR=%d", SMS_Number);
	if (ATCommandSendCommand(Serial_Port_ID, String_Command) != 0) return -1;
	printf("\033[31mSMS_Number=%d\033[0m\n", SMS_Number);
	// Wait for the information string
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Information, sizeof(String_Information)) < 0) {printf("ERR\n");return -1;}
	printf("info : %s\n", String_Information);

	// Wait for the message content
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	printf("hex content : %s\n", String_Temporary);

	// Wait for the standard OK
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for empty line before "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for "OK"
	printf("OK : %s\n", String_Command);
	if (strcmp(String_Command, "OK") != 0) return -1;

	// Convert all characters to their binary representation to allow processing them
	Converted_Data_Size = ATCommandConvertHexadecimalToBinary(String_Temporary, Temporary_Buffer, sizeof(Temporary_Buffer));
	if (Converted_Data_Size < 0) return -1;

	{
		FILE *a;

		sprintf(String_Command, "/tmp/%d.hex", SMS_Number);
		a = fopen(String_Command, "w");
		fwrite(Temporary_Buffer, Converted_Data_Size, 1, a);
		fclose(a);
	}

	Pointer_Temporary_Buffer = Temporary_Buffer + 15;
	printf("# characters = %d\ncontent = \n", Temporary_Buffer[14]);
	Index = 7;
	Mask = 0x7F;
	Leftover_Bits = 0;
	for (i = 0; i < Temporary_Buffer[14]; i++) // The amount of uncompressed characters is stored in the first byte
	{
		printf("BYTE %d = %02X, ", i, *Pointer_Temporary_Buffer);
		printf("Index=%d, Mask=0x%02X, Leftover_Bits=0x%02X\n", Index, Mask, Leftover_Bits);

		// Retrieve the most significant bits
		Byte = *Pointer_Temporary_Buffer & Mask;
		// Adjust their position
		Byte <<= 7 - Index;
		// Append the least significant bits from the previous byte
		Byte |= Leftover_Bits;
		// Make sure bit 7 is not set
		Byte &= 0x7F;

		// Keep the next byte least significant bits
		Leftover_Bits = *Pointer_Temporary_Buffer & ~Mask;
		Leftover_Bits >>= Index;

		Pointer_Temporary_Buffer++;
		printf("conv = %c\n", Byte);

		if (Index <= 1)
		{
			printf("conv reset = %c\n", Leftover_Bits & 0x7F);

			Index = 7;
			Mask = 0x7F;
			Leftover_Bits = 0;
		}
		else
		{
			Index--;
			Mask >>= 1;
		}
	}

	/*printf("content : ");
	for (i = 22; i < Converted_Data_Size; i++)
	{
		if (Temporary_Buffer[i] != 0) putchar(Temporary_Buffer[i]);
	}*/
	putchar('\n');

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int SMSDownload(TSerialPortID Serial_Port_ID)
{
	static char test[2048];
	int i;

	for (i = 1; i <= 17; i++) SMSDownloadSingleRecord(Serial_Port_ID, i, test, sizeof(test));

	return 0;
}
