/** @file SMS.c
 * See SMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <errno.h>
#include <File_Manager.h>
#include <Log.h>
#include <Phone_Book.h>
#include <SMS.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** Allow to turn on or off debug messages. */
#define SMS_IS_DEBUG_ENABLED 0

/** The size in bytes of the buffer holding a SMS text. */
#define SMS_TEXT_STRING_MAXIMUM_SIZE 512

/** How many records to read from the phone. */
#define SMS_RECORDS_MAXIMUM_COUNT 450 // This value is reported by the command AT+EQSI, it is set to 20 for the SIM storage and 450 for the mobile equipment storage

/** The hardcoded path of the directory containing the archived SMS files. */
#define SMS_ARCHIVED_MESSAGES_DIRECTORY_PATH "C:\\SMSArch"
/** The temporary file used to store each archived message. */
#define SMS_ARCHIVED_MESSAGE_TEMPORARY_FILE_PATH "Output/SMS/Archive.tmp"

//-------------------------------------------------------------------------------------------------
// Private types
//-------------------------------------------------------------------------------------------------
/** All available message storage locations. */
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
	static unsigned char Extended_ASCII_Conversion_Table[256] = // Look-up table that converts SMS extended characters to Windows CP1252 extended characters, the default SMS alphabet is described in the GSM 03.38 Version 5.3.0 document section 6.2.1 (see also https://www.developershome.com/sms/gsmAlphabet.asp)
	{
		 '@', 0xA3,  '$', 0xA5, 0xE8, 0xE9, 0xF9, 0xEC, 0xF2, 0xC7, '\n', 0xD8, 0xF8, '\r', 0xC5, 0xE5,
		0x86,  '_', 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0xC6, 0xE6, 0xDF, 0xC9,
		 ' ',  '!',  '"',  '#', 0xA4,  '%',  '&', '\'',  '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
		 '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
		0xA1,  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
		 'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z', 0xC4, 0xD6, 0xD1, 0xDC, 0xA7,
		0xBF,  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z', 0xE4, 0xF6, 0xF1, 0xFC, 0xE0
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
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Text converted to Windows 1252 characters set : \"%s\".\n", Pointer_String_Custom_Character_Set_Text);

	// Convert text to UTF-8
	UtilityConvertString(Pointer_String_Custom_Character_Set_Text, Pointer_String_Converted_Text, UTILITY_CHARACTER_SET_WINDOWS_1252, UTILITY_CHARACTER_SET_UTF8, 0, SMS_TEXT_STRING_MAXIMUM_SIZE);
}

/** Extract the phone number from an ETSI GSM 03.38 version 5.3.0 address field (see paragraph 9.1.2.5).
 * @param Pointer_Pointer_Message_Buffer On input, the pointer must be initialized with the position in the message buffer that corresponds to the beginning of the address field. On output, the pointer is updated with the amount of read bytes and targets the data right after the address field.
 * @param Pointer_Text_Payload_Offset On input, the pointer must be initialized with the current text payload offset value. On output, this value is updated accordingly.
 * @param Pointer_SMS_Record On output, the phone number is filled.
 */
static void SMSExtractHeaderPhoneNumber(unsigned char **Pointer_Pointer_Message_Buffer, int *Pointer_Text_Payload_Offset, TSMSRecord *Pointer_SMS_Record)
{
	unsigned char *Pointer_Message_Buffer = *Pointer_Pointer_Message_Buffer;
	char *Pointer_String_Phone_Number = Pointer_SMS_Record->String_Phone_Number, Digit;
	int Phone_Number_Length, i, Is_Least_Significant_Nibble_Selected = 1, Text_Payload_Offset = 0;

	// Retrieve Address-Length
	Phone_Number_Length = *Pointer_Message_Buffer;
	Pointer_Message_Buffer += 2; // Bypass in the same time the Type-of-Address byte
	Text_Payload_Offset += 2;
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Phone number length : %d bytes.\n", Phone_Number_Length);

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

		// Discard double zeros prefix if any
		UtilityNormalizePhoneNumber(Pointer_SMS_Record->String_Phone_Number);
	}
	// No phone number was provided, make sure a string is present to tell that
	else strcpy(Pointer_String_Phone_Number, "<Unspecified>");
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Phone number : %s.\n", Pointer_SMS_Record->String_Phone_Number);

	// Adjust the amount of read bytes from the buffer
	*Pointer_Pointer_Message_Buffer = Pointer_Message_Buffer;
	*Pointer_Text_Payload_Offset += Text_Payload_Offset;
}

/** Parse the SMS PDU header according to ETSI GSM 03.40 version 5.3.0 and ETSI GSM 03.38 version 5.3.0 documents. Extract useful information and find the text payload location and encoding.
 * @param Pointer_Message_Buffer The raw message, as downloaded from the phone.
 * @param Pointer_SMS_Record On output, fill most of the information of the decoded SMS record.
 * @param Pointer_Is_Wide_Character_Encoding On output, tell whether the message text uses UTF-16 encoding (if set to 1) or 7-bit characters (if set to 0).
 * @param Pointer_Text_Bytes_Count On output, contain the size in bytes of the decoded text.
 * @return -1 on error,
 * @return On success, a positive number indicating the offset of the text payload in the message buffer.
 * @note The SMS format is specified by ETSI GSM 03.40 version 5.3.0 document.
 */
static int SMSDecodeRecordHeader(unsigned char *Pointer_Message_Buffer, TSMSRecord *Pointer_SMS_Record, int *Pointer_Is_Wide_Character_Encoding, int *Pointer_Text_Bytes_Count)
{
	unsigned char Byte, First_Message_Byte;
	int Text_Payload_Offset = 0, Is_SMS_Deliver_Message_Type, Validity_Period_Format = 0;

	// Bypass the SMSC information if this is a SMS-DELIVER message (see http://www.gsm-modem.de/sms-pdu-mode.html)
	Byte = *Pointer_Message_Buffer;
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "SMSC length : %d bytes.\n", Byte);
	Pointer_Message_Buffer += Byte + 1; // Also add 1 to bypass the SMSC length byte
	Text_Payload_Offset += Byte + 1;

	// Cache the first message byte (the one following the SMSC) as it will be used several times
	First_Message_Byte = *Pointer_Message_Buffer;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Extract the Message-Type-Indicator
	Byte = First_Message_Byte & 0x03;
	if (Byte == 0) Is_SMS_Deliver_Message_Type = 1;
	else if (Byte == 1) Is_SMS_Deliver_Message_Type = 0;
	else
	{
		LOG("Unsupported SMS Message-Type-Indicator : 0x%02X.\n", Byte);
		return -1;
	}
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "SMS Message-Type-Indicator : %s.\n", Is_SMS_Deliver_Message_Type ? "SMS-DELIVER" : "SMS-SUBMIT");

	// Manage submitted-only message fields
	if (!Is_SMS_Deliver_Message_Type)
	{
		// Is the validity period field present on a SMS-SUBMIT message ?
		Validity_Period_Format = (First_Message_Byte >> 3) & 0x03;
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Validity period format : %d.\n", Validity_Period_Format);

		// Bypass the Message-Reference byte
		Pointer_Message_Buffer++;
		Text_Payload_Offset++;
	}

	// Extract Originating-Address for SMS-DELIVER, or Destination-Address for SMS-SUBMIT
	SMSExtractHeaderPhoneNumber(&Pointer_Message_Buffer, &Text_Payload_Offset, Pointer_SMS_Record);

	// Bypass Protocol-Identifier byte
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Extract Data-Coding-Scheme to determine whether the text is 7-bit ASCII encoded or UTF-16 encoded (see ETSI GSM 03.38 version 5.3.0 document for more details)
	Byte = (*Pointer_Message_Buffer >> 2) & 0x03;
	if (Byte == 0) *Pointer_Is_Wide_Character_Encoding = 0; // Default alphabet
	else if (Byte == 2) *Pointer_Is_Wide_Character_Encoding = 1; // UCS2 (16bit)
	else
	{
		LOG("Error : unsupported Data-Coding-Scheme.\n");
		return -1;
	}
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Data coding scheme : %s.\n", *Pointer_Is_Wide_Character_Encoding ? "UCS2 (UTF-16)" : "default alphabet (7 bits)");

	// Extract message reception date and time (Service-Centre-Time-Stamp)
	if (Is_SMS_Deliver_Message_Type)
	{
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
		Pointer_Message_Buffer += 7; // Also bypass the time zone byte
		Text_Payload_Offset += 7;
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Message reception date : %04d-%02d-%02d %02d:%02d:%02d.\n", Pointer_SMS_Record->Date_Year, Pointer_SMS_Record->Date_Month, Pointer_SMS_Record->Date_Day, Pointer_SMS_Record->Time_Hour, Pointer_SMS_Record->Time_Minutes, Pointer_SMS_Record->Time_Seconds);
	}
	else if (Validity_Period_Format != 0)
	{
		if (Validity_Period_Format == 2)
		{
			// Bypass relative validity period
			Pointer_Message_Buffer++;
			Text_Payload_Offset++;
		}
		else
		{
			// Bypass absolute validity period
			Pointer_Message_Buffer += 7;
			Text_Payload_Offset += 7;
		}
	}

	// Retrieve the text size in bytes (User-Data-Length)
	*Pointer_Text_Bytes_Count = *Pointer_Message_Buffer;
	Pointer_Message_Buffer++;
	Text_Payload_Offset++;

	// Is the User-Data-Header-Indicator present (see ETSI GSM 03.40 version 5.3.0 chapter 9.2.3.23 and https://en.wikipedia.org/wiki/User_Data_Header) ?
	if (First_Message_Byte & 0x40)
	{
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "SMS User-Data-Header-Indicator is present.\n");

		// Bypass the User Data Header Length field, the parser will rely only on each Information Element Identifier length
		Pointer_Message_Buffer++;
		Text_Payload_Offset++;

		// Parse the element according to its Information Element Identifier
		Byte = *Pointer_Message_Buffer;
		Pointer_Message_Buffer++;
		Text_Payload_Offset++;
		*Pointer_Text_Bytes_Count -= 2; // Also take into account the User Data Header Length field
		switch (Byte)
		{
			// This is the concatenated short messages facility with 8-bit reference numbers (see ETSI GSM 03.40 version 5.3.0 chapter 9.2.3.24.1)
			case 0:
				LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "SMS User-Data-Header contains a concatenated short messages element with 8-bit reference numbers.\n");

				// Make sure the Length of the Information Element is what expected
				if (*Pointer_Message_Buffer != 3)
				{
					LOG("Error : unsupported Length of the Information Element : %d.\n", *Pointer_Message_Buffer);
					return -1;
				}
				Pointer_Message_Buffer++;
				Text_Payload_Offset++;

				// Decode the Information Element Data
				Pointer_SMS_Record->Record_ID = Pointer_Message_Buffer[0];
				Pointer_SMS_Record->Records_Count = Pointer_Message_Buffer[1];
				Pointer_SMS_Record->Record_Number = Pointer_Message_Buffer[2];
				LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Record ID : 0x%02X, records count : %d, record number : %d.\n", Pointer_SMS_Record->Record_ID, Pointer_SMS_Record->Records_Count, Pointer_SMS_Record->Record_Number);
				Pointer_Message_Buffer += 3;
				Text_Payload_Offset += 3;

				// Adjust the data length accordingly
				*Pointer_Text_Bytes_Count -= 4;
				break;

			// This is the concatenated short messages facility with 16-bit reference numbers (see https://en.wikipedia.org/wiki/User_Data_Header and https://en.wikipedia.org/wiki/Concatenated_SMS)
			case 8:
				LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "SMS User-Data-Header contains a concatenated short messages element with 16-bit reference numbers.\n");

				// Make sure the Length of the Information Element is what expected
				if (*Pointer_Message_Buffer != 4)
				{
					LOG("Error : unsupported Length of the Information Element : %d.\n", *Pointer_Message_Buffer);
					return -1;
				}
				Pointer_Message_Buffer++;
				Text_Payload_Offset++;

				// Decode the Information Element Data
				Pointer_SMS_Record->Record_ID = (Pointer_Message_Buffer[0] << 8) | Pointer_Message_Buffer[1];
				Pointer_SMS_Record->Records_Count = Pointer_Message_Buffer[2];
				Pointer_SMS_Record->Record_Number = Pointer_Message_Buffer[3];
				LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Record ID : 0x%04X, records count : %d, record number : %d.\n", Pointer_SMS_Record->Record_ID, Pointer_SMS_Record->Records_Count, Pointer_SMS_Record->Record_Number);
				Pointer_Message_Buffer += 4;
				Text_Payload_Offset += 4;

				// Adjust the data length accordingly
				*Pointer_Text_Bytes_Count -= 5;
				break;

			default:
				LOG("Error : unsupported Information Element Identifier : %d.\n", Byte);
				return -1;
		}
	}
	// Make sure values are coherent if there is only a single record (in this case there is not record 4 bytes field
	else
	{
		Pointer_SMS_Record->Records_Count = 1;
		Pointer_SMS_Record->Record_Number = 1; // Record numbers start from 1
	}

	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Data length : %d bytes.\n", *Pointer_Text_Bytes_Count);
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Text payload offset : %d.\n", Text_Payload_Offset);

	return Text_Payload_Offset;
}

/** Retrieve an SMS PDU from the phone.
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
	unsigned char Current_Byte, Next_Byte;
	int Text_Payload_Offset, Is_Wide_Character_Encoding, Text_Payload_Bytes_Count, Septet_Padding_Bits_Count, i;
	TSMSStorageLocation Message_Storage_Location;

	// Send the command
	sprintf(String_Command, "AT+EMGR=%d", SMS_Number);
	if (ATCommandSendCommand(Serial_Port_ID, String_Command) != 0)
	{
		LOG("Error : failed to send the AT command to read SMS record %d.\n", SMS_Number);
		return -1;
	}

	// Wait for the information string
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "AT command answer : \"%s\".\n", String_Temporary);
	if (strcmp(String_Temporary, "+CMS ERROR: 321") == 0) return -3; // The message storage location is empty
	if (sscanf(String_Temporary, "+EMGR: %d", (int *) &Message_Storage_Location) != 1) return -1; // Extract message storage location information
	Pointer_SMS_Record->Message_Storage_Location = Message_Storage_Location;
	switch (Pointer_SMS_Record->Message_Storage_Location)
	{
		case SMS_STORAGE_LOCATION_INBOX:
			LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Message storage location : inbox.\n");
			break;

		case SMS_STORAGE_LOCATION_SENT:
			LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Message storage location : sent.\n");
			break;

		case SMS_STORAGE_LOCATION_DRAFT:
			LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Message storage location : draft.\n");
			break;

		default:
			LOG("Error : unsupported message storage location %d.\n", Pointer_SMS_Record->Message_Storage_Location);
			return -3;
	}

	// Wait for the message content
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Hexadecimal content : %s.\n", String_Temporary);

	// Wait for the standard OK
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for empty line before "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for "OK"
	if (strcmp(String_Command, "OK") != 0) return -1;

	// Convert all characters to their binary representation to allow processing them
	if (ATCommandConvertHexadecimalToBinary(String_Temporary, Temporary_Buffer, sizeof(Temporary_Buffer)) < 0) return -1;

	// Retrieve all useful information from the message header
	Text_Payload_Offset = SMSDecodeRecordHeader(Temporary_Buffer, Pointer_SMS_Record, &Is_Wide_Character_Encoding, &Text_Payload_Bytes_Count);
	if (Text_Payload_Offset < 0) return -1;

	// Decode text
	if (Is_Wide_Character_Encoding)
	{
		// Convert UTF-16 to UTF-8
		if (UtilityConvertString(&Temporary_Buffer[Text_Payload_Offset], Pointer_SMS_Record->String_Text, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Text_Payload_Bytes_Count, sizeof(Pointer_SMS_Record->String_Text)) < 0) return -1;
	}
	else
	{
		// Concatenated messages with 7-bit characters have a header that must be a multiple of septets
		if (Pointer_SMS_Record->Records_Count > 1)
		{
			Septet_Padding_Bits_Count = 1; // The Concatenated Short Messages user header is 6-byte long, so it is 48-bit long. To align on a septet, 1 padding bit must be added TODO support more user headers if needed by providing the padding bits count as a SMSDecodeRecordHeader() parameter
			Text_Payload_Bytes_Count--; // TODO Is this needed because of the 1-bit padding ?

			// Shift all message bytes by the needed amount of bits
			for (i = 0; i < Text_Payload_Bytes_Count; i++)
			{
				// Shift the current byte by the padding bits amount
				Current_Byte = Temporary_Buffer[Text_Payload_Offset + i];
				Current_Byte >>= Septet_Padding_Bits_Count;

				// Make sure not to overflow the reception buffer by trying to access one more byte at the end of the message
				if (i < Text_Payload_Bytes_Count - 1)
				{
					// Retrieve the remaining current byte bits in the next byte
					Next_Byte = Temporary_Buffer[Text_Payload_Offset + i + 1];
					Next_Byte <<= 8 - Septet_Padding_Bits_Count;
					Current_Byte |= Next_Byte;
				}

				// Update the data buffer so it can be passed as-is to the uncompression function
				Temporary_Buffer[Text_Payload_Offset + i] = Current_Byte;
			}
		}

		// Extract the text content with the custom character set for extended ASCII
		SMSUncompress7BitText(&Temporary_Buffer[Text_Payload_Offset], Text_Payload_Bytes_Count, String_Temporary);
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Uncompressed text (may miss some SMS custom characters) : \"%s\".\n", String_Temporary);

		// Convert custom character set to UTF-8
		SMSConvert7BitExtendedASCII(String_Temporary, Pointer_SMS_Record->String_Text);
	}
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Text converted to UTF-8 : \"%s\".\n", Pointer_SMS_Record->String_Text);

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
	int Result;

	// Try to find the name of the sender
	Result = PhoneBookGetNameFromNumber(Pointer_SMS_Record->String_Phone_Number, String_Temporary_2);
	if (Result == 0) LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "No matching name was found for the phone number \"%s\".\n", Pointer_SMS_Record->String_Phone_Number);
	else LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "The matching name \"%s\" was found for the phone number \"%s\".\n", String_Temporary_2, Pointer_SMS_Record->String_Phone_Number);

	// Write phone number
	if (Pointer_SMS_Record->Message_Storage_Location == SMS_STORAGE_LOCATION_INBOX) strcpy(String_Temporary, "From : ");
	else strcpy(String_Temporary, "To : ");
	strcat(String_Temporary, String_Temporary_2);
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

/** Parse a archived SMS file (with a .a file extension) located at the path SMS_ARCHIVED_MESSAGE_TEMPORARY_FILE_PATH to extract the message content.
 * @param Pointer_String_Converted_Text On output, contain the message text converted to UTF-8. Make sure to provide a buffer big enough, otherwise some data may be truncated.
 * @param Converted_Text_String_Length The size in bytes of the output string.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
static int SMSExtractArchivedMessageText(char *Pointer_String_Converted_Text, size_t Converted_Text_String_Length)
{
	static unsigned char Buffer[16386]; // Should be enough for any SMS content, store the variable in the DATA section due to its size
	FILE *Pointer_File;
	int Return_Value = -1;
	size_t Data_Size;

	// Try to open the file
	Pointer_File = fopen(SMS_ARCHIVED_MESSAGE_TEMPORARY_FILE_PATH, "r");
	if (Pointer_File == NULL)
	{
		LOG("Error : failed to open the archived SMS temporary file \"%s\" (%s).\n", SMS_ARCHIVED_MESSAGE_TEMPORARY_FILE_PATH, strerror(errno));
		return -1;
	}

	// Discard the first byte because it is unknown yet, the following two bytes contain the data size
	if (fread(Buffer, 1, 3, Pointer_File) != 3)
	{
		LOG("Error : failed to read the initial 3 bytes of the archived SMS file (%s).\n", strerror(errno));
		goto Exit;
	}
	Data_Size = (Buffer[2] << 8) | Buffer[1];
	Data_Size += 2; // There are always 2 zeroed bytes at the end of the file (which are not taken into account by this data size value), it's pretty sure that their use is to provide an UTF-16 string ending zero character
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Data size = %zu bytes.\n", Data_Size);

	// Make sure there is enough room to load the data in RAM
	if (Data_Size > sizeof(Buffer))
	{
		LOG("Error : the archived SMS data size (%zu) is too big to fit in memory (%zu).\n", Data_Size, sizeof(Buffer));
		goto Exit;
	}

	// Load the data
	if (fread(Buffer, 1, Data_Size, Pointer_File) != Data_Size)
	{
		LOG("Error : failed to read the data of the archived SMS file (%s).\n", strerror(errno));
		goto Exit;
	}

	// Convert the string to more standard UTF-8
	if (UtilityConvertString(Buffer, Pointer_String_Converted_Text, UTILITY_CHARACTER_SET_UTF16_LITTLE_ENDIAN, UTILITY_CHARACTER_SET_UTF8, sizeof(Buffer), Converted_Text_String_Length) < 0)
	{
		LOG("Error : could not convert the string to UTF-8.\n");
		goto Exit;
	}

	// Everything went fine
	Return_Value = 0;

Exit:
	fclose(Pointer_File);
	return Return_Value;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int SMSDownloadAll(TSerialPortID Serial_Port_ID)
{
	static TSMSRecord SMS_Records[SMS_RECORDS_MAXIMUM_COUNT];
	static char String_Temporary[16384]; // Should be enough for any SMS content, store the variable in the DATA section due to its size
	int i, Return_Value = -1, j, Current_Record_Number, Archived_SMS_Count;
	FILE *Pointer_File_Inbox = NULL, *Pointer_File_Sent = NULL, *Pointer_File_Draft = NULL, *Pointer_File_Archives = NULL, *Pointer_File;
	TSMSRecord *Pointer_SMS_Record, *Pointer_Searched_SMS_Record;
	TList List;
	TListItem *Pointer_List_Item;
	TFileManagerFileListItem *Pointer_File_List_Item;

	printf("Retrieving phone book information to match with SMS phone numbers...\n");
	if (PhoneBookReadAllEntries(Serial_Port_ID) < 0) goto Exit;

	// Read all possible records
	printf("Retrieving all SMS records...\n");
	for (i = 1; i <= SMS_RECORDS_MAXIMUM_COUNT; i++)
	{
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "SMS record number = %d/%d.\n", i, SMS_RECORDS_MAXIMUM_COUNT);
		if (SMSDownloadSingleRecord(Serial_Port_ID, i, &SMS_Records[i - 1]) == 0) LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Record contains data.\n"); // Record array is zero-based
		else LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Record is empty.\n");
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "\n");
	}

	// Create output directories
	if (UtilityCreateDirectory("Output/SMS") != 0) goto Exit;

	// Create all needed files
	// Inbox
	Pointer_File_Inbox = fopen("Output/SMS/Inbox.txt", "w");
	if (Pointer_File_Inbox == NULL)
	{
		LOG("Error : could not create the SMS \"Inbox.txt\" file (%s).\n", strerror(errno));
		goto Exit;
	}
	// Sent
	Pointer_File_Sent = fopen("Output/SMS/Sent.txt", "w");
	if (Pointer_File_Sent == NULL)
	{
		LOG("Error : could not create the SMS \"Sent.txt\" file (%s).\n", strerror(errno));
		goto Exit;
	}
	// Draft
	Pointer_File_Draft = fopen("Output/SMS/Draft.txt", "w");
	if (Pointer_File_Draft == NULL)
	{
		LOG("Error : could not create the SMS \"Draft.txt\" file (%s).\n", strerror(errno));
		goto Exit;
	}
	// Archives
	Pointer_File_Archives = fopen("Output/SMS/Archives.txt", "w");
	if (Pointer_File_Archives == NULL)
	{
		LOG("Error : could not create the SMS \"Archives.txt\" file (%s).\n", strerror(errno));
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

				// Find the message next record
				if ((Pointer_Searched_SMS_Record->Records_Count > 1) && (Pointer_Searched_SMS_Record->Record_ID == Pointer_SMS_Record->Record_ID) && (Pointer_Searched_SMS_Record->Record_Number == Current_Record_Number))
				{
					// Add some more checks because the record ID is stored on one byte only and this can lead to collisions pretty fast
					// The message storage location must be identical
					if (Pointer_Searched_SMS_Record->Message_Storage_Location != Pointer_SMS_Record->Message_Storage_Location) continue;
					// The phone number must be identical
					if (strncmp(Pointer_Searched_SMS_Record->String_Phone_Number, Pointer_SMS_Record->String_Phone_Number, sizeof(Pointer_Searched_SMS_Record->String_Phone_Number)) != 0) continue;

					// Append the next record text
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

	// Retrieve archive files
	if (FileManagerListDirectory(Serial_Port_ID, SMS_ARCHIVED_MESSAGES_DIRECTORY_PATH, &List) != 0)
	{
		LOG("Error : could not list the content of the archived SMS directory.\n");
		goto Exit;
	}
	Archived_SMS_Count = List.Items_Count - 2; // Do not take the special directory entries ("." and "..") into account
	LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "Archived message files found : %d.\n", Archived_SMS_Count);

	// Process each archived SMS file
	Pointer_List_Item = List.Pointer_Head;
	i = 1;
	while (Pointer_List_Item != NULL)
	{
		Pointer_File_List_Item = Pointer_List_Item->Pointer_Data;

		// Bypass the special directory entries
		if (strcmp(Pointer_File_List_Item->String_File_Name, ".") == 0) goto Next_Archived_File;
		if (strcmp(Pointer_File_List_Item->String_File_Name, "..") == 0) goto Next_Archived_File;

		// Retrieve the file
		printf("Retrieving the archived SMS %d/%d...\n", i, Archived_SMS_Count);
		i++;
		snprintf(String_Temporary, sizeof(String_Temporary), SMS_ARCHIVED_MESSAGES_DIRECTORY_PATH "\\%s", Pointer_File_List_Item->String_File_Name);
		LOG_DEBUG(SMS_IS_DEBUG_ENABLED, "File to retrieve : \"%s\".\n", String_Temporary);
		if (FileManagerDownloadFile(Serial_Port_ID, String_Temporary, SMS_ARCHIVED_MESSAGE_TEMPORARY_FILE_PATH) != 0)
		{
			LOG("Error : failed to retrieve the SMS file \"%s\".\n", String_Temporary);
			goto Exit_Clear_List;
		}

		// Retrieve the message content
		if (SMSExtractArchivedMessageText(String_Temporary, sizeof(String_Temporary)) != 0)
		{
			LOG("Error : failed to extract the SMS message content.\n");
			goto Exit_Clear_List;
		}

		// Append the message to the output file
		fprintf(Pointer_File_Archives, "Message\n-------\n%s\n\n", String_Temporary);

Next_Archived_File:
		Pointer_List_Item = Pointer_List_Item->Pointer_Next_Item;
	}

	// Everything went fine
	Return_Value = 0;
	Pointer_File = NULL; // Tell the exit code that this file has already been closed

Exit_Clear_List:
	ListClear(&List);

Exit:
	unlink(SMS_ARCHIVED_MESSAGE_TEMPORARY_FILE_PATH);
	if (Pointer_File_Inbox != NULL) fclose(Pointer_File_Inbox);
	if (Pointer_File_Sent != NULL) fclose(Pointer_File_Sent);
	if (Pointer_File_Draft != NULL) fclose(Pointer_File_Draft);
	if (Pointer_File_Archives != NULL) fclose(Pointer_File_Archives);
	return Return_Value;
}
