/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

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
#define DBTUP_TRIGGER_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/TuxMaint.hpp>

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ----------------------- TRIGGER HANDLING ----------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

DLList<Dbtup::TupTriggerData>*
Dbtup::findTriggerList(Tablerec* table,
                       TriggerType::Value ttype,
                       TriggerActionTime::Value ttime,
                       TriggerEvent::Value tevent)
{
  DLList<TupTriggerData>* tlist = NULL;
  switch (ttype) {
  case TriggerType::SUBSCRIPTION:
  case TriggerType::SUBSCRIPTION_BEFORE:
    switch (tevent) {
    case TriggerEvent::TE_INSERT:
      jam();
      if (ttime == TriggerActionTime::TA_DETACHED)
        tlist = &table->subscriptionInsertTriggers;
      break;
    case TriggerEvent::TE_UPDATE:
      jam();
      if (ttime == TriggerActionTime::TA_DETACHED)
        tlist = &table->subscriptionUpdateTriggers;
      break;
    case TriggerEvent::TE_DELETE:
      jam();
      if (ttime == TriggerActionTime::TA_DETACHED)
        tlist = &table->subscriptionDeleteTriggers;
      break;
    default:
      break;
    }
    break;
  case TriggerType::SECONDARY_INDEX:
  case TriggerType::REORG_TRIGGER:
    switch (tevent) {
    case TriggerEvent::TE_INSERT:
      jam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->afterInsertTriggers;
      break;
    case TriggerEvent::TE_UPDATE:
      jam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->afterUpdateTriggers;
      break;
    case TriggerEvent::TE_DELETE:
      jam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->afterDeleteTriggers;
      break;
    default:
      break;
    }
    break;
  case TriggerType::ORDERED_INDEX:
    switch (tevent) {
    case TriggerEvent::TE_CUSTOM:
      jam();
      if (ttime == TriggerActionTime::TA_CUSTOM)
        tlist = &table->tuxCustomTriggers;
      break;
    default:
      break;
    }
    break;
  case TriggerType::READ_ONLY_CONSTRAINT:
    switch (tevent) {
    case TriggerEvent::TE_UPDATE:
      jam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->constraintUpdateTriggers;
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  return tlist;
}

// Trigger signals
void
Dbtup::execCREATE_TRIG_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const CreateTrigImplReq* req = (const CreateTrigImplReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableId = req->tableId;
  const Uint32 triggerId = req->triggerId;
  const Uint32 triggerInfo = req->triggerInfo;

  CreateTrigRef::ErrorCode error = CreateTrigRef::NoError;

  // Find table
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  if (tabPtr.p->tableStatus != DEFINED )
  {
    jam();
    error = CreateTrigRef::InvalidTable;
  }
  // Create trigger and associate it with the table
  else if (createTrigger(tabPtr.p, req))
  {
    jam();
    // Send conf
    CreateTrigImplConf* conf = (CreateTrigImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->tableId = tableId;
    conf->triggerId = triggerId;
    conf->triggerInfo = triggerInfo;

    sendSignal(senderRef, GSN_CREATE_TRIG_IMPL_CONF, 
               signal, CreateTrigImplConf::SignalLength, JBB);
    return;
  }
  else
  {
    jam();
    error = CreateTrigRef::TooManyTriggers;
  }

  ndbassert(error != CreateTrigRef::NoError);
  // Send ref
  CreateTrigImplRef* ref = (CreateTrigImplRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = senderData;
  ref->tableId = tableId;
  ref->triggerId = triggerId;
  ref->triggerInfo = triggerInfo;
  ref->errorCode = error;

  sendSignal(senderRef, GSN_CREATE_TRIG_IMPL_REF, 
	     signal, CreateTrigImplRef::SignalLength, JBB);
}

void
Dbtup::execDROP_TRIG_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const DropTrigImplReq* req = (const DropTrigImplReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableId = req->tableId;
  const Uint32 triggerId = req->triggerId;
  const Uint32 receiverRef = req->receiverRef;

  // Find table
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  // Drop trigger
  Uint32 r = dropTrigger(tabPtr.p, req, refToBlock(receiverRef));
  if (r == 0){
    // Send conf
    DropTrigImplConf* conf = (DropTrigImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->tableId = tableId;
    conf->triggerId = triggerId;

    sendSignal(senderRef, GSN_DROP_TRIG_IMPL_CONF, 
	       signal, DropTrigImplConf::SignalLength, JBB);
  } else {
    // Send ref
    DropTrigImplRef* ref = (DropTrigImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tableId;
    ref->triggerId = triggerId;
    ref->errorCode = r;
    sendSignal(senderRef, GSN_DROP_TRIG_IMPL_REF, 
	       signal, DropTrigImplRef::SignalLength, JBB);
  }
}

/* ---------------------------------------------------------------- */
/* ------------------------- createTrigger ------------------------ */
/*                                                                  */
/* Creates a new trigger record by fetching one from the trigger    */
/* pool and associates it with the given table.                     */
/* Trigger type can be one of secondary_index, subscription,        */
/* constraint(NYI), foreign_key(NYI), schema_upgrade(NYI),          */
/* api_trigger(NYI) or sql_trigger(NYI).                            */   
/* Note that this method only checks for total number of allowed    */
/* triggers. Checking the number of allowed triggers per table is   */
/* done by TRIX.                                                    */
/*                                                                  */
/* ---------------------------------------------------------------- */
bool
Dbtup::createTrigger(Tablerec* table,
                     const CreateTrigImplReq* req)
{
  if (ERROR_INSERTED(4003)) {
    CLEAR_ERROR_INSERT_VALUE;
    return false;
  }

  const Uint32 tinfo = req->triggerInfo;
  TriggerType::Value ttype = TriggerInfo::getTriggerType(tinfo);
  TriggerActionTime::Value ttime = TriggerInfo::getTriggerActionTime(tinfo);
  TriggerEvent::Value tevent = TriggerInfo::getTriggerEvent(tinfo);

  int cnt;
  struct {
    TriggerEvent::Value event;
    DLList<TupTriggerData> * list;
    TriggerPtr ptr;
  } tmp[3];

  if (ttype == TriggerType::SECONDARY_INDEX ||
      ttype == TriggerType::REORG_TRIGGER)
  {
    jam();
    cnt = 3;
    tmp[0].event = TriggerEvent::TE_INSERT;
    tmp[1].event = TriggerEvent::TE_UPDATE;
    tmp[2].event = TriggerEvent::TE_DELETE;
  }
  else
  {
    jam();
    cnt = 1;
    tmp[0].event = tevent;
  }

  int i = 0;
  for (i = 0; i<cnt; i++)
  {
    tmp[i].list = findTriggerList(table, ttype, ttime, tmp[i].event);
    ndbrequire(tmp[i].list != NULL);

    TriggerPtr tptr;
    if (!tmp[i].list->seize(tptr))
    {
      jam();
      goto err;
    }

    tmp[i].ptr = tptr;

    // Set trigger id
    tptr.p->triggerId = req->triggerId;
    tptr.p->oldTriggerIds[0] = req->upgradeExtra[0];
    tptr.p->oldTriggerIds[1] = req->upgradeExtra[1];
    tptr.p->oldTriggerIds[2] = req->upgradeExtra[2];

    // Set index id
    tptr.p->indexId = req->indexId;

    // Set trigger type etc
    tptr.p->triggerType = ttype;
    tptr.p->triggerActionTime = ttime;
    tptr.p->triggerEvent = tevent;

    tptr.p->sendBeforeValues = true;
    if ((tptr.p->triggerType == TriggerType::SUBSCRIPTION) &&
	((tptr.p->triggerEvent == TriggerEvent::TE_UPDATE) ||
	 (tptr.p->triggerEvent == TriggerEvent::TE_DELETE))) {
      jam();
      tptr.p->sendBeforeValues = false;
    }

    if (ttype == TriggerType::REORG_TRIGGER)
    {
      jam();
      tptr.p->sendBeforeValues = false;
    }

    /*
      tptr.p->sendOnlyChangedAttributes = false;
      if (((tptr.p->triggerType == TriggerType::SUBSCRIPTION) ||
      (tptr.p->triggerType == TriggerType::SUBSCRIPTION_BEFORE)) &&
      (tptr.p->triggerEvent == TriggerEvent::TE_UPDATE)) {
      jam();
      tptr.p->sendOnlyChangedAttributes = true;
      }
    */
    tptr.p->sendOnlyChangedAttributes =
      !TriggerInfo::getReportAllMonitoredAttributes(tinfo);

    tptr.p->monitorAllAttributes = TriggerInfo::getMonitorAllAttributes(tinfo);
    tptr.p->monitorReplicas = TriggerInfo::getMonitorReplicas(tinfo);
    tptr.p->m_receiverRef = req->receiverRef;

    if (tptr.p->monitorAllAttributes)
    {
      jam();
      // Set all non-pk attributes
      tptr.p->attributeMask.set();
      for(Uint32 i = 0; i < table->m_no_of_attributes; i++) {
	if (primaryKey(table, i))
	  tptr.p->attributeMask.clear(i);
      }
    }
    else
    {
      jam();
      // Set attribute mask
      tptr.p->attributeMask = req->attributeMask;
    }
  }
  return true;

err:
  for (--i; i >= 0; i--)
  {
    jam();
    tmp[i].list->release(tmp[i].ptr);
  }
  return false;
}//Dbtup::createTrigger()

bool
Dbtup::primaryKey(Tablerec* const regTabPtr, Uint32 attrId)
{
  Uint32 attrDescriptorStart = regTabPtr->tabDescriptor;
  Uint32 attrDescriptor = getTabDescrWord(attrDescriptorStart +
                                          (attrId * ZAD_SIZE));
  return (bool)AttributeDescriptor::getPrimaryKey(attrDescriptor);
}//Dbtup::primaryKey()

/* ---------------------------------------------------------------- */
/* -------------------------- dropTrigger ------------------------- */
/*                                                                  */
/* Deletes a trigger record by disassociating it with the given     */
/* table and returning it to the trigger pool.                      */
/* Trigger type can be one of secondary_index, subscription,        */
/* constraint(NYI), foreign_key(NYI), schema_upgrade(NYI),          */
/* api_trigger(NYI) or sql_trigger(NYI).                            */   
/*                                                                  */
/* ---------------------------------------------------------------- */
Uint32
Dbtup::dropTrigger(Tablerec* table, const DropTrigImplReq* req, BlockNumber receiver)
{
  if (ERROR_INSERTED(4004)) {
    CLEAR_ERROR_INSERT_VALUE;
    return 9999;
  }
  Uint32 triggerId = req->triggerId;

  const Uint32 tinfo = req->triggerInfo;
  TriggerType::Value ttype = TriggerInfo::getTriggerType(tinfo);
  TriggerActionTime::Value ttime = TriggerInfo::getTriggerActionTime(tinfo);
  TriggerEvent::Value tevent = TriggerInfo::getTriggerEvent(tinfo);

  //  ndbout_c("Drop TupTrigger %u = %u %u %u %u by %u", triggerId, table, ttype, ttime, tevent, sender);

  int cnt;
  struct {
    TriggerEvent::Value event;
    DLList<TupTriggerData> * list;
    TriggerPtr ptr;
  } tmp[3];

  if (ttype == TriggerType::SECONDARY_INDEX ||
      ttype == TriggerType::REORG_TRIGGER)
  {
    jam();
    cnt = 3;
    tmp[0].event = TriggerEvent::TE_INSERT;
    tmp[1].event = TriggerEvent::TE_UPDATE;
    tmp[2].event = TriggerEvent::TE_DELETE;
  }
  else
  {
    jam();
    cnt = 1;
    tmp[0].event = tevent;
  }

  int i = 0;
  for (i = 0; i<cnt; i++)
  {
    tmp[i].list = findTriggerList(table, ttype, ttime, tmp[i].event);
    ndbrequire(tmp[i].list != NULL);

    Ptr<TupTriggerData> ptr;
    tmp[i].ptr.setNull();
    for (tmp[i].list->first(ptr); !ptr.isNull(); tmp[i].list->next(ptr))
    {
      jam();
      if (ptr.p->triggerId == triggerId)
      {
	if(ttype==TriggerType::SUBSCRIPTION &&
	   receiver != refToBlock(ptr.p->m_receiverRef))
	{
	  /**
	   * You can only drop your own triggers for subscription triggers.
	   * Trigger IDs are private for each block.
	   *
	   * SUMA encodes information in the triggerId
	   *
	   * Backup doesn't really care about the Ids though.
	   */
	  jam();
	  continue;
	}
	jam();
	tmp[i].ptr = ptr;
      }
    }
    if (tmp[i].ptr.isNull())
    {
      jam();
      return DropTrigRef::TriggerNotFound;
    }
  }

  for (i = 0; i<cnt; i++)
  {
    jam();
    tmp[i].list->release(tmp[i].ptr);
  }
  return 0;
}//Dbtup::dropTrigger()

/* ---------------------------------------------------------------- */
/* -------------- checkImmediateTriggersAfterOp ------------------ */
/*                                                                  */
/* Called after an insert, delete, or update operation takes        */
/* place. Fetches before tuple for deletes and updates and          */
/* after tuple for inserts and updates.                             */
/* Executes immediate triggers by sending FIRETRIGORD               */
/*                                                                  */
/* ---------------------------------------------------------------- */
void
Dbtup::checkImmediateTriggersAfterInsert(KeyReqStruct *req_struct,
                                         Operationrec *regOperPtr, 
                                         Tablerec *regTablePtr,
                                         bool disk)
{
  if(refToBlock(req_struct->TC_ref) != DBTC) {
    return;
  }

  if ((regOperPtr->op_struct.primary_replica) &&
      (!(regTablePtr->afterInsertTriggers.isEmpty()))) {
    jam();
    fireImmediateTriggers(req_struct,
                          regTablePtr->afterInsertTriggers,
                          regOperPtr,
                          disk);
  }
}

void
Dbtup::checkImmediateTriggersAfterUpdate(KeyReqStruct *req_struct,
                                         Operationrec* regOperPtr, 
                                         Tablerec* regTablePtr,
                                         bool disk)
{
  if(refToBlock(req_struct->TC_ref) != DBTC) {
    return;
  }

  if ((regOperPtr->op_struct.primary_replica) &&
      (!(regTablePtr->afterUpdateTriggers.isEmpty()))) {
    jam();
    fireImmediateTriggers(req_struct,
                          regTablePtr->afterUpdateTriggers,
                          regOperPtr,
                          disk);
  }
  if ((regOperPtr->op_struct.primary_replica) &&
      (!(regTablePtr->constraintUpdateTriggers.isEmpty()))) {
    jam();
    fireImmediateTriggers(req_struct,
                          regTablePtr->constraintUpdateTriggers,
                          regOperPtr,
                          disk);
  }
}

void
Dbtup::checkImmediateTriggersAfterDelete(KeyReqStruct *req_struct,
                                         Operationrec* regOperPtr, 
                                         Tablerec* regTablePtr,
                                         bool disk)
{
  if(refToBlock(req_struct->TC_ref) != DBTC) {
    return;
  }

  if ((regOperPtr->op_struct.primary_replica) &&
      (!(regTablePtr->afterDeleteTriggers.isEmpty()))) {
    jam();
    executeTriggers(req_struct,
                    regTablePtr->afterDeleteTriggers,
                    regOperPtr,
                    disk);
  }
}

#if 0
/* ---------------------------------------------------------------- */
/* --------------------- checkDeferredTriggers -------------------- */
/*                                                                  */
/* Called before commit after an insert, delete, or update          */
/* operation. Fetches before tuple for deletes and updates and      */
/* after tuple for inserts and updates.                             */
/* Executes deferred triggers by sending FIRETRIGORD                */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkDeferredTriggers(Signal* signal, 
                                  Operationrec* const regOperPtr,
                                  Tablerec* const regTablePtr)
{
  jam();
  // NYI
}//Dbtup::checkDeferredTriggers()
#endif

/* ---------------------------------------------------------------- */
/* --------------------- checkDetachedTriggers -------------------- */
/*                                                                  */
/* Called at commit after an insert, delete, or update operation.   */
/* Fetches before tuple for deletes and updates and                 */
/* after tuple for inserts and updates.                             */
/* Executes detached triggers by sending FIRETRIGORD                */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkDetachedTriggers(KeyReqStruct *req_struct,
                                  Operationrec* regOperPtr,
                                  Tablerec* regTablePtr,
                                  bool disk)
{
  Uint32 save_type = regOperPtr->op_struct.op_type;
  Tuple_header *save_ptr = req_struct->m_tuple_ptr;  

  switch (save_type) {
  case ZUPDATE:
  case ZINSERT:
    req_struct->m_tuple_ptr =
      get_copy_tuple(regTablePtr, &regOperPtr->m_copy_tuple_location);
    break;
  }

  /**
   * Set correct operation type and fix change mask
   * Note ALLOC is set in "orig" tuple
   */
  if (save_ptr->m_header_bits & Tuple_header::ALLOC) {
    if (save_type == ZDELETE) {
      // insert + delete = nothing
      jam();
      return;
      goto end;
    }
    regOperPtr->op_struct.op_type = ZINSERT;
  }
  else if (save_type == ZINSERT) {
    /**
     * Tuple was not created but last op is INSERT.
     * This is possible only on DELETE + INSERT
     */
    regOperPtr->op_struct.op_type = ZUPDATE;
  }
  
  switch(regOperPtr->op_struct.op_type) {
  case(ZINSERT):
    jam();
    if (regTablePtr->subscriptionInsertTriggers.isEmpty()) {
      // Table has no active triggers monitoring inserts at commit
      jam();
      goto end;
    }

    // If any fired immediate insert trigger then fetch after tuple
    fireDetachedTriggers(req_struct,
                         regTablePtr->subscriptionInsertTriggers, 
                         regOperPtr, disk);
    break;
  case(ZDELETE):
    jam();
    if (regTablePtr->subscriptionDeleteTriggers.isEmpty()) {
      // Table has no active triggers monitoring deletes at commit
      jam();
      goto end;
    }

    // Execute any after delete triggers by sending 
    // FIRETRIGORD with the before tuple
    fireDetachedTriggers(req_struct,
			 regTablePtr->subscriptionDeleteTriggers, 
			 regOperPtr, disk);
    break;
  case(ZUPDATE):
    jam();
    if (regTablePtr->subscriptionUpdateTriggers.isEmpty()) {
      // Table has no active triggers monitoring updates at commit
      jam();
      goto end;
    }

    // If any fired immediate update trigger then fetch after tuple
    // and send two FIRETRIGORD one with before tuple and one with after tuple
    fireDetachedTriggers(req_struct,
                         regTablePtr->subscriptionUpdateTriggers, 
                         regOperPtr, disk);
    break;
  default:
    ndbrequire(false);
    break;
  }

end:
  regOperPtr->op_struct.op_type = save_type;
  req_struct->m_tuple_ptr = save_ptr;
}

void 
Dbtup::fireImmediateTriggers(KeyReqStruct *req_struct,
                             DLList<TupTriggerData>& triggerList, 
                             Operationrec* const regOperPtr,
                             bool disk)
{
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(req_struct->changeMask)) {
      jam();
      executeTrigger(req_struct,
                     trigPtr.p,
                     regOperPtr,
                     disk);
    }//if
    triggerList.next(trigPtr);
  }//while
}//Dbtup::fireImmediateTriggers()

#if 0
void 
Dbtup::fireDeferredTriggers(Signal* signal,
                            KeyReqStruct *req_struct,
                            DLList<TupTriggerData>& triggerList, 
                            Operationrec* const regOperPtr)
{
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(req_struct->changeMask)) {
      jam();
      executeTrigger(req_struct,
                     trigPtr,
                     regOperPtr);
    }//if
    triggerList.next(trigPtr);
  }//while
}//Dbtup::fireDeferredTriggers()
#endif

void 
Dbtup::fireDetachedTriggers(KeyReqStruct *req_struct,
                            DLList<TupTriggerData>& triggerList, 
                            Operationrec* const regOperPtr,
                            bool disk)
{
  
  TriggerPtr trigPtr;  
  
  /**
   * Set disk page
   */
  req_struct->m_disk_page_ptr.i = m_pgman_ptr.i;
  
  ndbrequire(regOperPtr->is_first_operation());
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if ((trigPtr.p->monitorReplicas ||
         regOperPtr->op_struct.primary_replica) &&
        (trigPtr.p->monitorAllAttributes ||
         trigPtr.p->attributeMask.overlaps(req_struct->changeMask))) {
      jam();
      executeTrigger(req_struct,
                     trigPtr.p,
                     regOperPtr,
                     disk);
    }
    triggerList.next(trigPtr);
  }
}

void Dbtup::executeTriggers(KeyReqStruct *req_struct,
                            DLList<TupTriggerData>& triggerList, 
                            Operationrec* regOperPtr,
                            bool disk)
{
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    executeTrigger(req_struct,
                   trigPtr.p,
                   regOperPtr,
                   disk);
    triggerList.next(trigPtr);

  }
}

bool
Dbtup::check_fire_trigger(const Fragrecord * fragPtrP,
                          const TupTriggerData* trigPtrP,
                          const KeyReqStruct * req_struct,
                          const Operationrec * regOperPtr) const
{
  jam();

  if (trigPtrP->triggerType == TriggerType::SUBSCRIPTION_BEFORE)
  {
    if (!check_fire_suma(req_struct, regOperPtr, fragPtrP))
      return false;
    return true;
  }

  switch(fragPtrP->fragStatus){
  case Fragrecord::FS_REORG_NEW:
    jam();
    return false;
  case Fragrecord::FS_REORG_COMMIT:
  case Fragrecord::FS_REORG_COMPLETE:
    return req_struct->m_reorg == 0;
  default:
    return true;
  }
}

bool
Dbtup::check_fire_reorg(const KeyReqStruct *req_struct,
                        Fragrecord::FragState state) const
{
  Uint32 flag = req_struct->m_reorg;
  switch(state){
  case Fragrecord::FS_ONLINE:
  case Fragrecord::FS_REORG_COMMIT_NEW:
  case Fragrecord::FS_REORG_COMPLETE_NEW:
    jam();
    if (flag == 2)
    {
      jam();
      return true;
    }
    return false;
  case Fragrecord::FS_REORG_NEW:
  case Fragrecord::FS_REORG_COMMIT:
  case Fragrecord::FS_REORG_COMPLETE:
  default:
    jam();
    return false;
  }
}

bool
Dbtup::check_fire_suma(const KeyReqStruct *req_struct,
                       const Operationrec* opPtrP,
                       const Fragrecord* regFragPtrP) const
{
  Ptr<Tablerec> tablePtr;
  tablePtr.i = regFragPtrP->fragTableId;
  Fragrecord::FragState state = regFragPtrP->fragStatus;
  Uint32 gci_hi = req_struct->gci_hi;
  Uint32 flag = opPtrP->op_struct.m_reorg;

  switch(state){
  case Fragrecord::FS_FREE:
    ndbassert(false);
    return false;
  case Fragrecord::FS_ONLINE:
    jam();
    return true;
  case Fragrecord::FS_REORG_NEW:
    jam();
    return false;
  case Fragrecord::FS_REORG_COMMIT_NEW:
    jam();
    return false;
  case Fragrecord::FS_REORG_COMPLETE_NEW:
    jam();
    return true;
  case Fragrecord::FS_REORG_COMMIT:
    jam();
    return true;
  case Fragrecord::FS_REORG_COMPLETE:
    jam();
    if (flag != 1)
    {
      jam();
      return true;
    }
    break;
  }

  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  if (gci_hi < tablePtr.p->m_reorg_suma_filter.m_gci_hi)
  {
    jam();
    return true;
  }

  return false;
}

Uint32
Dbtup::getOldTriggerId(const TupTriggerData* trigPtrP,
                       Uint32 op_type)
{
  switch(op_type){
  case ZINSERT:
    return trigPtrP->oldTriggerIds[0];
  case ZUPDATE:
    return trigPtrP->oldTriggerIds[1];
  case ZDELETE:
    return trigPtrP->oldTriggerIds[2];
  }
  ndbrequire(false);
  return RNIL;
}

void Dbtup::executeTrigger(KeyReqStruct *req_struct,
                           TupTriggerData* const trigPtr,
                           Operationrec* const regOperPtr,
                           bool disk)
{
  Signal* signal= req_struct->signal;
  BlockReference ref = trigPtr->m_receiverRef;
  Uint32* const keyBuffer = &cinBuffer[0];
  Uint32* const afterBuffer = &coutBuffer[0];
  Uint32* const beforeBuffer = &clogMemBuffer[0];
  Uint32 triggerType = trigPtr->triggerType;

  Uint32 noPrimKey, noAfterWords, noBeforeWords;
  FragrecordPtr regFragPtr;
  regFragPtr.i= regOperPtr->fragmentPtr;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  Fragrecord::FragState fragstatus = regFragPtr.p->fragStatus;

  if (refToMain(ref) == BACKUP)
  {
    jam();
    if (isNdbMtLqh())
    {
      goto out;
    }

    /*
    In order for the implementation of BACKUP to work even when changing
    primaries in the middle of the backup we need to set the trigger on
    all replicas. This check checks whether this is the node where this
    trigger should be fired. The check should preferably have been put
    completely in the BACKUP block but it was about five times simpler
    to put it here and also much faster for the backup (small overhead
    for everybody else.
    */
    signal->theData[0] = trigPtr->triggerId;
    signal->theData[1] = regFragPtr.p->fragmentId;
    EXECUTE_DIRECT(BACKUP, GSN_BACKUP_TRIG_REQ, signal, 2);
    jamEntry();
    if (signal->theData[0] == 0) {
      jam();
      return;
    }
out:
    (void)1;
  }
  else if (unlikely(triggerType == TriggerType::REORG_TRIGGER))
  {
    if (!check_fire_reorg(req_struct, fragstatus))
      return;
  }
  else if (unlikely(regFragPtr.p->fragStatus != Fragrecord::FS_ONLINE))
  {
    if (!check_fire_trigger(regFragPtr.p, trigPtr, req_struct, regOperPtr))
      return;
  }

  if (!readTriggerInfo(trigPtr,
                       regOperPtr,
                       req_struct,
                       regFragPtr.p,
                       keyBuffer,
                       noPrimKey,
                       afterBuffer,
                       noAfterWords,
                       beforeBuffer,
                       noBeforeWords,
                       disk)) {
    jam();
    return;
  }
//--------------------------------------------------------------------
// Now all data for this trigger has been read. It is now time to send
// the trigger information consisting of two or three sets of TRIG_
// ATTRINFO signals and one FIRE_TRIG_ORD signal.
// We start by setting common header info for all TRIG_ATTRINFO signals.
//--------------------------------------------------------------------
  bool executeDirect;
  bool longsignal = false;
  Uint32 triggerId = trigPtr->triggerId;
  TrigAttrInfo* const trigAttrInfo = (TrigAttrInfo *)signal->getDataPtrSend();
  trigAttrInfo->setConnectionPtr(req_struct->TC_index);
  trigAttrInfo->setTriggerId(trigPtr->triggerId);

  switch(triggerType) {
  case (TriggerType::SECONDARY_INDEX):
  {
    jam();
    /**
     * Handle stupid 6.3 which uses one triggerId per operation type
     */
    Uint32 node = refToNode(req_struct->TC_ref);
    if (unlikely(node && getNodeInfo(node).m_version < MAKE_VERSION(6,4,0)))
    {
      jam();
      triggerId = getOldTriggerId(trigPtr, regOperPtr->op_struct.op_type);
      trigAttrInfo->setTriggerId(triggerId);
    }
    // fall-through
  }
  case (TriggerType::REORG_TRIGGER):
    jam();
    ref = req_struct->TC_ref;
    executeDirect = false;
    break;
  case (TriggerType::SUBSCRIPTION):
  case (TriggerType::SUBSCRIPTION_BEFORE):
    jam();
    // Since only backup uses subscription triggers we send to backup directly for now
    ref = trigPtr->m_receiverRef;
    // executeDirect = !isNdbMtLqh() || (refToMain(ref) != SUMA);
    executeDirect = refToInstance(ref) == instance();

    // If we can do execute direct, lets do that, else do long signal (only local node)
    longsignal = !executeDirect;
    ndbassert(refToNode(ref) == 0 || refToNode(ref) == getOwnNodeId());
    break;
  case (TriggerType::READ_ONLY_CONSTRAINT):
    terrorCode = ZREAD_ONLY_CONSTRAINT_VIOLATION;
    // XXX should return status and abort the rest
    return;
  default:
    ndbrequire(false);
    executeDirect= false; // remove warning
  }//switch

  req_struct->no_fired_triggers++;

  if (longsignal == false)
  {
    jam();

    trigAttrInfo->setAttrInfoType(TrigAttrInfo::PRIMARY_KEY);
    sendTrigAttrInfo(signal, keyBuffer, noPrimKey, executeDirect, ref);

    switch(regOperPtr->op_struct.op_type) {
    case(ZINSERT):
      jam();
      // Send AttrInfo signals with new attribute values
      trigAttrInfo->setAttrInfoType(TrigAttrInfo::AFTER_VALUES);
      sendTrigAttrInfo(signal, afterBuffer, noAfterWords, executeDirect, ref);
      break;
    case(ZDELETE):
      if (trigPtr->sendBeforeValues) {
        jam();
        trigAttrInfo->setAttrInfoType(TrigAttrInfo::BEFORE_VALUES);
        sendTrigAttrInfo(signal, beforeBuffer, noBeforeWords, executeDirect,ref);
      }
      break;
    case(ZUPDATE):
      jam();
      if (trigPtr->sendBeforeValues) {
        jam();
        trigAttrInfo->setAttrInfoType(TrigAttrInfo::BEFORE_VALUES);
        sendTrigAttrInfo(signal, beforeBuffer, noBeforeWords, executeDirect,ref);
      }
      trigAttrInfo->setAttrInfoType(TrigAttrInfo::AFTER_VALUES);
      sendTrigAttrInfo(signal, afterBuffer, noAfterWords, executeDirect, ref);
      break;
    default:
      ndbrequire(false);
    }
  }

  /**
   * sendFireTrigOrd
   */
  FireTrigOrd* const fireTrigOrd = (FireTrigOrd *)signal->getDataPtrSend();

  fireTrigOrd->setConnectionPtr(req_struct->TC_index);
  fireTrigOrd->setTriggerId(triggerId);
  fireTrigOrd->fragId= regFragPtr.p->fragmentId;

  switch(regOperPtr->op_struct.op_type) {
  case(ZINSERT):
    jam();
    fireTrigOrd->m_triggerEvent = TriggerEvent::TE_INSERT;
    break;
  case(ZUPDATE):
    jam();
    fireTrigOrd->m_triggerEvent = TriggerEvent::TE_UPDATE;
    break;
  case(ZDELETE):
    jam();
    fireTrigOrd->m_triggerEvent = TriggerEvent::TE_DELETE;
    break;
  default:
    ndbrequire(false);
    break;
  }

  fireTrigOrd->setNoOfPrimaryKeyWords(noPrimKey);
  fireTrigOrd->setNoOfBeforeValueWords(noBeforeWords);
  fireTrigOrd->setNoOfAfterValueWords(noAfterWords);

  switch(trigPtr->triggerType) {
  case (TriggerType::SECONDARY_INDEX):
  case (TriggerType::REORG_TRIGGER):
    jam();
    fireTrigOrd->m_triggerType = trigPtr->triggerType;
    fireTrigOrd->m_transId1 = req_struct->trans_id1;
    fireTrigOrd->m_transId2 = req_struct->trans_id2;
    sendSignal(req_struct->TC_ref, GSN_FIRE_TRIG_ORD,
               signal, FireTrigOrd::SignalLength, JBB);
    break;
  case (TriggerType::SUBSCRIPTION_BEFORE): // Only Suma
    jam();
    fireTrigOrd->setGCI(req_struct->gci_hi);
    fireTrigOrd->setHashValue(req_struct->hash_value);
    fireTrigOrd->m_any_value = regOperPtr->m_any_value;
    fireTrigOrd->m_gci_lo = req_struct->gci_lo;
    if (executeDirect)
    {
      jam();
      EXECUTE_DIRECT(refToMain(trigPtr->m_receiverRef),
                     GSN_FIRE_TRIG_ORD,
                     signal,
                     FireTrigOrd::SignalLengthSuma);
      jamEntry();
    }
    else
    {
      ndbassert(longsignal);
      LinearSectionPtr ptr[3];
      ptr[0].p = keyBuffer;
      ptr[0].sz = noPrimKey;
      ptr[1].p = beforeBuffer;
      ptr[1].sz = noBeforeWords;
      ptr[2].p = afterBuffer;
      ptr[2].sz = noAfterWords;
      sendSignal(trigPtr->m_receiverRef, GSN_FIRE_TRIG_ORD,
                 signal, FireTrigOrd::SignalLengthSuma, JBB, ptr, 3);
    }
    break;
  case (TriggerType::SUBSCRIPTION):
    jam();
    // Since only backup uses subscription triggers we
    // send to backup directly for now
    fireTrigOrd->setGCI(req_struct->gci_hi);

    if (executeDirect)
    {
      jam();
      EXECUTE_DIRECT(refToMain(trigPtr->m_receiverRef),
                     GSN_FIRE_TRIG_ORD,
                     signal,
                     FireTrigOrd::SignalWithGCILength);
      jamEntry();
    }
    else
    {
      jam();
      // Todo send onlu before/after depending on BACKUP REDO/UNDO
      ndbassert(longsignal);
      LinearSectionPtr ptr[3];
      ptr[0].p = keyBuffer;
      ptr[0].sz = noPrimKey;
      ptr[1].p = beforeBuffer;
      ptr[1].sz = noBeforeWords;
      ptr[2].p = afterBuffer;
      ptr[2].sz = noAfterWords;
      sendSignal(trigPtr->m_receiverRef, GSN_FIRE_TRIG_ORD,
                 signal, FireTrigOrd::SignalWithGCILength, JBB, ptr, 3);
    }
    break;
  default:
    ndbrequire(false);
    break;
  }
}

Uint32 Dbtup::setAttrIds(Bitmask<MAXNROFATTRIBUTESINWORDS>& attributeMask, 
                         Uint32 m_no_of_attributesibutes, 
                         Uint32* inBuffer)
{
  Uint32 bufIndx = 0;
  for (Uint32 i = 0; i < m_no_of_attributesibutes; i++) {
    jam();
    if (attributeMask.get(i)) {
      jam();
      AttributeHeader::init(&inBuffer[bufIndx++], i, 0);
    }
  }
  return bufIndx;
}

bool Dbtup::readTriggerInfo(TupTriggerData* const trigPtr,
                            Operationrec* const regOperPtr,
                            KeyReqStruct *req_struct,
                            Fragrecord* const regFragPtr,
                            Uint32* const keyBuffer,
                            Uint32& noPrimKey,
                            Uint32* const afterBuffer,
                            Uint32& noAfterWords,
                            Uint32* const beforeBuffer,
                            Uint32& noBeforeWords,
                            bool disk)
{
  noAfterWords = 0;
  noBeforeWords = 0;
  Uint32 readBuffer[MAX_ATTRIBUTES_IN_TABLE];

//---------------------------------------------------------------------------
// Set-up variables needed by readAttributes operPtr.p, tabptr.p
//---------------------------------------------------------------------------
  operPtr.p = regOperPtr;
  tabptr.i = regFragPtr->fragTableId;
  ptrCheckGuard(tabptr, cnoOfTablerec, tablerec);

  Tablerec* const regTabPtr = tabptr.p;
  Uint32 num_attr= regTabPtr->m_no_of_attributes;
  Uint32 descr_start= regTabPtr->tabDescriptor;
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);

  req_struct->check_offset[MM]= regTabPtr->get_check_offset(MM);
  req_struct->check_offset[DD]= regTabPtr->get_check_offset(DD);
  req_struct->attr_descr= &tableDescriptor[descr_start];

//--------------------------------------------------------------------
// Read Primary Key Values
//--------------------------------------------------------------------
  Tuple_header *save0= req_struct->m_tuple_ptr;
  if (regOperPtr->op_struct.op_type == ZDELETE && 
      !regOperPtr->is_first_operation())
  {
    jam();
    req_struct->m_tuple_ptr=
      get_copy_tuple(regTabPtr,
                     &req_struct->prevOpPtr.p->m_copy_tuple_location);
  }

  if (regTabPtr->need_expand(disk)) 
    prepare_read(req_struct, regTabPtr, disk);
  
  int ret = readAttributes(req_struct,
			   &tableDescriptor[regTabPtr->readKeyArray].tabDescr,
			   regTabPtr->noOfKeyAttr,
			   keyBuffer,
			   ZATTR_BUFFER_SIZE,
			   false);
  ndbrequire(ret != -1);
  noPrimKey= ret;
  
  req_struct->m_tuple_ptr = save0;
  
  Uint32 numAttrsToRead;
  if ((regOperPtr->op_struct.op_type == ZUPDATE) &&
      (trigPtr->sendOnlyChangedAttributes)) {
    jam();
//--------------------------------------------------------------------
// Update that sends only changed information
//--------------------------------------------------------------------
    Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;
    attributeMask = trigPtr->attributeMask;
    attributeMask.bitAND(req_struct->changeMask);
    numAttrsToRead = setAttrIds(attributeMask, regTabPtr->m_no_of_attributes, 
				&readBuffer[0]);
    
  } else if ((regOperPtr->op_struct.op_type == ZDELETE) &&
             (!trigPtr->sendBeforeValues)) {
    jam();
//--------------------------------------------------------------------
// Delete without sending before values only read Primary Key
//--------------------------------------------------------------------
    return true;
  } else {
    jam();
//--------------------------------------------------------------------
// All others send all attributes that are monitored, except:
// Omit unchanged blob inlines on update i.e.
// attributeMask & ~ (blobAttributeMask & ~ changeMask)
//--------------------------------------------------------------------
    Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;
    attributeMask = trigPtr->attributeMask;
    if (regOperPtr->op_struct.op_type == ZUPDATE) {
      Bitmask<MAXNROFATTRIBUTESINWORDS> tmpMask = regTabPtr->blobAttributeMask;
      tmpMask.bitANDC(req_struct->changeMask);
      attributeMask.bitANDC(tmpMask);
    }
    numAttrsToRead = setAttrIds(attributeMask, regTabPtr->m_no_of_attributes,
                                &readBuffer[0]);
  }
  ndbrequire(numAttrsToRead < MAX_ATTRIBUTES_IN_TABLE);
//--------------------------------------------------------------------
// Read Main tuple values
//--------------------------------------------------------------------
  if (regOperPtr->op_struct.op_type != ZDELETE)
  {
    jam();
    int ret = readAttributes(req_struct,
			     &readBuffer[0],
			     numAttrsToRead,
			     afterBuffer,
			     ZATTR_BUFFER_SIZE,
			     false);
    ndbrequire(ret != -1);
    noAfterWords= ret;
  } else {
    jam();
    noAfterWords = 0;
  }

//--------------------------------------------------------------------
// Read Copy tuple values for UPDATE's
//--------------------------------------------------------------------
// Initialise pagep and tuple offset for read of copy tuple
//--------------------------------------------------------------------
  if ((regOperPtr->op_struct.op_type == ZUPDATE || 
       regOperPtr->op_struct.op_type == ZDELETE) &&
      (trigPtr->sendBeforeValues)) {
    jam();
    
    Tuple_header *save= req_struct->m_tuple_ptr;
    PagePtr tmp;
    if(regOperPtr->is_first_operation())
    {
      Uint32 *ptr= get_ptr(&tmp, &regOperPtr->m_tuple_location, regTabPtr);
      req_struct->m_tuple_ptr= (Tuple_header*)ptr;
    }
    else
    {
      req_struct->m_tuple_ptr =
        get_copy_tuple(regTabPtr,
                       &req_struct->prevOpPtr.p->m_copy_tuple_location);
    }

    if (regTabPtr->need_expand(disk)) 
      prepare_read(req_struct, regTabPtr, disk);
    
    int ret = readAttributes(req_struct,
			     &readBuffer[0],
			     numAttrsToRead,
			     beforeBuffer,
			     ZATTR_BUFFER_SIZE,
			     false);
    req_struct->m_tuple_ptr= save;
    ndbrequire(ret != -1);
    noBeforeWords = ret;
    if (refToMain(trigPtr->m_receiverRef) != SUMA &&
        (noAfterWords == noBeforeWords) &&
        (memcmp(afterBuffer, beforeBuffer, noAfterWords << 2) == 0)) {
//--------------------------------------------------------------------
// Although a trigger was fired it was not necessary since the old
// value and the new value was exactly the same
//--------------------------------------------------------------------
      jam();
      //XXX does this work with collations?
      return false;
    }
  }
  return true;
}

void Dbtup::sendTrigAttrInfo(Signal* signal, 
                             Uint32* data, 
                             Uint32  dataLen,
                             bool    executeDirect,
                             BlockReference receiverReference)
{
  TrigAttrInfo* const trigAttrInfo = (TrigAttrInfo *)signal->getDataPtrSend();
  Uint32 sigLen;
  Uint32 dataIndex = 0;
  do {
    sigLen = dataLen - dataIndex;
    if (sigLen > TrigAttrInfo::DataLength) {
      jam();
      sigLen = TrigAttrInfo::DataLength;
    }
    MEMCOPY_NO_WORDS(trigAttrInfo->getData(), 
                     data + dataIndex,
                     sigLen);
    if (executeDirect) {
      jam();
      EXECUTE_DIRECT(refToMain(receiverReference), 
                     GSN_TRIG_ATTRINFO,
                     signal,
		     TrigAttrInfo::StaticLength + sigLen);
      jamEntry();
    } else {
      jam();
      sendSignal(receiverReference, 
                 GSN_TRIG_ATTRINFO, 
                 signal, 
                 TrigAttrInfo::StaticLength + sigLen,
                 JBB);
    }
    dataIndex += sigLen;
  } while (dataLen != dataIndex);
}

void Dbtup::sendFireTrigOrd(Signal* signal,
                            KeyReqStruct *req_struct,
                            Operationrec * const regOperPtr, 
                            TupTriggerData* const trigPtr, 
			    Uint32 fragmentId,
                            Uint32 noPrimKeyWords, 
                            Uint32 noBeforeValueWords, 
                            Uint32 noAfterValueWords)
{
}

/*
 * Ordered index triggers.
 *
 * Insert: add entry to index
 * Update: add entry to index, de|ay remove until commit
 * Delete: do nothing, delay remove until commit
 * Commit: remove entry delayed from update and delete
 * Abort : remove entry added by insert and update
 *
 * See Notes.txt for the details.
 */

int
Dbtup::executeTuxInsertTriggers(Signal* signal,
                                Operationrec* regOperPtr,
                                Fragrecord* regFragPtr,
                                Tablerec* regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = regOperPtr->tupVersion;
  req->opInfo = TuxMaintReq::OpAdd;
  return addTuxEntries(signal, regOperPtr, regTabPtr);
}

int
Dbtup::executeTuxUpdateTriggers(Signal* signal,
                                Operationrec* regOperPtr,
                                Fragrecord* regFragPtr,
                                Tablerec* regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = regOperPtr->tupVersion;
  req->opInfo = TuxMaintReq::OpAdd;
  return addTuxEntries(signal, regOperPtr, regTabPtr);
}

int
Dbtup::addTuxEntries(Signal* signal,
                     Operationrec* regOperPtr,
                     Tablerec* regTabPtr)
{
  if (ERROR_INSERTED(4022)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    terrorCode = 9999;
    return -1;
  }
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  const DLList<TupTriggerData>& triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  Uint32 failPtrI;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    jam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    if (ERROR_INSERTED(4023) &&
        ! triggerList.hasNext(triggerPtr)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      terrorCode = 9999;
      failPtrI = triggerPtr.i;
      goto fail;
    }
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    jamEntry();
    if (req->errorCode != 0) {
      jam();
      terrorCode = req->errorCode;
      failPtrI = triggerPtr.i;
      goto fail;
    }
    triggerList.next(triggerPtr);
  }
  return 0;
fail:
  req->opInfo = TuxMaintReq::OpRemove;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != failPtrI) {
    jam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    jamEntry();
    ndbrequire(req->errorCode == 0);
    triggerList.next(triggerPtr);
  }
#ifdef VM_TRACE
  ndbout << "aborted partial tux update: op " << hex << regOperPtr << endl;
#endif
  return -1;
}

int
Dbtup::executeTuxDeleteTriggers(Signal* signal,
                                Operationrec* const regOperPtr,
                                Fragrecord* const regFragPtr,
                                Tablerec* const regTabPtr)
{
  // do nothing
  return 0;
}

void
Dbtup::executeTuxCommitTriggers(Signal* signal,
                                Operationrec* regOperPtr,
                                Fragrecord* regFragPtr,
                                Tablerec* regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  Uint32 tupVersion;
  if (regOperPtr->op_struct.op_type == ZINSERT) {
    if (! regOperPtr->op_struct.delete_insert_flag)
      return;
    jam();
    tupVersion= decr_tup_version(regOperPtr->tupVersion);
  } else if (regOperPtr->op_struct.op_type == ZUPDATE) {
    jam();
    tupVersion= decr_tup_version(regOperPtr->tupVersion);
  } else if (regOperPtr->op_struct.op_type == ZDELETE) {
    if (regOperPtr->op_struct.delete_insert_flag)
      return;
    jam();
    tupVersion= regOperPtr->tupVersion;
  } else {
    ndbrequire(false);
    tupVersion= 0; // remove warning
  }
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpRemove;
  removeTuxEntries(signal, regTabPtr);
}

void
Dbtup::executeTuxAbortTriggers(Signal* signal,
                               Operationrec* regOperPtr,
                               Fragrecord* regFragPtr,
                               Tablerec* regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  // get version
  Uint32 tupVersion;
  if (regOperPtr->op_struct.op_type == ZINSERT) {
    jam();
    tupVersion = regOperPtr->tupVersion;
  } else if (regOperPtr->op_struct.op_type == ZUPDATE) {
    jam();
    tupVersion = regOperPtr->tupVersion;
  } else if (regOperPtr->op_struct.op_type == ZDELETE) {
    jam();
    return;
  } else {
    ndbrequire(false);
    tupVersion= 0; // remove warning
  }
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpRemove;
  removeTuxEntries(signal, regTabPtr);
}

void
Dbtup::removeTuxEntries(Signal* signal,
                        Tablerec* regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  const DLList<TupTriggerData>& triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    jam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL,
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    jamEntry();
    // must succeed
    ndbrequire(req->errorCode == 0);
    triggerList.next(triggerPtr);
  }
}
