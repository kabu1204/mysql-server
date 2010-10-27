/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef COPY_DATA_HPP
#define COPY_DATA_HPP

#include "SignalData.hpp"

struct CopyDataReq
{

  STATIC_CONST( SignalLength = 9 );

  enum RequestType {
    ReorgCopy = 0,
    ReorgDelete = 1
    // AlterTableCopy
  };

  union {
    Uint32 clientRef;
    Uint32 senderRef;
  };
  union {
    Uint32 clientData;
    Uint32 senderData;
  };
  Uint32 transKey;
  Uint32 transId;
  Uint32 requestType;
  Uint32 requestInfo;
  Uint32 srcTableId;
  Uint32 dstTableId;
  Uint32 srcFragments; // Only used for ReorgDelete
};

struct CopyDataConf
{

  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  Uint32 transId;
};

struct CopyDataRef
{
  STATIC_CONST( SignalLength = 9 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 transId;
  Uint32 masterNodeId;
  Uint32 errorNodeId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorKey;
  Uint32 errorStatus;
};

typedef CopyDataReq CopyDataImplReq;
typedef CopyDataRef CopyDataImplRef;
typedef CopyDataConf CopyDataImplConf;

#endif
