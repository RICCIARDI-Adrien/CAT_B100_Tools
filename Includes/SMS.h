/** @file SMS.h
 * Retrieve all possible SMS messages from phone.
 * @author Adrien RICCIARDI
 */
#ifndef H_SMS_H
#define H_SMS_H

#include <Serial_Port.h>

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Download all SMS from the phone, then write them into the appropriate output files.
 * @param Serial_Port_ID The phone serial port.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int SMSDownloadAll(TSerialPortID Serial_Port_ID);

#endif
