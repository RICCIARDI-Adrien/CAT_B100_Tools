/** @file SMS.c
 * See SMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <SMS.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Private types
//-------------------------------------------------------------------------------------------------
/** All available message storage location. */
typedef enum
{
	SMS_MESSAGE_STORAGE_LOCATION_INBOX = 1,
	SMS_MESSAGE_STORAGE_LOCATION_SENT = 3,
	SMS_MESSAGE_STORAGE_LOCATION_DRAFT = 7
} TSMSMessageStorageLocation;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** Uncompress 7-bit encoded SMS text.
 * @param Pointer_Compressed_Text The compressed text received from the phone.
 * @param Compressed_Bytes_Count The compressed text length.
 * @param Pointer_String_Uncompressed_Text On output, contain the uncompressed text converted to 8-bit ASCII. Make sure the provided string is large enough.
 */
static void SMSDecode7BitText(unsigned char *Pointer_Compressed_Text, int Compressed_Bytes_Count, char *Pointer_String_Uncompressed_Text)
{
	int i, Index = 7;
	unsigned char Byte, Mask = 0x7F, Leftover_Bits = 0;

	// Go through all compressed bytes
	for (i = 0; i < Compressed_Bytes_Count; i++)
	{
		// Retrieve the most significant bits
		Byte = *Pointer_Compressed_Text & Mask;
		// Adjust their position
		Byte <<= 7 - Index;
		// Append the least significant bits from the previous byte
		Byte |= Leftover_Bits;
		// Make sure bit 7 is not set
		Byte &= 0x7F;

		// Keep the next byte least significant bits
		Leftover_Bits = *Pointer_Compressed_Text & ~Mask;
		Leftover_Bits >>= Index;

		// Append the decoded character to the output string
		Pointer_Compressed_Text++;
		*Pointer_String_Uncompressed_Text = (char) Byte;
		Pointer_String_Uncompressed_Text++;

		// 7 bytes encode 8 characters, si reset the state machine when 7 input bytes have been processed
		if (Index <= 1)
		{
			// When 7 input bytes have been processed, a whole character is still contained in the leftover bits, handle it here
			*Pointer_String_Uncompressed_Text = Leftover_Bits & 0x7F;
			Pointer_String_Uncompressed_Text++;

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

	// Terminate the output string
	*Pointer_String_Uncompressed_Text = 0;
}

/** TODO
 * @param Pointer_Message_Buffer The raw message, it must be of type SMS_MESSAGE_STORAGE_LOCATION_SENT or SMS_MESSAGE_STORAGE_LOCATION_DRAFT.
 * @param Pointer_String_Phone_Number On output, contain the phone number if it is present in the message. Otherwise, the string will be set to empty. Make sure to provide a string with enough room for the data that will be stored inside.
 * @return A positive number indicating the offset of the text payload in the message buffer.
 */
static int SMSDecodeShortHeaderMessage(unsigned char *Pointer_Message_Buffer, char *Pointer_String_Phone_Number)
{
	int Phone_Number_Length, Is_Least_Significant_Nibble_Selected = 1, i, Text_Payload_Offset = 0;
	char Digit;

	// Bypass currently unknown bytes
	Pointer_Message_Buffer += 4;
	Text_Payload_Offset += 4;

	// Retrieve phone number
	Phone_Number_Length = *Pointer_Message_Buffer;
	Pointer_Message_Buffer += 2; // Bypass an unknown byte following the phone number length
	Text_Payload_Offset += 2;

	// Extract the phone number (each digit is stored in a byte nibble)
	for (i = 0; i < Phone_Number_Length; i++)
	{
		// Extract the correct digit from the byte
		if (Is_Least_Significant_Nibble_Selected) Digit = *Pointer_Message_Buffer & 0x0F;
		else Digit = *Pointer_Message_Buffer >> 4;

		// Add the digit to the string
		Digit += '0'; // Convert the digit to ASCII
		*Pointer_String_Phone_Number = Digit;
		Pointer_String_Phone_Number++;

		// Select the next nibble to extract
		if (Is_Least_Significant_Nibble_Selected) Is_Least_Significant_Nibble_Selected = 0;
		else
		{
			Is_Least_Significant_Nibble_Selected = 1;
			Pointer_Message_Buffer++; // Both digits have been extracted from the byte, go to next byte
		}
	}
	// Adjust read bytes count
	Text_Payload_Offset += Phone_Number_Length / 2;
	if (Phone_Number_Length & 1) Text_Payload_Offset++; // Take into account the one more byte if the phone number length is odd

	// Terminate the string
	*Pointer_String_Phone_Number = 0;

	// Bypass unknown bytes
	Text_Payload_Offset += 3;

	return Text_Payload_Offset;
}

/** TODO
 * @return -2 if the provided content string buffer is too small,
 * @return -1 if the SMS with the specified number content could not be read,
 */
static int SMSDownloadSingleRecord(TSerialPortID Serial_Port_ID, int SMS_Number, char *Pointer_String_Phone_Number, char *Pointer_String_Text, TSMSMessageStorageLocation *Pointer_Message_Storage_Location)
{
	static char String_Temporary[2048];
	static unsigned char Temporary_Buffer[2048];
	char String_Command[64];
	int Converted_Data_Size, Text_Payload_Offset;
	TSMSMessageStorageLocation Message_Storage_Location;

	// Send the command
	sprintf(String_Command, "AT+EMGR=%d", SMS_Number);
	if (ATCommandSendCommand(Serial_Port_ID, String_Command) != 0) return -1;

	// Wait for the information string
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	printf("info : %s\n", String_Temporary);
	if (sscanf(String_Temporary, "+EMGR: %d", (int *) &Message_Storage_Location) != 1) return -1; // Extract message storage location information
	*Pointer_Message_Storage_Location = Message_Storage_Location;
	printf("Message_Storage_Location=%d\n", Message_Storage_Location);

	// Wait for the message content
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	printf("hex content : %s\n", String_Temporary);

	// Wait for the standard OK
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for empty line before "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for "OK"
	if (strcmp(String_Command, "OK") != 0) return -1;

	// Convert all characters to their binary representation to allow processing them
	Converted_Data_Size = ATCommandConvertHexadecimalToBinary(String_Temporary, Temporary_Buffer, sizeof(Temporary_Buffer));
	if (Converted_Data_Size < 0) return -1;

	// Sent and draft messages share the same header format
	if ((Message_Storage_Location == SMS_MESSAGE_STORAGE_LOCATION_SENT) || (Message_Storage_Location == SMS_MESSAGE_STORAGE_LOCATION_DRAFT))
	{
		Text_Payload_Offset = SMSDecodeShortHeaderMessage(Temporary_Buffer, Pointer_String_Phone_Number);
		SMSDecode7BitText(&Temporary_Buffer[Text_Payload_Offset + 1], Temporary_Buffer[Text_Payload_Offset], Pointer_String_Text);
	}
	else return -2; // Unsupported message format (for now)

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int SMSDownload(TSerialPortID Serial_Port_ID)
{
	char String_Phone_Number[32], String_Text[512];
	int i;
	TSMSMessageStorageLocation Message_Storage_Location;

	for (i = 1; i <= 30; i++)
	{
		printf("\033[32mSMS number = %d\033[0m\n", i);
		if (SMSDownloadSingleRecord(Serial_Port_ID, i, String_Phone_Number, String_Text, &Message_Storage_Location) == 0) printf("Message %d phone number = %s, text = \"%s\", message location = %d\n\n", i, String_Phone_Number, String_Text, Message_Storage_Location);
		else printf("\033[31mUnsupported\033[0m\n\n");
	}

	return 0;
}
