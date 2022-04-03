/** @file SMS.c
 * See SMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <iconv.h>
#include <SMS.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** The size in bytes of the buffer holding a SMS text. */
#define SMS_TEXT_STRING_MAXIMUM_SIZE 512

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

/** Hold the meaningful parts of a SMS record. */
typedef struct
{
	char String_Phone_Number[16]; //<! The International Telecommunication Union (ITU) specified that a phone number can't be longer than 15 digits.
	TSMSMessageStorageLocation Message_Storage_Location;
	int Is_Stored_On_Multiple_Records;
	int Record_ID;
} TSMSRecord;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** Uncompress 7-bit encoded SMS text.
 * @param Pointer_Compressed_Text The compressed text received from the phone.
 * @param Bytes_Count The uncompressed text length.
 * @param Pointer_String_Uncompressed_Text On output, contain the uncompressed text converted to 8-bit ASCII. Make sure the provided string is large enough.
 */
static void SMSUncompress7BitText(unsigned char *Pointer_Compressed_Text, int Bytes_Count, char *Pointer_String_Uncompressed_Text)
{
	int i, Index = 7;
	unsigned char Byte, Mask = 0x7F, Leftover_Bits = 0;
	char *Pointer_String_Uncompressed_Text_Initial_Value = Pointer_String_Uncompressed_Text;
	
	// Go through all compressed bytes, stopping in case the message is wrong and does not include the terminating character
	for (i = 0; i < Bytes_Count; i++)
	{
		// Retrieve the most significant bits
		Byte = *Pointer_Compressed_Text & Mask;
		// Adjust their position
		Byte <<= 7 - Index;
		// Append the least significant bits from the previous byte
		Byte |= Leftover_Bits;
		// Make sure bit 7 is not set
		Byte &= 0x7F;
		// Is the end of the text string reached ?
		if (Byte == 0) break;

		// Keep the next byte least significant bits
		Leftover_Bits = *Pointer_Compressed_Text & ~Mask;
		Leftover_Bits >>= Index;

		// Append the decoded character to the output string
		Pointer_Compressed_Text++;
		*Pointer_String_Uncompressed_Text = (char) Byte;
		Pointer_String_Uncompressed_Text++;

		// 7 bytes encode 8 characters, so reset the state machine when 7 input bytes have been processed
		if (Index <= 1)
		{
			// When 7 input bytes have been processed, a whole character is still contained in the leftover bits, handle it here
			Byte = Leftover_Bits & 0x7F;
			if (Byte == 0) break; // Is the end of the text string reached ?
			*Pointer_String_Uncompressed_Text = Byte;
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
	Pointer_String_Uncompressed_Text_Initial_Value[Bytes_Count] = 0;
}

/** Replace custom character set character values by standard ones and convert the text to UTF-8.
 * @param Pointer_String_Custom_Character_Set_Text The decoded text from the phone. This string will be modified by this function (special characters value will be changed, but string length will remain the same).
 * @param Pointer_String_Converted_Text On output, contain the text converted to UTF-8. Make sure the provided string is large enough.
 */
static void SMSConvert7BitExtendedASCII(char *Pointer_String_Custom_Character_Set_Text, char *Pointer_String_Converted_Text)
{
	static unsigned char Extended_ASCII_Conversion_Table[256] = // Look-up table that converts custom CAT extended characters to Windows CP1252 extended characters
	{
		0x00, 0x00, 0x00, 0x00, 0xE8, 0xE9, 0xF9, 0x00, 0x00, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xE6, 0x00, 0xC9,
		 ' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'',  '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
		 '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
		 '@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
		 'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z',  '[', '\\',  ']',  '^',  '_',
		 '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  '{',  '|',  '}',  '~', 0xE0
	};
	unsigned char Byte;
	char *Pointer_String_Temporary;
	iconv_t Conversion_Descriptor;
	size_t Source_String_Remaining_Bytes, Destination_String_Available_Bytes;

	// Process each character through the look-up table
	Pointer_String_Temporary = Pointer_String_Custom_Character_Set_Text;
	while (*Pointer_String_Temporary != 0)
	{
		Byte = *Pointer_String_Temporary;
		*Pointer_String_Temporary = Extended_ASCII_Conversion_Table[Byte]; // Use a non-signed variable as array index to avoid a compiler warning
		Pointer_String_Temporary++;
	}

	// Convert text to UTF-8
	// Configure character sets
	Conversion_Descriptor = iconv_open("UTF-8", "WINDOWS-1252");
	if (Conversion_Descriptor == (iconv_t) -1)
	{
		printf("Error : failed to create the character set conversion descriptor, skipping text conversion to UTF-8.\n");
		return;
	}

	// Do conversion
	Source_String_Remaining_Bytes = strlen(Pointer_String_Custom_Character_Set_Text);
	Destination_String_Available_Bytes = SMS_TEXT_STRING_MAXIMUM_SIZE;
	if (iconv(Conversion_Descriptor, &Pointer_String_Custom_Character_Set_Text, &Source_String_Remaining_Bytes, &Pointer_String_Converted_Text, &Destination_String_Available_Bytes) == (size_t) -1) printf("Error : text conversion to UTF-8 failed.\n");
	iconv_close(Conversion_Descriptor);
}

/** Convert an UTF-16 big endian text to UTF-8.
 * @param Pointer_Text The raw text to decode.
 * @param Bytes_Count The raw text size in bytes.
 * @param Pointer_String_Decoded_Text On output, contain the text converted to UTF-8.
 */
static void SMSDecode16BitText(unsigned char *Pointer_Text, int Bytes_Count, char *Pointer_String_Decoded_Text)
{
	iconv_t Conversion_Descriptor;
	size_t Source_String_Remaining_Bytes, Destination_String_Available_Bytes;

	// Convert it to UTF-8
	// Configure character sets
	Conversion_Descriptor = iconv_open("UTF-8", "UTF-16BE");
	if (Conversion_Descriptor == (iconv_t) -1)
	{
		printf("Error : failed to create the character set conversion descriptor, skipping text conversion to UTF-8.\n");
		return;
	}

	// Do conversion
	Source_String_Remaining_Bytes = Bytes_Count;
	Destination_String_Available_Bytes = SMS_TEXT_STRING_MAXIMUM_SIZE;
	if (iconv(Conversion_Descriptor, (char **) &Pointer_Text, &Source_String_Remaining_Bytes, &Pointer_String_Decoded_Text, &Destination_String_Available_Bytes) == (size_t) -1) printf("Error : text conversion to UTF-8 failed.\n");
	iconv_close(Conversion_Descriptor);
}

/** TODO
 * @param Pointer_Message_Buffer The raw message, it must be of type SMS_MESSAGE_STORAGE_LOCATION_SENT or SMS_MESSAGE_STORAGE_LOCATION_DRAFT.
 * @param Pointer_SMS_Record On output, fill the phone number if it is present in the message (if no phone number is present the string will be set to empty). Also fill the multiple records boolean and record ID if any.
 * @return A positive number indicating the offset of the text payload in the message buffer.
 */
static int SMSDecodeShortHeaderMessage(unsigned char *Pointer_Message_Buffer, TSMSRecord *Pointer_SMS_Record, int *Pointer_Is_Wide_Character_Encoding, int *Pointer_Text_Bytes_Count)
{
	int Phone_Number_Length, Is_Least_Significant_Nibble_Selected = 1, i, Text_Payload_Offset = 0;
	char Digit, *Pointer_String_Phone_Number;
	unsigned char Byte;

	// Bypass currently unknown bytes
	Pointer_Message_Buffer += 2;
	Text_Payload_Offset += 2;

	// Is this message stored on multiple records ?
	Byte = *Pointer_Message_Buffer & 0xF0;
	if (Byte == 0x90) Pointer_SMS_Record->Is_Stored_On_Multiple_Records = 0;
	else if (Byte == 0xD0) Pointer_SMS_Record->Is_Stored_On_Multiple_Records = 1;
	else
	{
		printf("Error : unknown multiple record indicator in short header message.\n");
		return -1;
	}
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Bypass currently unknown byte
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Retrieve phone number
	Pointer_String_Phone_Number = Pointer_SMS_Record->String_Phone_Number;
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
	if (Phone_Number_Length & 1) // Take into account the one more byte if the phone number length is odd
	{
		Pointer_Message_Buffer++;
		Text_Payload_Offset++;
	}

	// Terminate the string
	*Pointer_String_Phone_Number = 0;

	// Bypass unknown byte
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Determine whether the text is 7-bit ASCII encoded or UTF-16 encoded
	if (*Pointer_Message_Buffer & 0x08) *Pointer_Is_Wide_Character_Encoding = 1;
	else *Pointer_Is_Wide_Character_Encoding = 0;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Bypass unknown byte
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Retrieve the text size in bytes
	*Pointer_Text_Bytes_Count = *Pointer_Message_Buffer;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Retrieve record ID if any
	if (Pointer_SMS_Record->Is_Stored_On_Multiple_Records)
	{
		// Bypass the initial 0x05 byte, which might represent the record ID field size in bytes, and the following unknown byte
		Pointer_Message_Buffer += 2;
		Text_Payload_Offset += 2;

		// Consider all 4 bytes as the ID because they can fit in the provided "int" variable
		Pointer_SMS_Record->Record_ID = (Pointer_Message_Buffer[0] << 24) | (Pointer_Message_Buffer[1] << 16) | (Pointer_Message_Buffer[2] << 8) | Pointer_Message_Buffer[3];
		Text_Payload_Offset += 4;
	}

	return Text_Payload_Offset;
}

/** TODO
 * @return -2 if the provided content string buffer is too small,
 * @return -1 if the SMS with the specified number content could not be read,
 */
static int SMSDownloadSingleRecord(TSerialPortID Serial_Port_ID, int SMS_Number, TSMSRecord *Pointer_SMS_Record, char *Pointer_String_Text)
{
	static char String_Temporary[2048];
	static unsigned char Temporary_Buffer[2048];
	char String_Command[64];
	int Converted_Data_Size, Text_Payload_Offset, Is_Wide_Character_Encoding, Text_Payload_Bytes_Count;
	TSMSMessageStorageLocation Message_Storage_Location;

	// Send the command
	sprintf(String_Command, "AT+EMGR=%d", SMS_Number);
	if (ATCommandSendCommand(Serial_Port_ID, String_Command) != 0) return -1;

	// Wait for the information string
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	printf("info : %s\n", String_Temporary);
	if (sscanf(String_Temporary, "+EMGR: %d", (int *) &Message_Storage_Location) != 1) return -1; // Extract message storage location information
	Pointer_SMS_Record->Message_Storage_Location = Message_Storage_Location;

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
		// Retrieve all useful information from the message header
		Text_Payload_Offset = SMSDecodeShortHeaderMessage(Temporary_Buffer, Pointer_SMS_Record, &Is_Wide_Character_Encoding, &Text_Payload_Bytes_Count);
		if (Text_Payload_Offset < 0) return -1;
		printf("Text_Payload_Bytes_Count=%d, Is_Wide_Character_Encoding=%d\n", Text_Payload_Bytes_Count, Is_Wide_Character_Encoding);

		// Decode text
		if (Is_Wide_Character_Encoding) SMSDecode16BitText(&Temporary_Buffer[Text_Payload_Offset], Text_Payload_Bytes_Count, Pointer_String_Text);
		else
		{
			// Extract the text content with the custom character set for extended ASCII
			SMSUncompress7BitText(&Temporary_Buffer[Text_Payload_Offset], Text_Payload_Bytes_Count, String_Temporary);

			// Convert custom character set to UTF-8
			SMSConvert7BitExtendedASCII(String_Temporary, Pointer_String_Text);
		}
	}
	else return -2; // Unsupported message format (for now)

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int SMSDownload(TSerialPortID Serial_Port_ID)
{
	char String_Text[SMS_TEXT_STRING_MAXIMUM_SIZE];
	int i;
	TSMSRecord SMS_Record;

	for (i = 1; i <= 100; i++)
	{
		printf("\033[32mSMS number = %d\033[0m\n", i);
		memset(String_Text, 0, sizeof(String_Text));
		if (SMSDownloadSingleRecord(Serial_Port_ID, i, &SMS_Record, String_Text) == 0)
		{
			printf("Message %d : phone number = %s, text = \"%s\" (len = %lu), message storage location = %d, is stored on multiple records = %d", i, SMS_Record.String_Phone_Number, String_Text, strlen(String_Text), SMS_Record.Message_Storage_Location, SMS_Record.Is_Stored_On_Multiple_Records);
			if (SMS_Record.Is_Stored_On_Multiple_Records) printf(", record ID = 0x%08X", SMS_Record.Record_ID);
		}
		else printf("\033[31mUnsupported\033[0m");
		printf("\n\n");
	}

	return 0;
}
