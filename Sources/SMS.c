/** @file SMS.c
 * See SMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <errno.h>
#include <SMS.h>
#include <stdio.h>
#include <string.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** The size in bytes of the buffer holding a SMS text. */
#define SMS_TEXT_STRING_MAXIMUM_SIZE 512

/** How many records to read from the phone. */
#define SMS_RECORDS_MAXIMUM_COUNT 300

//-------------------------------------------------------------------------------------------------
// Private types
//-------------------------------------------------------------------------------------------------
/** All available message storage location. */
typedef enum
{
	SMS_STORAGE_LOCATION_INBOX = 1,
	SMS_STORAGE_LOCATION_SENT = 3,
	SMS_STORAGE_LOCATION_DRAFT = 7
} TSMSStorageLocation;

/** Hold the meaningful parts of a SMS record. */
typedef struct
{
	int Is_Data_Present; //!< Tell whether this record holds valid data or if it is empty.
	char String_Phone_Number[16]; //!< The International Telecommunication Union (ITU) specified that a phone number can't be longer than 15 digits.
	char String_Text[SMS_TEXT_STRING_MAXIMUM_SIZE]; //!< Contain the decoded message text.
	TSMSStorageLocation Message_Storage_Location;
	int Record_ID; //!< The record unique identifier (it is made of the two first bytes of the record field).
	int Records_Count; //!< The amount of records needed to store the whole message.
	int Record_Number; //!< The number of this record among all needed records (starts from one).
	int Date_Year;
	int Date_Month;
	int Date_Day;
	int Time_Hour;
	int Time_Minutes;
	int Time_Seconds;
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
		0x00, 0x00, 0x00, 0x00, 0xE8, 0xE9, 0xF9, 0x00, 0x00, 0xC7, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00,
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

	// Process each character through the look-up table
	Pointer_String_Temporary = Pointer_String_Custom_Character_Set_Text;
	while (*Pointer_String_Temporary != 0)
	{
		Byte = *Pointer_String_Temporary;
		*Pointer_String_Temporary = Extended_ASCII_Conversion_Table[Byte]; // Use a non-signed variable as array index to avoid a compiler warning
		Pointer_String_Temporary++;
	}

	// Convert text to UTF-8
	UtilityConvertString(Pointer_String_Custom_Character_Set_Text, Pointer_String_Converted_Text, UTILITY_CHARACTER_SET_WINDOWS_1252, UTILITY_CHARACTER_SET_UTF8, 0, SMS_TEXT_STRING_MAXIMUM_SIZE);
}

/** TODO
 * @param Pointer_Message_Buffer The raw message, it must be of type SMS_STORAGE_LOCATION_SENT or SMS_STORAGE_LOCATION_DRAFT.
 * @param Pointer_SMS_Record On output, fill most of the information of the decoded SMS record.
 * @return A positive number indicating the offset of the text payload in the message buffer.
 */
static int SMSDecodeShortHeaderMessage(unsigned char *Pointer_Message_Buffer, TSMSRecord *Pointer_SMS_Record, int *Pointer_Is_Wide_Character_Encoding, int *Pointer_Text_Bytes_Count)
{
	int Phone_Number_Length, Is_Least_Significant_Nibble_Selected = 1, i, Text_Payload_Offset = 0, Is_Stored_On_Multiple_Records;
	char Digit, *Pointer_String_Phone_Number;
	unsigned char Byte;

	// Bypass currently unknown bytes
	Pointer_Message_Buffer += 2;
	Text_Payload_Offset += 2;

	// Is this message stored on multiple records ?
	Byte = *Pointer_Message_Buffer & 0xF0;
	if (Byte == 0x90) Is_Stored_On_Multiple_Records = 0;
	else if (Byte == 0xD0) Is_Stored_On_Multiple_Records = 1;
	else
	{
		printf("Error : unknown multiple record indicators in short header message.\n");
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

	// Is the phone number provided ?
	if (Phone_Number_Length > 0)
	{
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
	}
	// No phone number was provided, make sure a string is present to tell that
	else strcpy(Pointer_String_Phone_Number, "<Unspecified>");

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
	if (Is_Stored_On_Multiple_Records)
	{
		// Bypass the initial 0x05 byte, which might represent the record ID field size in bytes, and the following unknown byte
		Pointer_Message_Buffer += 2;
		Text_Payload_Offset += 2;

		// Decode the record 4 bytes
		Pointer_SMS_Record->Record_ID = (Pointer_Message_Buffer[0] << 8) | Pointer_Message_Buffer[1];
		Pointer_SMS_Record->Records_Count = Pointer_Message_Buffer[2];
		Pointer_SMS_Record->Record_Number = Pointer_Message_Buffer[3];
		Pointer_Message_Buffer += 4;
		Text_Payload_Offset += 4;
	}
	// Make sure values are coherent if there is only a single record (in this case there is not record 4 bytes field
	else
	{
		Pointer_SMS_Record->Records_Count = 1;
		Pointer_SMS_Record->Record_Number = 1; // Record numbers start from 1
	}

	return Text_Payload_Offset;
}

/** TODO
 * @param Pointer_Message_Buffer The raw message, it must be of type SMS_STORAGE_LOCATION_INBOX.
 * @param Pointer_SMS_Record On output, fill most of the information of the decoded SMS record.
 * @return A positive number indicating the offset of the text payload in the message buffer.
 */
static int SMSDecodeLongHeaderMessage(unsigned char *Pointer_Message_Buffer, TSMSRecord *Pointer_SMS_Record, int *Pointer_Is_Wide_Character_Encoding, int *Pointer_Text_Bytes_Count)
{
	int Phone_Number_Length, Is_Least_Significant_Nibble_Selected = 1, i, Text_Payload_Offset = 0, Is_Stored_On_Multiple_Records;
	char Digit, *Pointer_String_Phone_Number;
	unsigned char Byte;

	// Bypass currently unknown bytes
	Pointer_Message_Buffer += 8;
	Text_Payload_Offset += 8;

	// Is this message stored on multiple records ?
	Byte = *Pointer_Message_Buffer;
	if (Byte & 0x40) Is_Stored_On_Multiple_Records = 1;
	else Is_Stored_On_Multiple_Records = 0;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Retrieve phone number
	Pointer_String_Phone_Number = Pointer_SMS_Record->String_Phone_Number;
	Phone_Number_Length = *Pointer_Message_Buffer;
	Pointer_Message_Buffer += 2; // Bypass an unknown byte following the phone number length
	Text_Payload_Offset += 2;

	// Is the phone number provided ?
	if (Phone_Number_Length > 0)
	{
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
	}
	// No phone number was provided, make sure a string is present to tell that
	else strcpy(Pointer_String_Phone_Number, "<Unspecified>");

	// Bypass unknown byte
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Determine whether the text is 7-bit ASCII encoded or UTF-16 encoded
	if (*Pointer_Message_Buffer & 0x08) *Pointer_Is_Wide_Character_Encoding = 1;
	else *Pointer_Is_Wide_Character_Encoding = 0;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Extract message reception date and time
	// Year
	Byte = Pointer_Message_Buffer[0];
	Pointer_SMS_Record->Date_Year = 2000 + ((Byte & 0x0F) * 10) + (Byte >> 4);
	// Month
	Byte = Pointer_Message_Buffer[1];
	Pointer_SMS_Record->Date_Month = ((Byte & 0x0F) * 10) + (Byte >> 4);
	// Day
	Byte = Pointer_Message_Buffer[2];
	Pointer_SMS_Record->Date_Day = ((Byte & 0x0F) * 10) + (Byte >> 4);
	// Hour
	Byte = Pointer_Message_Buffer[3];
	Pointer_SMS_Record->Time_Hour = ((Byte & 0x0F) * 10) + (Byte >> 4);
	// Minutes
	Byte = Pointer_Message_Buffer[4];
	Pointer_SMS_Record->Time_Minutes = ((Byte & 0x0F) * 10) + (Byte >> 4);
	// Seconds
	Byte = Pointer_Message_Buffer[5];
	Pointer_SMS_Record->Time_Seconds = ((Byte & 0x0F) * 10) + (Byte >> 4);
	Pointer_Message_Buffer += 6;
	Text_Payload_Offset += 6;

	// Bypass currently unknown byte
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Retrieve the text size in bytes
	*Pointer_Text_Bytes_Count = *Pointer_Message_Buffer;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Retrieve record ID if any
	if (Is_Stored_On_Multiple_Records)
	{
		// Bypass the initial 0x05 byte, which might represent the record ID field size in bytes, and the following unknown byte
		Pointer_Message_Buffer += 2;
		Text_Payload_Offset += 2;

		// Decode the record 4 bytes
		Pointer_SMS_Record->Record_ID = (Pointer_Message_Buffer[0] << 8) | Pointer_Message_Buffer[1];
		Pointer_SMS_Record->Records_Count = Pointer_Message_Buffer[2];
		Pointer_SMS_Record->Record_Number = Pointer_Message_Buffer[3];
		Pointer_Message_Buffer += 4;
		Text_Payload_Offset += 4;
	}
	// Make sure values are coherent if there is only a single record (in this case there is not record 4 bytes field
	else
	{
		Pointer_SMS_Record->Records_Count = 1;
		Pointer_SMS_Record->Record_Number = 1; // Record numbers start from 1
	}

	return Text_Payload_Offset;
}

/** TODO
 * @return -3 if the record is empty,
 * @return -2 if the SMS format is not supported yet,
 * @return -1 if an unrecoverable error occurred,
 * @return 0 on success.
 */
static int SMSDownloadSingleRecord(TSerialPortID Serial_Port_ID, int SMS_Number, TSMSRecord *Pointer_SMS_Record)
{
	static char String_Temporary[2048];
	static unsigned char Temporary_Buffer[2048];
	char String_Command[64];
	int Converted_Data_Size, Text_Payload_Offset, Is_Wide_Character_Encoding, Text_Payload_Bytes_Count;
	TSMSStorageLocation Message_Storage_Location;

	// Send the command
	sprintf(String_Command, "AT+EMGR=%d", SMS_Number);
	if (ATCommandSendCommand(Serial_Port_ID, String_Command) != 0)
	{
		printf("Error : failed to send the AT command to read SMS record %d.\n", SMS_Number);
		return -1;
	}

	// Wait for the information string
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	printf("info : %s\n", String_Temporary);
	if (strcmp(String_Temporary, "+CMS ERROR: 321") == 0) return -3; // The message storage location is empty
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
	if ((Message_Storage_Location == SMS_STORAGE_LOCATION_SENT) || (Message_Storage_Location == SMS_STORAGE_LOCATION_DRAFT))
	{
		// Retrieve all useful information from the message header
		Text_Payload_Offset = SMSDecodeShortHeaderMessage(Temporary_Buffer, Pointer_SMS_Record, &Is_Wide_Character_Encoding, &Text_Payload_Bytes_Count);
		if (Text_Payload_Offset < 0) return -1;
	}
	// Inbox messages use a longer header
	else if (Message_Storage_Location == SMS_STORAGE_LOCATION_INBOX)
	{
		// Retrieve all useful information from the message header
		Text_Payload_Offset = SMSDecodeLongHeaderMessage(Temporary_Buffer, Pointer_SMS_Record, &Is_Wide_Character_Encoding, &Text_Payload_Bytes_Count);
		if (Text_Payload_Offset < 0) return -1;
	}
	else
	{
		printf("Error : unknown SMS message storage location %d.\n", Message_Storage_Location);
		return -1;
	}

	// Decode text
	if (Is_Wide_Character_Encoding)
	{
		// Quick fix for multi records messages text string trailing characters (TODO find a better solution)
		if ((Pointer_SMS_Record->Records_Count > 1) && (Text_Payload_Bytes_Count > 6)) Text_Payload_Bytes_Count -= 6;

		// Convert UTF-16 to UTF-8
		if (UtilityConvertString(&Temporary_Buffer[Text_Payload_Offset], Pointer_SMS_Record->String_Text, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Text_Payload_Bytes_Count, sizeof(Pointer_SMS_Record->String_Text)) < 0) return -1;
	}
	else
	{
		// Extract the text content with the custom character set for extended ASCII
		SMSUncompress7BitText(&Temporary_Buffer[Text_Payload_Offset], Text_Payload_Bytes_Count, String_Temporary);

		// Convert custom character set to UTF-8
		SMSConvert7BitExtendedASCII(String_Temporary, Pointer_SMS_Record->String_Text);
	}

	// Tell that record contains data
	Pointer_SMS_Record->Is_Data_Present = 1;

	return 0;
}

/** Write the appropriate message header to the output file according to the message storage location.
 * @param Pointer_Output_File The output file to write to.
 * @param Pointer_SMS_Record The message information.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
static int SMSWriteOutputMessageInformation(FILE *Pointer_Output_File, TSMSRecord *Pointer_SMS_Record)
{
	char String_Temporary[256], String_Temporary_2[32];

	// Write phone number
	if (Pointer_SMS_Record->Message_Storage_Location == SMS_STORAGE_LOCATION_INBOX) strcpy(String_Temporary, "From : ");
	else strcpy(String_Temporary, "To : ");
	strcat(String_Temporary, Pointer_SMS_Record->String_Phone_Number);
	strcat(String_Temporary, "\n");

	// Write date if any
	if (Pointer_SMS_Record->Message_Storage_Location == SMS_STORAGE_LOCATION_INBOX)
	{
		sprintf(String_Temporary_2, "Date : %04d-%02d-%02d %02d:%02d:%02d\n", Pointer_SMS_Record->Date_Year, Pointer_SMS_Record->Date_Month, Pointer_SMS_Record->Date_Day, Pointer_SMS_Record->Time_Hour, Pointer_SMS_Record->Time_Minutes, Pointer_SMS_Record->Time_Seconds);
		strcat(String_Temporary, String_Temporary_2);
	}

	// Message text
	strcat(String_Temporary, "Text : ");

	// Try to write to file
	if (fprintf(Pointer_Output_File, "%s", String_Temporary) <= 0)
	{
		printf("Error : could not write to SMS output file (%s).\n", strerror(errno));
		return -1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int SMSDownloadAll(TSerialPortID Serial_Port_ID)
{
	static TSMSRecord SMS_Records[SMS_RECORDS_MAXIMUM_COUNT];
	int i, Return_Value = -1, j, Current_Record_Number;
	FILE *Pointer_File_Inbox = NULL, *Pointer_File_Sent = NULL, *Pointer_File_Draft = NULL, *Pointer_File;
	TSMSRecord *Pointer_SMS_Record, *Pointer_Searched_SMS_Record;

	// Read all possible records
	printf("Reading all SMS records...\n");
	for (i = 1; i <= SMS_RECORDS_MAXIMUM_COUNT; i++)
	{
		printf("\033[32mSMS record number = %d\033[0m\n", i); // TEST
		if (SMSDownloadSingleRecord(Serial_Port_ID, i, &SMS_Records[i - 1]) == 0) // Record array is zero-based
		{
			// TEST
			printf("Phone number : %s\n", SMS_Records[i - 1].String_Phone_Number);
			printf("Message storage location : ");
			switch (SMS_Records[i - 1].Message_Storage_Location)
			{
				case SMS_STORAGE_LOCATION_DRAFT:
					printf("draft\n");
					break;
				case SMS_STORAGE_LOCATION_INBOX:
					printf("inbox\n");
					break;
				case SMS_STORAGE_LOCATION_SENT:
					printf("sent\n");
					break;
			}
			printf("Message content : %s\n", SMS_Records[i - 1].String_Text);
		}
		printf("\n");
	}

	// Create output directories
	if (UtilityCreateDirectory("Output/SMS") != 0) goto Exit;

	// Create all needed files
	// Inbox
	Pointer_File_Inbox = fopen("Output/SMS/Inbox.txt", "w");
	if (Pointer_File_Inbox == NULL)
	{
		printf("Error : could not create the SMS \"Inbox.txt\" file (%s).\n", strerror(errno));
		goto Exit;
	}
	// Sent
	Pointer_File_Sent = fopen("Output/SMS/Sent.txt", "w");
	if (Pointer_File_Sent == NULL)
	{
		printf("Error : could not create the SMS \"Sent.txt\" file (%s).\n", strerror(errno));
		goto Exit;
	}
	// Draft
	Pointer_File_Draft = fopen("Output/SMS/Draft.txt", "w");
	if (Pointer_File_Draft == NULL)
	{
		printf("Error : could not create the SMS \"Draft.txt\" file (%s).\n", strerror(errno));
		goto Exit;
	}

	// Store all records to the appropriate files
	for (i = 0; i < SMS_RECORDS_MAXIMUM_COUNT; i++)
	{
		// Cache record access
		Pointer_SMS_Record = &SMS_Records[i];

		// Is the record empty ?
		if (!Pointer_SMS_Record->Is_Data_Present) continue;

		// Select the correct output file
		switch (Pointer_SMS_Record->Message_Storage_Location)
		{
			case SMS_STORAGE_LOCATION_INBOX:
				Pointer_File = Pointer_File_Inbox;
				break;

			case SMS_STORAGE_LOCATION_SENT:
				Pointer_File = Pointer_File_Sent;
				break;

			case SMS_STORAGE_LOCATION_DRAFT:
				Pointer_File = Pointer_File_Draft;
				break;

			default:
				printf("Error : unknown storage location %d.\n", Pointer_SMS_Record->Message_Storage_Location);
				goto Exit;
		}

		// Handle messages split in multiple parts
		if (Pointer_SMS_Record->Records_Count > 1)
		{
			// Ignore the record if it is not the initial record (all message records will be processed when the initial record will be found)
			if (Pointer_SMS_Record->Record_Number > 1) continue;

			// This is the initial record, write its content to the appropriate file
			if (SMSWriteOutputMessageInformation(Pointer_File, Pointer_SMS_Record) != 0) return -1;
			fprintf(Pointer_File, "%s", Pointer_SMS_Record->String_Text);

			// Search for the next record
			Current_Record_Number = 2; // Start searching from record 2 as record 1 has already been written to the file
			for (j = 0; j < SMS_RECORDS_MAXIMUM_COUNT; j++)
			{
				// Cache searched record access
				Pointer_Searched_SMS_Record = &SMS_Records[j];

				// Append the next record text
				if ((Pointer_Searched_SMS_Record->Records_Count > 1) && (Pointer_Searched_SMS_Record->Record_ID == Pointer_SMS_Record->Record_ID) && (Pointer_Searched_SMS_Record->Record_Number == Current_Record_Number))
				{
					fprintf(Pointer_File, "%s", Pointer_Searched_SMS_Record->String_Text);
					Current_Record_Number++;

					// Was it the last record ?
					if (Current_Record_Number > Pointer_SMS_Record->Records_Count)
					{
						fprintf(Pointer_File, "\n\n");
						break;
					}
				}
			}
		}
		// This message is stored on a single record, write the record content to the appropriate file
		else
		{
			if (SMSWriteOutputMessageInformation(Pointer_File, Pointer_SMS_Record) != 0) return -1;
			fprintf(Pointer_File, "%s\n\n", Pointer_SMS_Record->String_Text);
		}
	}

	// Everything went fine
	Return_Value = 0;

Exit:
	if (Pointer_File_Inbox != NULL) fclose(Pointer_File_Inbox);
	if (Pointer_File_Sent != NULL) fclose(Pointer_File_Sent);
	if (Pointer_File_Draft != NULL) fclose(Pointer_File_Draft);
	return Return_Value;
}
