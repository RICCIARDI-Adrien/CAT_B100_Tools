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
/** TODO */
int SMSDownload(TSerialPortID Serial_Port_ID);

#endif
