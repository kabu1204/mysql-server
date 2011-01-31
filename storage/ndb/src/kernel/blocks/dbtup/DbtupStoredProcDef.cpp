/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#define DBTUP_C
#define DBTUP_STORE_PROC_DEF_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ------------ADD/DROP STORED PROCEDURE MODULE ------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execSTORED_PROCREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  jamEntry();
  regOperPtr.i = signal->theData[0];
  c_operation_pool.getPtr(regOperPtr);
  regTabPtr.i = signal->theData[1];
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  Uint32 requestInfo = signal->theData[3];
  TransState trans_state= get_trans_state(regOperPtr.p);
  ndbrequire(trans_state == TRANS_IDLE ||
             ((trans_state == TRANS_ERROR_WAIT_STORED_PROCREQ) &&
             (requestInfo == ZSTORED_PROCEDURE_DELETE)));
  ndbrequire(regTabPtr.p->tableStatus == DEFINED);
  /*
   * Also store count of procs called from non-API scans.
   * It can be done here since seize/release always succeeds.
   * The count is only used under -DERROR_INSERT via DUMP.
   */
  BlockReference apiBlockref = signal->theData[5];
  switch (requestInfo) {
  case ZSCAN_PROCEDURE:
  {
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
    storedProcCountNonAPI(apiBlockref, +1);
#endif
    SectionHandle handle(this, signal);
    ndbrequire(handle.m_cnt == 1);

    scanProcedure(signal,
                  regOperPtr.p,
                  &handle,
                  false); // Not copy
    break;
  }
  case ZCOPY_PROCEDURE:
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
    storedProcCountNonAPI(apiBlockref, +1);
#endif
    copyProcedure(signal, regTabPtr, regOperPtr.p);
    break;
  case ZSTORED_PROCEDURE_DELETE:
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
    storedProcCountNonAPI(apiBlockref, -1);
#endif
    deleteScanProcedure(signal, regOperPtr.p);
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dbtup::execSTORED_PROCREQ()

void Dbtup::storedProcCountNonAPI(BlockReference apiBlockref, int add_del)
{
  BlockNumber apiBlockno = refToBlock(apiBlockref);
  if (apiBlockno < MIN_API_BLOCK_NO) {
    ndbassert(blockToMain(apiBlockno) == BACKUP ||
              blockToMain(apiBlockno) == SUMA ||
              blockToMain(apiBlockno) == DBLQH);
    if (add_del == +1) {
      jam();
      c_storedProcCountNonAPI++;
    } else if (add_del == -1) {
      jam();
      ndbassert(c_storedProcCountNonAPI > 0);
      c_storedProcCountNonAPI--;
    } else {
      ndbassert(false);
    }
  }
}

void Dbtup::deleteScanProcedure(Signal* signal,
                                Operationrec* regOperPtr) 
{
  StoredProcPtr storedPtr;
  Uint32 storedProcId = signal->theData[4];
  c_storedProcPool.getPtr(storedPtr, storedProcId);
  ndbrequire(storedPtr.p->storedCode != ZSTORED_PROCEDURE_FREE);
  if (unlikely(storedPtr.p->storedCode == ZCOPY_PROCEDURE))
  {
    releaseCopyProcedure();
  }
  else
  {
    /* ZSCAN_PROCEDURE */
    releaseSection(storedPtr.p->storedProcIVal);
  }
  storedPtr.p->storedCode = ZSTORED_PROCEDURE_FREE;
  storedPtr.p->storedProcIVal= RNIL;
  c_storedProcPool.release(storedPtr);

  set_trans_state(regOperPtr, TRANS_IDLE);
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = storedProcId;
  BlockReference lqhRef = calcInstanceBlockRef(DBLQH);
  sendSignal(lqhRef, GSN_STORED_PROCCONF, signal, 2, JBB);
}//Dbtup::deleteScanProcedure()

void Dbtup::scanProcedure(Signal* signal,
                          Operationrec* regOperPtr,
                          SectionHandle* handle,
                          bool isCopy)
{
  /* Size a stored procedure record, and link the
   * stored procedure AttrInfo section from it
   */
  ndbrequire( handle->m_cnt == 1 );
  ndbrequire( handle->m_ptr[0].p->m_sz > 0 );

  StoredProcPtr storedPtr;
  c_storedProcPool.seize(storedPtr);
  ndbrequire(storedPtr.i != RNIL);
  storedPtr.p->storedCode = (isCopy)? ZCOPY_PROCEDURE : ZSCAN_PROCEDURE;
  Uint32 lenAttrInfo= handle->m_ptr[0].p->m_sz;
  storedPtr.p->storedProcIVal= handle->m_ptr[0].i;
  handle->clear();

  set_trans_state(regOperPtr, TRANS_IDLE);
  
  if (lenAttrInfo >= ZATTR_BUFFER_SIZE) { // yes ">="
    jam();
    // send REF and change state
    storedProcBufferSeizeErrorLab(signal, 
                                  regOperPtr,
                                  storedPtr.i,
                                  ZSTORED_TOO_MUCH_ATTRINFO_ERROR);
    return;
  }

  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = storedPtr.i;
  
  BlockReference lqhRef = calcInstanceBlockRef(DBLQH);
  sendSignal(lqhRef, GSN_STORED_PROCCONF, signal, 2, JBB);
}//Dbtup::scanProcedure()

void Dbtup::allocCopyProcedure()
{
  /* We allocate some segments and initialise them with
   * Attribute Ids for the 'worst case' table.
   * At run time we can use prefixes of this data.
   * 
   * TODO : Consider using read packed 'read all columns' word once
   * updatePacked supported.
   */
  Uint32 iVal= RNIL;
  Uint32 ahWord;

  for (Uint32 attrNum=0; attrNum < MAX_ATTRIBUTES_IN_TABLE; attrNum++)
  {
    AttributeHeader::init(&ahWord, attrNum, 0);
    ndbrequire(appendToSection(iVal, &ahWord, 1));
  }

  cCopyProcedure= iVal;
  cCopyLastSeg= RNIL;
}

void Dbtup::freeCopyProcedure()
{
  /* Should only be called when shutting down node.
   */
  releaseSection(cCopyProcedure);
  cCopyProcedure=RNIL;
}

void Dbtup::prepareCopyProcedure(Uint32 numAttrs)
{
  /* Set length of copy procedure section to the
   * number of attributes supplied
   */
  ndbassert(numAttrs <= MAX_ATTRIBUTES_IN_TABLE);
  ndbassert(cCopyProcedure != RNIL);
  ndbassert(cCopyLastSeg == RNIL);
  Ptr<SectionSegment> first;
  g_sectionSegmentPool.getPtr(first, cCopyProcedure);

  /* Record original 'last segment' of section */
  cCopyLastSeg= first.p->m_lastSegment;

  /* Modify section to represent relevant prefix 
   * of code by modifying size and lastSegment
   */
  first.p->m_sz= numAttrs;

  Ptr<SectionSegment> curr= first;  
  while(numAttrs > SectionSegment::DataLength)
  {
    g_sectionSegmentPool.getPtr(curr, curr.p->m_nextSegment);
    numAttrs-= SectionSegment::DataLength;
  }
  first.p->m_lastSegment= curr.i;
}

void Dbtup::releaseCopyProcedure()
{
  /* Return Copy Procedure section to original length */
  ndbassert(cCopyProcedure != RNIL);
  ndbassert(cCopyLastSeg != RNIL);
  
  Ptr<SectionSegment> first;
  g_sectionSegmentPool.getPtr(first, cCopyProcedure);
  
  ndbassert(first.p->m_sz <= MAX_ATTRIBUTES_IN_TABLE);
  first.p->m_sz= MAX_ATTRIBUTES_IN_TABLE;
  first.p->m_lastSegment= cCopyLastSeg;
  
  cCopyLastSeg= RNIL;
}
  

void Dbtup::copyProcedure(Signal* signal,
                          TablerecPtr regTabPtr,
                          Operationrec* regOperPtr) 
{
  /* We create a stored procedure for the fragment copy scan
   * This is done by trimming a 'read all columns in order'
   * program to the correct length for this table and
   * using that to create the procedure
   * This assumes that there is only one fragment copy going
   * on at any time, which is verified by checking 
   * cCopyLastSeg == RNIL before starting each copy
   */
  prepareCopyProcedure(regTabPtr.p->m_no_of_attributes);

  SectionHandle handle(this);
  handle.m_cnt=1;
  handle.m_ptr[0].i= cCopyProcedure;
  getSections(handle.m_cnt, handle.m_ptr);

  scanProcedure(signal,
                regOperPtr,
                &handle,
                true); // isCopy
}//Dbtup::copyProcedure()

void Dbtup::storedProcBufferSeizeErrorLab(Signal* signal,
                                          Operationrec* regOperPtr,
                                          Uint32 storedProcPtr,
                                          Uint32 errorCode)
{
  regOperPtr->m_any_value = 0;
  set_trans_state(regOperPtr, TRANS_ERROR_WAIT_STORED_PROCREQ);
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = errorCode;
  signal->theData[2] = storedProcPtr;
  BlockReference lqhRef = calcInstanceBlockRef(DBLQH);
  sendSignal(lqhRef, GSN_STORED_PROCREF, signal, 3, JBB);
}//Dbtup::storedSeizeAttrinbufrecErrorLab()

