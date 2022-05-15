/** @file MMS.c
 * See MMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <arpa/inet.h>
#include <AT_Command.h>
#include <errno.h>
#include <fcntl.h>
#include <File_Manager.h>
#include <Log.h>
#include <MMS.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** Allow to turn on or off debug messages. */
#define MMS_IS_DEBUG_ENABLED 0

/** The local file name and path of the MMS database file. */
#define MMS_DATABASE_FILE_NAME "Output/MMS/Database.db"
/** The local file name of a downloaded but not yet processed MMS file. */
#define MMS_RAW_MMS_FILE_NAME "Output/MMS/MMS.tmp"

//-------------------------------------------------------------------------------------------------
// Private types
//-------------------------------------------------------------------------------------------------
/** All locations the MMS are stored in. */
typedef enum
{
	MMS_STORAGE_LOCATION_INBOX = 1,
	MMS_STORAGE_LOCATION_OUTBOX = 2,
	MMS_STORAGE_LOCATION_SENT = 4,
	MMS_STORAGE_LOCATION_DRAFTS = 8,
	MMS_STORAGE_LOCATION_TEMPLATES = 64
} TMMSStorageLocation;

/** All memory devices the MMS can be stored in. */
typedef enum
{
	MMS_STORAGE_DEVICE_PHONE = 2,
	MMS_STORAGE_DEVICE_SD_CARD = 4
} TMMSStorageDevice;

/** A record in the MMS information database. */
typedef struct __attribute__((packed))
{
	unsigned int Message_ID; //!< This value is used by commands like "AT+EMMSEXE=2,0,2147483658". This value is stored in little endian in the file.
	unsigned int Padding;
	unsigned int File_Size; //!< The size in bytes of the file in which all MMS data are stored. This value is stored in little endian in the file.
	unsigned int Unknown;
	char String_File_Name[40]; //!< The string is zero-terminated.
	char String_Phone_Number[80]; //!< The string is zero-terminated.
} TMMSDatabaseRecord;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** TODO */
static int MMSGetStorageInformation(TSerialPortID Serial_Port_ID, TMMSStorageLocation Storage_Location, TMMSStorageDevice Storage_Device, int *Pointer_Messages_Count, char *Pointer_String_Messages_Payload_Directory, char *Pointer_String_Database_File)
{
	char String_Temporary[256], String_Answer[256];
	unsigned char Buffer[256];
	int Size;

	// Send the command
	sprintf(String_Temporary, "AT+EMMSFS=%d,%d", Storage_Location, Storage_Device);
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) != 0) return -1;

	// Was the command successful ?
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Answer, sizeof(String_Answer)) < 0) return -1;
	if (strncmp(String_Answer, "+EMMSFS: 0,", 11) != 0)
	{
		LOG("Error : failed to send the AT command that queries the MMS count (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	// Wait for the empty line before "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	// Wait for "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that queries the MMS count (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}

	// Extract messages count
	if (sscanf(String_Answer, "+EMMSFS: %*d, %d", Pointer_Messages_Count) != 1)
	{
		LOG("Error : failed to extract the MMS count (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}

	// Do not parse the remaining fields if no message is available
	if (*Pointer_Messages_Count == 0) return 0;

	// Extract messages payload directory
	if (sscanf(String_Answer, "+EMMSFS: %*d, %*d, %*d, \"%[0-9A-F]\"", String_Temporary) != 1)
	{
		LOG("Error : failed to extract the MMS payload directory (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	// Convert the string to binary UTF-16, so it can be converted to UTF-8
	Size = ATCommandConvertHexadecimalToBinary(String_Temporary, Buffer, sizeof(Buffer));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS payload directory hexadecimal string \"%s\" to binary (storage location = %d, storage device = %d).\n", String_Temporary, Storage_Location, Storage_Device);
		return -1;
	}
	// Convert the string to UTF-8
	Size = UtilityConvertString(Buffer, Pointer_String_Messages_Payload_Directory, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_Temporary));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS payload directory from UTF-16 to UTF-8 (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	Pointer_String_Messages_Payload_Directory[Size] = 0; // Make sure the string is terminated

	// Extract database file
	if (sscanf(String_Answer, "+EMMSFS: %*d, %*d, %*d, \"%*[0-9A-F]\", \"%[0-9A-F]\"", String_Temporary) != 1)
	{
		LOG("Error : failed to extract the MMS database file (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	// Convert the string to binary UTF-16, so it can be converted to UTF-8
	Size = ATCommandConvertHexadecimalToBinary(String_Temporary, Buffer, sizeof(Buffer));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS database file hexadecimal string \"%s\" to binary (storage location = %d, storage device = %d).\n", String_Temporary, Storage_Location, Storage_Device);
		return -1;
	}
	// Convert the string to UTF-8
	Size = UtilityConvertString(Buffer, Pointer_String_Database_File, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_Temporary));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS database file from UTF-16 to UTF-8 (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	Pointer_String_Database_File[Size] = 0; // Make sure the string is terminated

	return 0;
}

/** TODO
 * Designed for Encoded-string-value
 */
static int MMSReadStringField(FILE *Pointer_File, char *Pointer_String_Output, unsigned int Buffer_Size)
{
	unsigned char Byte;

	// Is the first byte the Value-length ?
	if (fread(&Byte, 1, 1, Pointer_File) != 1) return -1;
	if (Byte == 31)
	{
		// Discard Value-length value (TODO handle uintvar)
		if (fread(&Byte, 1, 1, Pointer_File) != 1) return -1;
		// Discard Char-set value
		if (fread(&Byte, 1, 1, Pointer_File) != 1) return -1;
	}
	else
	{
		// Store the byte as a regular character into the string
		*Pointer_String_Output = Byte;
		if (Byte == 0) return 0;
		Pointer_String_Output++;
		Buffer_Size--;
	}

	// Read characters without overflowing the output buffer
	while (Buffer_Size > 0)
	{
		// Read next character
		if (fread(Pointer_String_Output, 1, 1, Pointer_File) != 1)
		{
			LOG("Error : unexpected file end or read error (%s).\n", strerror(errno));
			return -1;
		}

		// Is the string end reached ?
		if (*Pointer_String_Output == 0) return 0;

		// Get next character
		Pointer_String_Output++;
		Buffer_Size--;
	}

	// There is not enough room in the output buffer
	LOG("Error : the output buffer size is smaller than the string size.\n");
	return -1;
}

/** TODO */
static int MMSReadWAPVariableLengthUnsignedInteger(FILE *Pointer_File, unsigned int *Pointer_Unsigned_Integer)
{
	unsigned char Byte;
	unsigned int Temporary_Unsigned_Integer = 0;
	int i;

	// WAP-230-WSP-20010705-a specification chapter 8.1.2 tells that an uintvar is stored in up to 5 bytes
	for (i = 0; i < 5; i++)
	{
		// Read the next byte
		if (fread(&Byte, 1, 1, Pointer_File) != 1) return -1;

		// Extract the byte payload
		Temporary_Unsigned_Integer |= Byte & 0x7F;

		// The number is complete when the but 7 is cleared
		if (!(Byte & 0x80))
		{
			*Pointer_Unsigned_Integer = Temporary_Unsigned_Integer;
			return 0;
		}

		// Make room for the next byte
		Temporary_Unsigned_Integer <<= 7;
	}

	// There are too much bytes in this uintvar, it can't be valid
	return -1;
}

/** TODO */
static int MMSReadWAPIntegerValue(FILE *Pointer_File, int *Pointer_Value)
{
	unsigned char Byte;

	// The first byte tells whether this is a Short-integer or a Long-integer data type (see WAP-230-WSP-20010705-a specification chapter 8.4.2.3)
	if (fread(&Byte, 1, 1, Pointer_File) != 1) return -1;

	// A Short-integer has the most significant bit set and store its data on the 7 remaining bits
	if (Byte & 0x80) *Pointer_Value = Byte & 0x7F;
	else
	{
		LOG("TODO\n");
		return -1;
	}

	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Integer value : %d.\n", *Pointer_Value);

	return 0;
}

/** TODO */
static int MMSReadWAPValueLength(FILE *Pointer_File, unsigned int *Pointer_Length)
{
	unsigned char Byte;

	// First byte is the short length
	if (fread(&Byte, 1, 1, Pointer_File) != 1) return -1;
	if (Byte < 31)
	{
		*Pointer_Length = Byte;
		return 0;
	}

	// The specification does not tell what to do if the value is not 31, returning a length of 0 seems to be fine
	if (Byte != 31)
	{
		LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Warning: the WAP \"Length-quote\" octet should be 31 here, but it is %d.\n", Byte);
		*Pointer_Length = 0;
		return 0;
	}

	// Get the length stored as uintvar
	if (MMSReadWAPVariableLengthUnsignedInteger(Pointer_File, Pointer_Length) != 0)
	{
		LOG("Error : could not read uintvar.\n");
		return -1;
	}

	return 0;
}

/** TODO
 * For now the content type data are discarded.
 */
static int MMSReadWAPContentType(FILE *Pointer_File, unsigned int *Pointer_Length)
{
	unsigned char Byte, Buffer[256];

	// This field is encoded according to WSP protocol, see WAP-230-WSP-20010705-a specification chapter 8.4.2.24
	if (fread(&Byte, 1, 1, Pointer_File) != 1) // Get the field value as named in chapter 8.4.1.2
	{
		LOG("Error : could not read the field value byte (%s).\n", strerror(errno));
		return -1;
	}
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Content type field value byte : %d.\n", Byte);

	if (Byte < 32)
	{
		// The field value is part of the value length data, so go back one byte to allow MMSReadWAPValueLength() to read the correct data
		if (fseek(Pointer_File, -1, SEEK_CUR) != 0)
		{
			LOG("Error : could not set file position (%s).\n", strerror(errno));
			return -1;
		}

		// Get the media type field length
		if (MMSReadWAPValueLength(Pointer_File, Pointer_Length) != 0)
		{
			LOG("Error : could not read media type field length.\n");
			return -1;
		}
		LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Content type length : %u bytes.\n", *Pointer_Length);

		// Read the data if any and discard it for now
		if (*Pointer_Length > 0)
		{
			// Make sure there is enough place in the destination buffer
			if (*Pointer_Length > sizeof(Buffer))
			{
				LOG("Error : the content type length is too big (%u bytes).\n", *Pointer_Length);
				return -1;
			}
			if (fread(Buffer, *Pointer_Length, 1, Pointer_File) != 1) return -1;
		}
	}
	else if (Byte < 128)
	{
		if (MMSReadStringField(Pointer_File, (char *) Buffer, sizeof(Buffer)) != 0) return -1;
		*Pointer_Length = strlen((char *) Buffer);
		LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Content type length : %d bytes, Content type string : \"%s\"\n", *Pointer_Length, (char *) Buffer);
	}
	else
	{
		if (fread(&Byte, 1, 1, Pointer_File) != 1)
		{
			LOG("Error : could not read the well known media value (%s).\n", strerror(errno));
			return -1;
		}
		LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Well known media value : %d.\n", Byte);
	}

	return 0;
}

/** TODO
 * @note This function assumes for now that only Application/vnd.wap.multipart.* are found in messages.
 * @note See WAP-230-WSP-20010705-a chapter 8.5 for more information about headers.
 */
int MMSExtractAttachedFile(FILE *Pointer_File, char *Pointer_String_Output_Directory_Path)
{
	static unsigned char Buffer[4096]; // This buffer is too big to be stored on stack
	unsigned int Headers_Length, Data_Length, Length, i;
	char String_File_Name[256], String_Temporary[512];
	FILE *Pointer_File_Output = NULL;
	int Return_Value = -1;
	size_t Chunk_Size;
	long Position_Before_Content_Type, Position_After_Content_Type;

	// Retrieve header length (this the length of the headers + the length of the data)
	if (MMSReadWAPVariableLengthUnsignedInteger(Pointer_File, &Headers_Length) != 0)
	{
		LOG("Error : could not read headers length.\n");
		return -1;
	}
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Headers + content type length : %u.\n", Headers_Length);

	// Extract attached file length
	if (MMSReadWAPVariableLengthUnsignedInteger(Pointer_File, &Data_Length) != 0)
	{
		LOG("Error : could not read data length.\n");
		return -1;
	}
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Data length : %u.\n", Data_Length);

	// Keep the file position before reading the content type, so the total amount of read bytes for the content type can be determined
	Position_Before_Content_Type = ftell(Pointer_File);
	if (Position_Before_Content_Type < 0)
	{
		LOG("Error : could not get the file position before reading the content type field (%s).\n", strerror(errno));
		return -1;
	}
	// Extract content type
	if (MMSReadWAPContentType(Pointer_File, &Length) != 0)
	{
		LOG("Error : could not read content type.\n");
		return -1;
	}
	// Get the file position after reading the content type
	Position_After_Content_Type = ftell(Pointer_File);
	if (Position_After_Content_Type < 0)
	{
		LOG("Error : could not get the file position after reading the content type field (%s).\n", strerror(errno));
		return -1;
	}

	// Extract headers
	Length = Headers_Length - (Position_After_Content_Type - Position_Before_Content_Type);
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Computed headers length : %u.\n", Length);
	if (Length > sizeof(Buffer))
	{
		LOG("Error : the headers are too big to fit in the buffer (length : %u).\n", Length);
		return -1;
	}
	if (fread(Buffer, Length, 1, Pointer_File) != 1)
	{
		LOG("Error : could not read headers.\n");
		return -1;
	}

	// Search for a specific tag that precedes the file name TODO better when the headers specs is found
	for (i = 0; i < Length; i++)
	{
		if (Buffer[i] == 0x8E)
		{
			i++; // Bypass the tag byte
			break;
		}
	}
	if (i == Length)
	{
		LOG("Error : no file name could be found.\n");
		return -1;
	}
	// Adjust length to the file name string length
	Length -= i;
	// Get file name
	strncpy(String_File_Name, (char *) &Buffer[i], Length - 1); // Make room for the terminating zero
	String_File_Name[Length - 1] = 0; // Make sure string is terminated
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Attached file name : \"%s\".\n", String_File_Name);

	// Try to create the output file
	snprintf(String_Temporary, sizeof(String_Temporary), "%s/%s", Pointer_String_Output_Directory_Path, String_File_Name);
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Attached file output path : \"%s\".\n", String_Temporary);
	Pointer_File_Output = fopen(String_Temporary, "w");
	if (Pointer_File_Output == NULL)
	{
		LOG("Error : failed to create the attached file \"%s\".\n", String_Temporary);
		return -1;
	}

	// Copy the attached file content to the output file
	while (Data_Length > 0)
	{
		// Read no more bytes than the buffer size, but when the attached file end has been reached
		if (Data_Length >= sizeof(Buffer)) Chunk_Size = sizeof(Buffer);
		else Chunk_Size = Data_Length;

		// Copy a chunk of data
		if (fread(Buffer, Chunk_Size, 1, Pointer_File) != 1) goto Exit;
		if (fwrite(Buffer, Chunk_Size, 1, Pointer_File_Output) != 1) goto Exit;

		// Prepare for next chunk
		Data_Length -= Chunk_Size;
	}

	// Everything went fine
	Return_Value = 0;

Exit:
	if (Pointer_File_Output != NULL) fclose(Pointer_File_Output);
	return Return_Value;
}

/** TODO */
static int MMSProcessMessage(char *Pointer_String_Raw_MMS_File_Path, char *Pointer_String_Output_Directory_Path)
{
	FILE *Pointer_File = NULL;
	unsigned char Byte, Buffer[256]; // A field size is stored on one byte, with 256 bytes even an invalid size can't overflow the buffer
	size_t Read_Bytes_Count;
	char String_Temporary[256], String_Sender_Phone_Number[32];
	int Return_Value = -1, *Pointer_Integer, i, Attached_Files_Count, Integer;
	struct tm *Pointer_Broken_Down_Time;
	time_t Unix_Timestamp;
	unsigned int Length;

	// Try to open the file
	Pointer_File = fopen(Pointer_String_Raw_MMS_File_Path, "r");
	if (Pointer_File == NULL)
	{
		LOG("Error : failed to open the MMS file \"%s\" (%s).\n", Pointer_String_Raw_MMS_File_Path, strerror(errno));
		return -1;
	}

	// Extract some useful data from the header
	while (1)
	{
		// Retrieve next byte
		Read_Bytes_Count = fread(&Byte, 1, 1, Pointer_File);
		if (Read_Bytes_Count == 0) break; // Exit when the end of the file is reached
		if (Read_Bytes_Count != 1)
		{
			LOG("Error : failed to read the MMS file \"%s\" (%s).\n", Pointer_String_Raw_MMS_File_Path, strerror(errno));
			break;
		}

		// This byte should be a field ID (the values come from the )
		Byte &= 0x7F; // Field IDs use only the last 7 bits
		switch (Byte)
		{
			// Bcc
			case 0x01:
				if (MMSReadStringField(Pointer_File, String_Temporary, sizeof(String_Temporary)) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Bcc record : \"%s\".\n", String_Temporary);
				break;

			// Cc
			case 0x02:
				if (MMSReadStringField(Pointer_File, String_Temporary, sizeof(String_Temporary)) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Cc record : \"%s\".\n", String_Temporary);
				break;

			// Content location
			case 0x03:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Content location record.\n");
				// TODO
				break;

			// Content type
			case 0x04:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Content type record.\n");
				if (MMSReadWAPContentType(Pointer_File, &Length) != 0)
				{
					LOG("Error : failed to read content type.\n");
					goto Exit;
				}
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Content type length : %u bytes.\n", Length);
				goto Parse_Attached_Files;

			// Date
			case 0x05:
				// Get the field size
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				// Get the date data
				if (fread(Buffer, Byte, 1, Pointer_File) != 1) goto Exit;
				// Date is a classic Unix timestamp stored in big endian
				Pointer_Integer = (int *) Buffer;
				Unix_Timestamp = (time_t) ntohl(*Pointer_Integer);
				Pointer_Broken_Down_Time = gmtime(&Unix_Timestamp);
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Date record : %04d-%02d-%02d %02d:%02d:%02d.\n",
					Pointer_Broken_Down_Time->tm_year + 1900,
					Pointer_Broken_Down_Time->tm_mon + 1,
					Pointer_Broken_Down_Time->tm_wday + 1,
					Pointer_Broken_Down_Time->tm_hour,
					Pointer_Broken_Down_Time->tm_min,
					Pointer_Broken_Down_Time->tm_sec);
				break;

			// Delivery report
			case 0x06:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Delivery report record : %d.\n", Byte);
				break;

			// Delivery time
			case 0x07:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Delivery time record.\n");
				// TODO
				break;

			// Expiry
			case 0x08:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Expiry record.\n");
				// TODO
				break;

			// From
			case 0x09:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found From record.\n");
				// Get Value-length field
				if (MMSReadWAPValueLength(Pointer_File, &Length) != 0) goto Exit;
				if (Length >= sizeof(String_Temporary))
				{
					LOG("Error : the From address size is too big.\n");
					goto Exit;
				}
				// Next byte can be Address-present-token (in this case the address is provided), or Insert-address-token (no address is provided)
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				if (Byte == 128)
				{
					if (MMSReadStringField(Pointer_File, String_Sender_Phone_Number, sizeof(String_Sender_Phone_Number)) != 0) goto Exit;
					LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Phone number is provided in From record : \"%s\".\n", String_Sender_Phone_Number);
				}
				else LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "The From record does not contain a phone number.\n");
				break;

			// Message class
			case 0x0A:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Message class record : %d.\n", Byte);
				break;

			// Message ID
			case 0x0B:
				if (MMSReadStringField(Pointer_File, String_Temporary, sizeof(String_Temporary)) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Message ID record : \"%s\".\n", String_Temporary);
				break;

			// Message type
			case 0x0C:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Message type record : %d.\n", Byte);
				break;

			// MMS version
			case 0x0D:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found MMS version record : %d.\n", Byte);
				break;

			// Message size
			case 0x0E:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Message size record.\n");
				// TODO
				break;

			// Priority
			case 0x0F:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Priority record : %d.\n", Byte);
				break;

			// Read report
			case 0x10:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Read report record : %d.\n", Byte);
				break;

			// Report allowed
			case 0x11:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Report allowed record.\n");
				// TODO
				break;

			// Response status
			case 0x12:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Response status record.\n");
				// TODO
				break;

			// Response text
			case 0x13:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Response text record.\n");
				// TODO
				break;

			// Sender visibility
			case 0x14:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Sender visibility record.\n");
				// TODO
				break;

			// Status
			case 0x15:
				if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Status record : %d.\n", Byte);
				break;

			// Subject
			case 0x16:
				if (MMSReadStringField(Pointer_File, String_Temporary, sizeof(String_Temporary)) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Subject record : \"%s\".\n", String_Temporary);
				break;

			// To
			case 0x17:
				if (MMSReadStringField(Pointer_File, String_Temporary, sizeof(String_Temporary)) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found To record : \"%s\".\n", String_Temporary);
				break;

			// Transaction ID
			case 0x18:
				if (MMSReadStringField(Pointer_File, String_Temporary, sizeof(String_Temporary)) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Transaction ID record : \"%s\".\n", String_Temporary);
				break;

			// Retrieve status
			case 0x19:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Retrieve status record.\n");
				// TODO
				break;

			// Retrieve text
			case 0x1A:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Retrieve text record.\n");
				// TODO
				break;

			// Read status
			case 0x1B:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Read status record.\n");
				// TODO
				break;

			// Reply charging
			case 0x1C:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Reply charging record.\n");
				// TODO
				break;

			// Reply charging deadline
			case 0x1D:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Reply charging deadline record.\n");
				// TODO
				break;

			// Reply charging ID
			case 0x1E:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Reply charging ID record.\n");
				// TODO
				break;

			// Reply charging size
			case 0x1F:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Reply charging size record.\n");
				// TODO
				break;

			// Previously sent by
			case 0x20:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Previously sent by record.\n");
				// TODO
				break;

			// Previously sent date
			case 0x21:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Previously sent date record.\n");
				// TODO
				break;

			// Store
			case 0x22:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Store record.\n");
				// TODO
				break;

			// MM state
			case 0x23:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found MM state record.\n");
				// TODO
				break;

			// MM flags
			case 0x24:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found MM flags record.\n");
				// TODO
				break;

			// Store status
			case 0x25:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Store status record.\n");
				// TODO
				break;

			// Store status text
			case 0x26:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Store status text record.\n");
				// TODO
				break;

			// Stored
			case 0x27:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Stored record.\n");
				// TODO
				break;

			// Attributes
			case 0x28:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Attributes record.\n");
				// TODO
				break;

			// Totals
			case 0x29:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Totals record.\n");
				// TODO
				break;

			// Mbox totals
			case 0x2A:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Mbox totals record.\n");
				// TODO
				break;

			// Quotas
			case 0x2B:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Quotas record.\n");
				// TODO
				break;

			// Mbox quotas
			case 0x2C:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Mbox quotas record.\n");
				// TODO
				break;

			// Message count
			case 0x2D:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Message count record.\n");
				// TODO
				break;

			// Content
			case 0x2E:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Content record.\n");
				// TODO
				break;

			// Start
			case 0x2F:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Start record.\n");
				// TODO
				break;

			// Additional headers
			case 0x30:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Additional headers record.\n");
				// TODO
				break;

			// Distribution indicator
			case 0x31:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Distribution indicator record.\n");
				// TODO
				break;

			// Element descriptor
			case 0x32:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Element descriptor record.\n");
				// TODO
				break;

			// Limit
			case 0x33:
				if (MMSReadWAPIntegerValue(Pointer_File, &Integer) != 0) goto Exit;
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Limit record : %d.\n", Integer);
				// TODO
				break;

			// Recommended retrieval mode
			case 0x34:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Recommended retrieval mode record.\n");
				// TODO
				break;

			// Recommended retrieval mode text
			case 0x35:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Recommended retrieval mode text record.\n");
				// TODO
				break;

			// Status text
			case 0x36:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Status text record.\n");
				// TODO
				break;

			// Applic ID
			case 0x37:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Applic ID record.\n");
				// TODO
				break;

			// Reply applic ID
			case 0x38:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Reply applic ID record.\n");
				// TODO
				break;

			// Aux applic info
			case 0x39:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Aux applic info record.\n");
				// TODO
				break;

			// Content class
			case 0x3A:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Content class record.\n");
				// TODO
				break;

			// DRM content
			case 0x3B:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found DRM content record.\n");
				// TODO
				break;

			// Adaptation allowed
			case 0x3C:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Adaptation allowed record.\n");
				// TODO
				break;

			// Replace ID
			case 0x3D:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Replace ID record.\n");
				// TODO
				break;

			// Cancel ID
			case 0x3E:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Cancel ID record.\n");
				// TODO
				break;

			// Cancel status
			case 0x3F:
				LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Found Cancel status record.\n");
				// TODO
				break;

			default:
				LOG("Unknown field : %d.\n", Byte);
				goto Exit;
		}
	}

Parse_Attached_Files:
	// Create the directory to which the extracted attached files will be saved
	sprintf(String_Temporary, "%s/%s_%04d-%02d-%02d_%02d-%02d-%02d",
		Pointer_String_Output_Directory_Path,
		String_Sender_Phone_Number,
		Pointer_Broken_Down_Time->tm_year + 1900,
		Pointer_Broken_Down_Time->tm_mon + 1,
		Pointer_Broken_Down_Time->tm_wday + 1,
		Pointer_Broken_Down_Time->tm_hour,
		Pointer_Broken_Down_Time->tm_min,
		Pointer_Broken_Down_Time->tm_sec);
	if (UtilityCreateDirectory(String_Temporary) != 0) goto Exit;

	// Get the amount of attached files
	if (fread(&Byte, 1, 1, Pointer_File) != 1) goto Exit;
	Attached_Files_Count = Byte; // See WAP-230-WSP-20010705-a chapter 8.5.1 for details about multipart header
	LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Attached files count : %d.\n", Attached_Files_Count);

	// Retrieve each file content
	for (i = 0; i < Attached_Files_Count; i++)
	{
		LOG_DEBUG(MMS_IS_DEBUG_ENABLED, "Processing file %d/%d...\n", i + 1, Attached_Files_Count);
		if (MMSExtractAttachedFile(Pointer_File, String_Temporary) != 0) goto Exit;
	}

	// Everything went fine
	Return_Value = 0;

Exit:
	if (Pointer_File != NULL) fclose(Pointer_File);
	return Return_Value;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int MMSDownloadAll(TSerialPortID Serial_Port_ID)
{
	static TMMSStorageLocation Storage_Location_Lookup_Table[] =
	{
		MMS_STORAGE_LOCATION_INBOX,
		MMS_STORAGE_LOCATION_OUTBOX,
		MMS_STORAGE_LOCATION_SENT,
		MMS_STORAGE_LOCATION_DRAFTS,
		MMS_STORAGE_LOCATION_TEMPLATES
	};
	static TMMSStorageDevice Storage_Device_Lookup_Table[] =
	{
		MMS_STORAGE_DEVICE_PHONE,
		MMS_STORAGE_DEVICE_SD_CARD
	};
	static char *Pointer_Strings_Storage_Location_Names[] =
	{
		"Inbox",
		"Outbox",
		"Sent",
		"Drafts",
		"Templates"
	};
	static char *Pointer_Strings_Storage_Device_Names[] =
	{
		"phone",
		"SD card"
	};
	int Messages_Count, File_Descriptor, i, Return_Value = -1;
	unsigned int Location_Index, Device_Index;
	char String_Messages_Payload_Directory[128], String_Database_File[128], String_Temporary[256];
	TMMSStorageLocation Storage_Location;
	TMMSStorageDevice Storage_Device;
	TMMSDatabaseRecord Database_Record;

	// Create output directories
	if (UtilityCreateDirectory("Output/MMS") != 0) return -1;
	// Use the same names for the location directories
	for (i = 0; i < (int) UTILITY_ARRAY_SIZE(Pointer_Strings_Storage_Location_Names); i++)
	{
		// Create the directory path
		sprintf(String_Temporary, "Output/MMS/%s", Pointer_Strings_Storage_Location_Names[i]);

		// Create the directory if it does not exist yet
		if (UtilityCreateDirectory(String_Temporary) != 0) return -1;
	}

	// Try all possible messages storage combinations
	for (Device_Index = 0; Device_Index < UTILITY_ARRAY_SIZE(Storage_Device_Lookup_Table); Device_Index++)
	{
		for (Location_Index = 0; Location_Index < UTILITY_ARRAY_SIZE(Storage_Location_Lookup_Table); Location_Index++)
		{
			Storage_Location = Storage_Location_Lookup_Table[Location_Index];
			Storage_Device = Storage_Device_Lookup_Table[Device_Index];
			if (MMSGetStorageInformation(Serial_Port_ID, Storage_Location, Storage_Device, &Messages_Count, String_Messages_Payload_Directory, String_Database_File) != 0) return -1;
			printf("Found %d message(s) in %s \"%s\" location.\n", Messages_Count, Pointer_Strings_Storage_Device_Names[Device_Index], Pointer_Strings_Storage_Location_Names[Location_Index]);

			// Nothing to do is no message is stored in this location
			if (Messages_Count == 0) continue;

			// Retrieve the database file
			if (FileManagerDownloadFile(Serial_Port_ID, String_Database_File, MMS_DATABASE_FILE_NAME) != 0)
			{
				LOG("Error : could not download the MMS database file \"%s\" (storage location = %d, storage device = %d).\n", String_Database_File, Storage_Location, Storage_Device);
				return -1;
			}

			// Try to open the database file
			File_Descriptor = open(MMS_DATABASE_FILE_NAME, O_RDONLY);
			if (File_Descriptor == -1)
			{
				LOG("Error : failed to open MMS database file \"%s\" (storage location = %d, storage device = %d, %s).\n", String_Database_File, Storage_Location, Storage_Device, strerror(errno));
				return -1;
			}

			// Extract each message information from the database
			for (i = 1; i <= Messages_Count; i++) // Start from 1, so the 'i ' value can be displayed as-is
			{
				// Retrieve next record
				if (read(File_Descriptor, &Database_Record, sizeof(Database_Record)) != sizeof(Database_Record))
				{
					LOG("Error : could not read MMS database record %d (database file = \"%s\", storage location = %d, storage device = %d, %s).\n", i, String_Database_File, Storage_Location, Storage_Device, strerror(errno));
					goto Exit;
				}

				// Retrieve the MMS file
				printf("Retrieving message %d/%d (%u bytes)...\n", i, Messages_Count, Database_Record.File_Size);
				sprintf(String_Temporary, "%s\\%s", String_Messages_Payload_Directory, Database_Record.String_File_Name);
				if (FileManagerDownloadFile(Serial_Port_ID, String_Temporary, MMS_RAW_MMS_FILE_NAME) != 0)
				{
					LOG("Error : could not download the MMS file \"%s\" (storage location = %d, storage device = %d).\n", String_Temporary, Storage_Location, Storage_Device);
					goto Exit;
				}

				// Extract payload from MMS
				sprintf(String_Temporary, "Output/MMS/%s", Pointer_Strings_Storage_Location_Names[Location_Index]);
				if (MMSProcessMessage(MMS_RAW_MMS_FILE_NAME, String_Temporary) != 0)
				{
					LOG("Error : could not process the MMS file \"%s\" (storage location = %d, storage device = %d).\n", Database_Record.String_File_Name, Storage_Location, Storage_Device);
					goto Exit;
				}
			}

			// Stop accessing the database file but do not remove it, as it will be overwritten by another one
			close(File_Descriptor);
			File_Descriptor = -1;
		}
	}

	// Everything went fine
	Return_Value = 0;

Exit:
	if (File_Descriptor != -1) close(File_Descriptor);
	unlink(MMS_DATABASE_FILE_NAME);
	unlink(MMS_RAW_MMS_FILE_NAME);

	return Return_Value;
}
