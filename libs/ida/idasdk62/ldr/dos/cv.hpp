/*
 *      Interactive disassembler (IDA).
 *      Copyright (c) 1990-98 by Ilfak Guilfanov.
 *      ALL RIGHTS RESERVED.
 *                              E-mail: ig@estar.msk.su
 *                              FIDO:   2:5020/209
 *
 */

#ifndef __CV_HPP
#define __CV_HPP
#pragma pack(push, 1)

//----------------------------------------------------------------------
//              Codeview debug data
//----------------------------------------------------------------------
#define CV_NB00 "NB00"  // Not supported.
#define CV_NB01 "NB01"  // Not supported.
#define CV_NB02 "NB02"  // CodeView 3
#define CV_NB03 "NB03"  // Not supported.
#define CV_NB04 "NB04"  // Not supported.
#define CV_NB05 "NB05"  // Emitted by LINK, version 5.20 and later linkers for
                        // a file before it has been packed.
#define CV_NB06 "NB06"  // Not supported.
#define CV_NB07 "NB07"  // Used for Quick C for Windows 1.0 only.
#define CV_NB08 "NB08"  // Used by Microsoft CodeView debugger, versions 4.00
                        // through 4.05, for a file after it has been packed.
                        // Microsoft CodeView, version 4.00 through 4.05 will
                        // not process a file that does not have this
                        // signature.
#define CV_NB09 "NB09"  // Used by Microsoft CodeView, version 4.10 for a file
                        // after it has been packed. Microsoft CodeView 4.10
                        // will not process a file that does not have this
                        // signature.
#define CV_NB10 "NB10"  // The signature for an executable with the debug
                        // information stored in a separate PDB file. Corresponds
                        // with the formats set forth in NB09 or NB11.
#define CV_NB11 "NB11"  // The signature for Visual C++ 5.0 debug information that
                        // has been packed and bonded to the executable. This
                        // includes all 32-bit type indices.


//----------------------------------------------------------------------
bool inline is_codeview_magic(const char *data)
{
  return strncmp(data,CV_NB02,4) == 0
      || strncmp(data,CV_NB05,4) == 0
      || strncmp(data,CV_NB08,4) == 0
      || strncmp(data,CV_NB09,4) == 0
      || strncmp(data,CV_NB11,4) == 0;
}


#pragma pack(pop)
#endif // define __CV_HPP
