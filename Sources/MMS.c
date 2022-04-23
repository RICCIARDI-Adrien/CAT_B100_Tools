/** @file MMS.c
 * See MMS.h for description.
 * @author Adrien RICCIARDI
 */
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
#include <unistd.h>
#include <Utility.h>

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
		"inbox",
		"outbox",
		"sent",
		"drafts",
		"templates"
	};
	static char *Pointer_Strings_Storage_Device_Names[] =
	{
		"phone",
		"SD card"
	};
	int Messages_Count, File_Descriptor, i;
	unsigned int Location_Index, Device_Index;
	char String_Messages_Payload_Directory[128], String_Database_File[128];
	TMMSStorageLocation Storage_Location;
	TMMSStorageDevice Storage_Device;
	TMMSDatabaseRecord Database_Record;

	// Try all possible messages storage combinations
	for (Device_Index = 0; Device_Index < UTILITY_ARRAY_SIZE(Storage_Device_Lookup_Table); Device_Index++)
	{
		for (Location_Index = 0; Location_Index < UTILITY_ARRAY_SIZE(Storage_Location_Lookup_Table); Location_Index++)
		{
			Storage_Location = Storage_Location_Lookup_Table[Location_Index];
			Storage_Device = Storage_Device_Lookup_Table[Device_Index];
			if (MMSGetStorageInformation(Serial_Port_ID, Storage_Location, Storage_Device, &Messages_Count, String_Messages_Payload_Directory, String_Database_File) != 0) return -1;
			printf("Found %d message(s) in %s %s location.\n", Messages_Count, Pointer_Strings_Storage_Device_Names[Device_Index], Pointer_Strings_Storage_Location_Names[Location_Index]);

			// Nothing to do is no message is stored in this location
			if (Messages_Count == 0) continue;

			// Retrieve the database file
			if (FileManagerDownloadFile(Serial_Port_ID, String_Database_File, "Output/MMS/Database.db") != 0)
			{
				LOG("Error : could not download the MMS database file \"%s\" (storage location = %d, storage device = %d).\n", String_Database_File, Storage_Location, Storage_Device);
				return -1;
			}

			// Try to open the database file
			File_Descriptor = open("Output/MMS/Database.db", O_RDONLY);
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
					close(File_Descriptor);
					return -1;
				}
				printf("Retrieving message %d/%d (%u bytes)...\n", i, Messages_Count, Database_Record.File_Size);
				printf("file name = %s\n", Database_Record.String_File_Name); // TEST

				// TODO
			}

			close(File_Descriptor);
		}
	}

	return 0;
}
