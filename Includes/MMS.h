/** @file MMS.h
 * Retrieve MMS text and attached files from phone.
 * @author Adrien RICCIARDI
 */
#ifndef H_MMS_H
#define H_MMS_H

#include <Serial_Port.h>

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Download all MMS from the phone, then write them into the appropriate output files.
 * @param Serial_Port_ID The phone serial port.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int MMSDownloadAll(TSerialPortID Serial_Port_ID);

#endif
