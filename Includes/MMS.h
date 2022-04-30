/** @file MMS.h
 * Retrieve MMS text and attached files from phone.
 * MMS files decoding is based on the following specifications from the Open Mobile Alliance :
 * - OMA-TS-MMS_ENC-V1_3-20110913-A
 * - WAP-230-WSP-20010705-a
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
