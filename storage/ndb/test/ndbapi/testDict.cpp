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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <../../include/kernel/ndb_limits.h>
#include <random.h>
#include <NdbAutoPtr.hpp>
#include <NdbMixRestarter.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbEnv.h>

char f_tablename[256];
 
#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; } 

#define CHECK2(b, c) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << ": " << c << endl; \
  result = NDBT_FAILED; \
  goto end; }

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(pNdb, records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateInvalidTables(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;

  char failTabName[256];

  for (int i = 0; i < 10; i++){
    BaseString::snprintf(failTabName, 256, "F%d", i);
  
    const NdbDictionary::Table* pFailTab = NDBT_Tables::getTable(failTabName);
    if (pFailTab != NULL){
      ndbout << "|- " << failTabName << endl;

      // Try to create table in db
      if (pFailTab->createTableInDb(pNdb) == 0){
        ndbout << failTabName << " created, this was not expected"<< endl;
        result = NDBT_FAILED;
      }

      // Verify that table is not in db    
      const NdbDictionary::Table* pTab2 = 
	NDBT_Table::discoverTableFromDb(pNdb, failTabName) ;
      if (pTab2 != NULL){
        ndbout << failTabName << " was found in DB, this was not expected"<< endl;
        result = NDBT_FAILED;
	if (pFailTab->equal(*pTab2) == true){
	  ndbout << "It was equal" << endl;
	} else {
	  ndbout << "It was not equal" << endl;
	}
	int records = 1000;
	HugoTransactions hugoTrans(*pTab2);
	if (hugoTrans.loadTable(pNdb, records) != 0){
	  ndbout << "It can NOT be loaded" << endl;
	} else{
	  ndbout << "It can be loaded" << endl;
	  
	  UtilTransactions utilTrans(*pTab2);
	  if (utilTrans.clearTable(pNdb, records, 64) != 0){
	    ndbout << "It can NOT be cleared" << endl;
	  } else{
	    ndbout << "It can be cleared" << endl;
	  }	  
	}
	
	if (pNdb->getDictionary()->dropTable(pTab2->getName()) == -1){
	  ndbout << "It can NOT be dropped" << endl;
	} else {
	  ndbout << "It can be dropped" << endl;
	}
      }
    }
  }
  return result;
}

int runCreateTheTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  const NdbDictionary::Table* pTab = ctx->getTab();

  // Try to create table in db
  if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
    return NDBT_FAILED;
  }

  // Verify that table is in db     
  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }
  ctx->setTab(pTab2);

  BaseString::snprintf(f_tablename, sizeof(f_tablename), 
                       "%s", pTab->getName());

  return NDBT_OK;
}

int runDropTheTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  //const NdbDictionary::Table* pTab = ctx->getTab();
  
  // Try to create table in db
  pNdb->getDictionary()->dropTable(f_tablename);
  
  return NDBT_OK;
}

int runCreateTableWhenDbIsFull(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  const char* tabName = "TRANSACTION"; //Use a util table
  
  const NdbDictionary::Table* pTab = NDBT_Tables::getTable(tabName);
  if (pTab != NULL){
    ndbout << "|- " << tabName << endl;
    
    // Verify that table is not in db     
    if (NDBT_Table::discoverTableFromDb(pNdb, tabName) != NULL){
      ndbout << tabName << " was found in DB"<< endl;
      return NDBT_FAILED;
    }

    // Try to create table in db
    if (NDBT_Tables::createTable(pNdb, pTab->getName()) == 0){
      result = NDBT_FAILED;
    }

    // Verify that table is in db     
    if (NDBT_Table::discoverTableFromDb(pNdb, tabName) != NULL){
      ndbout << tabName << " was found in DB"<< endl;
      result = NDBT_FAILED;
    }
  }

  return result;
}

int runDropTableWhenDbIsFull(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  const char* tabName = "TRANSACTION"; //Use a util table
  
  const NdbDictionary::Table* pTab = NDBT_Table::discoverTableFromDb(pNdb, tabName);
  if (pTab != NULL){
    ndbout << "|- TRANSACTION" << endl;
    
    // Try to drop table in db
    if (pNdb->getDictionary()->dropTable(pTab->getName()) == -1){
      result = NDBT_FAILED;
    }

    // Verify that table is not in db     
    if (NDBT_Table::discoverTableFromDb(pNdb, tabName) != NULL){
      ndbout << tabName << " was found in DB"<< endl;
      result = NDBT_FAILED;
    }
  }

  return result;

}


int runCreateAndDrop(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();
  int i = 0;
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  while (i < loops){

    ndbout << i << ": ";    
    // Try to create table in db
    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }

    if (pNdb->getDictionary()->dropTable(pTab2->getName())){
      ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
      return NDBT_FAILED;
    }
    
    // Verify that table is not in db     
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab3 != NULL){
      ndbout << pTab3->getName() << " was found in DB"<< endl;
      return NDBT_FAILED;
    }
    i++;
  }

  return NDBT_OK;
}

int runCreateAndDropAtRandom(NDBT_Context* ctx, NDBT_Step* step)
{
  myRandom48Init(NdbTick_CurrentMillisecond());
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int loops = ctx->getNumLoops();
  int numTables = NDBT_Tables::getNumTables();
  bool* tabList = new bool [ numTables ];
  int tabCount;

  {
    for (int num = 0; num < numTables; num++) {
      (void)pDic->dropTable(NDBT_Tables::getTable(num)->getName());
      tabList[num] = false;
    }
    tabCount = 0;
  }

  NdbRestarter restarter;
  int result = NDBT_OK;
  int bias = 1; // 0-less 1-more
  int i = 0;
  
  while (i < loops) {
    g_info << "loop " << i << " tabs " << tabCount << "/" << numTables << endl;
    int num = myRandom48(numTables);
    const NdbDictionary::Table* pTab = NDBT_Tables::getTable(num);
    char tabName[200];
    strcpy(tabName, pTab->getName());

    if (tabList[num] == false) {
      if (bias == 0 && myRandom48(100) < 80)
        continue;
      g_info << tabName << ": create" << endl;
      if (pDic->createTable(*pTab) != 0) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": create failed: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      const NdbDictionary::Table* pTab2 = pDic->getTable(tabName);
      if (pTab2 == NULL) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": verify create: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      tabList[num] = true;
      assert(tabCount < numTables);
      tabCount++;
      if (tabCount == numTables)
        bias = 0;
    }
    else {
      if (bias == 1 && myRandom48(100) < 80)
        continue;
      g_info << tabName << ": drop" << endl;
      if (restarter.insertErrorInAllNodes(4013) != 0) {
        g_err << "error insert failed" << endl;
        result = NDBT_FAILED;
        break;
      }
      if (pDic->dropTable(tabName) != 0) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": drop failed: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      const NdbDictionary::Table* pTab2 = pDic->getTable(tabName);
      if (pTab2 != NULL) {
        g_err << tabName << ": verify drop: table exists" << endl;
        result = NDBT_FAILED;
        break;
      }
      if (pDic->getNdbError().code != 709 &&
          pDic->getNdbError().code != 723) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": verify drop: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      tabList[num] = false;
      assert(tabCount > 0);
      tabCount--;
      if (tabCount == 0)
        bias = 1;
    }
    i++;
  }
  
  for (Uint32 i = 0; i<(Uint32)numTables; i++)
    if (tabList[i])
      pDic->dropTable(NDBT_Tables::getTable(i)->getName());
  
  delete [] tabList;
  return result;
}


int runCreateAndDropWithData(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int i = 0;
  
  NdbRestarter restarter;
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if(restarter.dumpStateAllNodes(&val, 1) != 0){
    int result;
    do { CHECK(0); } while (0);
    g_err << "Unable to change timebetween LCP" << endl;
    return NDBT_FAILED;
  }
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  while (i < loops){
    ndbout << i << ": ";
    // Try to create table in db
    
    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }

    HugoTransactions hugoTrans(*pTab2);
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    int count = 0;
    UtilTransactions utilTrans(*pTab2);
    if (utilTrans.selectCount(pNdb, 64, &count) != 0){
      return NDBT_FAILED;
    }
    if (count != records){
      ndbout << count <<" != "<<records << endl;
      return NDBT_FAILED;
    }

    if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
      ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
      return NDBT_FAILED;
    }
    
    // Verify that table is not in db     
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab3 != NULL){
      ndbout << pTab3->getName() << " was found in DB"<< endl;
      return NDBT_FAILED;
    }
    

    i++;
  }

  return NDBT_OK;
}

int runFillTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.fillTable(pNdb) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(pNdb,  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateAndDropDuring(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int i = 0;
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  while (i < loops && result == NDBT_OK){
    ndbout << i << ": " << endl;    
    // Try to create table in db

    Ndb* pNdb = GETNDB(step);
    g_debug << "Creating table" << endl;

    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      g_err << "createTableInDb failed" << endl;
      result =  NDBT_FAILED;
      continue;
    }
    
    g_debug << "Verifying creation of table" << endl;

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      g_err << pTab->getName() << " was not found in DB"<< endl;
      result =  NDBT_FAILED;
      continue;
    }
    
    NdbSleep_MilliSleep(3000);

    g_debug << "Dropping table" << endl;

    if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
      g_err << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
      result =  NDBT_FAILED;
      continue;
    }
    
    g_debug << "Verifying dropping of table" << endl;

    // Verify that table is not in db     
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab3 != NULL){
      g_err << pTab3->getName() << " was found in DB"<< endl;
      result =  NDBT_FAILED;
      continue;
    }
    i++;
  }
  ctx->stopTest();
  
  return result;
}


int runUseTableUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();

  const NdbDictionary::Table* pTab = ctx->getTab();

  while (ctx->isTestStopped() == false) {
    // g_info << i++ << ": ";


    // Delete and recreate Ndb object
    // Otherwise you always get Invalid Schema Version
    // It would be a nice feature to remove this two lines
    //step->tearDown();
    //step->setUp();

    Ndb* pNdb = GETNDB(step);

    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL)
      continue;

    int res;
    HugoTransactions hugoTrans(*pTab2);
    if ((res = hugoTrans.loadTable(pNdb, records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
    
    if ((res = hugoTrans.clearTable(pNdb,  records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
  }
  g_info << endl;
  return NDBT_OK;
}

int runUseTableUntilStopped2(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  HugoTransactions hugoTrans(*pTab2);

  int i = 0;
  while (ctx->isTestStopped() == false) 
  {
    ndbout_c("loop: %u", i++);


    // Delete and recreate Ndb object
    // Otherwise you always get Invalid Schema Version
    // It would be a nice feature to remove this two lines
    //step->tearDown();
    //step->setUp();


    int res;
    if ((res = hugoTrans.loadTable(pNdb, records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
    
    if ((res = hugoTrans.scanUpdateRecords(pNdb, records)) != 0)
    {
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }

    if ((res = hugoTrans.clearTable(pNdb,  records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
  }
  g_info << endl;
  return NDBT_OK;
}

int runUseTableUntilStopped3(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table* pTab2 =
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  HugoTransactions hugoTrans(*pTab2);

  int i = 0;
  while (ctx->isTestStopped() == false)
  {
    ndbout_c("loop: %u", i++);


    // Delete and recreate Ndb object
    // Otherwise you always get Invalid Schema Version
    // It would be a nice feature to remove this two lines
    //step->tearDown();
    //step->setUp();


    int res;
    if ((res = hugoTrans.scanUpdateRecords(pNdb, records)) != 0)
    {
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
  }
  g_info << endl;
  return NDBT_OK;
}


int
runCreateMaxTables(NDBT_Context* ctx, NDBT_Step* step)
{
  char tabName[256];
  int numTables = ctx->getProperty("tables", 1000);
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int i = 0;
  for (i = 0; i < numTables; i++) {
    BaseString::snprintf(tabName, 256, "MAXTAB%d", i);
    if (pNdb->waitUntilReady(30) != 0) {
      // Db is not ready, return with failure
      return NDBT_FAILED;
    }
    const NdbDictionary::Table* pTab = ctx->getTab();
    //ndbout << "|- " << tabName << endl;
    // Set new name for T1
    NdbDictionary::Table newTab(* pTab);
    newTab.setName(tabName);
    // Drop any old (or try to)
    (void)pDic->dropTable(newTab.getName());
    // Try to create table in db
    if (newTab.createTableInDb(pNdb) != 0) {
      ndbout << tabName << " could not be created: "
             << pDic->getNdbError() << endl;
      if (pDic->getNdbError().code == 707 ||
          pDic->getNdbError().code == 708 ||
          pDic->getNdbError().code == 826 ||
          pDic->getNdbError().code == 827)
        break;
      return NDBT_FAILED;
    }
    // Verify that table exists in db    
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, tabName) ;
    if (pTab3 == NULL){
      ndbout << tabName << " was not found in DB: "
             << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    if (! newTab.equal(*pTab3)) {
      ndbout << "It was not equal" << endl; abort();
      return NDBT_FAILED;
    }
    int records = ctx->getNumRecords();
    HugoTransactions hugoTrans(*pTab3);
    if (hugoTrans.loadTable(pNdb, records) != 0) {
      ndbout << "It can NOT be loaded" << endl;
      return NDBT_FAILED;
    }
    UtilTransactions utilTrans(*pTab3);
    if (utilTrans.clearTable(pNdb, records, 64) != 0) {
      ndbout << "It can NOT be cleared" << endl;
      return NDBT_FAILED;
    }
  }
  if (pNdb->waitUntilReady(30) != 0) {
    // Db is not ready, return with failure
    return NDBT_FAILED;
  }
  ctx->setProperty("maxtables", i);
  // HURRAAA!
  return NDBT_OK;
}

int runDropMaxTables(NDBT_Context* ctx, NDBT_Step* step)
{
  char tabName[256];
  int numTables = ctx->getProperty("maxtables", (Uint32)0);
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  for (int i = 0; i < numTables; i++) {
    BaseString::snprintf(tabName, 256, "MAXTAB%d", i);
    if (pNdb->waitUntilReady(30) != 0) {
      // Db is not ready, return with failure
      return NDBT_FAILED;
    }
    // Verify that table exists in db    
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, tabName) ;
    if (pTab3 == NULL) {
      ndbout << tabName << " was not found in DB: "
             << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    // Try to drop table in db
    if (pDic->dropTable(pTab3->getName()) != 0) {
      ndbout << tabName << " could not be dropped: "
             << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runTestFragmentTypes(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int fragTtype = ctx->getProperty("FragmentType");
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  NdbRestarter restarter;

  if (pNdb->waitUntilReady(30) != 0){
    // Db is not ready, return with failure
    return NDBT_FAILED;
  }
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  pNdb->getDictionary()->dropTable(pTab->getName());

  NdbDictionary::Table newTab(* pTab);
  // Set fragment type for table    
  newTab.setFragmentType((NdbDictionary::Object::FragmentType)fragTtype);
  
  // Try to create table in db
  if (newTab.createTableInDb(pNdb) != 0){
    ndbout << newTab.getName() << " could not be created"
	   << ", fragmentType = "<<fragTtype <<endl;
    ndbout << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  // Verify that table exists in db    
  const NdbDictionary::Table* pTab3 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName()) ;
  if (pTab3 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
    
  }
  
  if (pTab3->getFragmentType() != fragTtype){
    ndbout << pTab->getName() << " fragmentType error "<< endl;
    result = NDBT_FAILED;
    goto drop_the_tab;
  }
/**
   This test does not work since fragmentation is
   decided by the kernel, hence the fragementation
   attribute on the column will differ

  if (newTab.equal(*pTab3) == false){
    ndbout << "It was not equal" << endl;
    result = NDBT_FAILED;
    goto drop_the_tab;
  } 
*/
  do {
    
    HugoTransactions hugoTrans(*pTab3);
    UtilTransactions utilTrans(*pTab3);
    int count;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    // restart all
    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    int timeout = 120;
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    // Verify content
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records, 64) == 0);
    
  } while(false);
  
 drop_the_tab:
  
  // Try to drop table in db
  if (pNdb->getDictionary()->dropTable(pTab3->getName()) != 0){
    ndbout << pTab3->getName()  << " could not be dropped"<< endl;
    result =  NDBT_FAILED;
  }
  
  return result;
}


int runTestTemporaryTables(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int i = 0;
  NdbRestarter restarter;
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  NdbDictionary::Table newTab(* pTab);
  // Set table as temporary
  newTab.setStoredTable(false);
  
  // Try to create table in db
  if (newTab.createTableInDb(pNdb) != 0){
    return NDBT_FAILED;
  }
  
  // Verify that table is in db     
  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }

  if (pTab2->getStoredTable() != false){
    ndbout << pTab->getName() << " was not temporary in DB"<< endl;
    result = NDBT_FAILED;
    goto drop_the_tab;
  }

  
  while (i < loops && result == NDBT_OK){
    ndbout << i << ": ";

    HugoTransactions hugoTrans(*pTab2);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);

    int count = 0;
    UtilTransactions utilTrans(*pTab2);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);

    // restart all
    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    int timeout = 120;
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    i++;
  }

 drop_the_tab:

   
  if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
    ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
    result = NDBT_FAILED;
  }
  
  // Verify that table is not in db     
  const NdbDictionary::Table* pTab3 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab3 != NULL){
    ndbout << pTab3->getName() << " was found in DB"<< endl;
    result = NDBT_FAILED;
  }
    
  return result;
}

int runPkSizes(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  char tabName[256];
  int minPkSize = 1;
  ndbout << "minPkSize=" <<minPkSize<<endl;
  int maxPkSize = MAX_KEY_SIZE_IN_WORDS * 4;
  ndbout << "maxPkSize=" <<maxPkSize<<endl;
  Ndb* pNdb = GETNDB(step);
  int numRecords = ctx->getNumRecords();

  for (int i = minPkSize; i < maxPkSize; i++){
    BaseString::snprintf(tabName, 256, "TPK_%d", i);

    int records = numRecords;
    int max = ~0;
    // Limit num records for small PKs
    if (i == 1)
      max = 99;
    if (i == 2)
      max = 999;
    if (i == 3)
      max = 9999;
    if (records > max)
      records = max;
    ndbout << "records =" << records << endl;

    if (pNdb->waitUntilReady(30) != 0){
      // Db is not ready, return with failure
      return NDBT_FAILED;
    }
  
    ndbout << "|- " << tabName << endl;

    if (NDBT_Tables::createTable(pNdb, tabName) != 0){
      ndbout << tabName << " could not be created"<< endl;
      return NDBT_FAILED;
    }
    
    // Verify that table exists in db    
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, tabName) ;
    if (pTab3 == NULL){
      g_err << tabName << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }

    //    ndbout << *pTab3 << endl;

    if (pTab3->equal(*NDBT_Tables::getTable(tabName)) == false){
      g_err << "It was not equal" << endl;
      return NDBT_FAILED;
    }

    do {
      // Do it all
      HugoTransactions hugoTrans(*pTab3);
      UtilTransactions utilTrans(*pTab3);
      int count;
      CHECK(hugoTrans.loadTable(pNdb, records) == 0);
      CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == records);
      CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
      CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == (records/2));
      CHECK(utilTrans.clearTable(pNdb, records) == 0);
      
#if 0
      // Fill table
      CHECK(hugoTrans.fillTable(pNdb) == 0);        
      CHECK(utilTrans.clearTable2(pNdb, records) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == 0);
#endif
    } while(false);

    // Drop table
    if (pNdb->getDictionary()->dropTable(pTab3->getName()) != 0){
      ndbout << "Failed to drop "<<pTab3->getName()<<" in db" << endl;
      return NDBT_FAILED;
    }
  }
  return result;
}

int runStoreFrm(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  const NdbDictionary::Table* pTab = ctx->getTab();
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();

  for (int l = 0; l < loops && result == NDBT_OK ; l++){

    Uint32 dataLen = (Uint32)myRandom48(MAX_FRM_DATA_SIZE);
    // size_t dataLen = 10;
    unsigned char data[MAX_FRM_DATA_SIZE];

    char start = l + 248;
    for(Uint32 i = 0; i < dataLen; i++){
      data[i] = start;
      start++;
    }
#if 0
    ndbout << "dataLen="<<dataLen<<endl;
    for (Uint32 i = 0; i < dataLen; i++){
      unsigned char c = data[i];
      ndbout << hex << c << ", ";
    }
    ndbout << endl;
#endif
        
    NdbDictionary::Table newTab(* pTab);
    void* pData = &data;
    newTab.setFrm(pData, dataLen);
    
    // Try to create table in db
    if (newTab.createTableInDb(pNdb) != 0){
      result = NDBT_FAILED;
      continue;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      g_err << pTab->getName() << " was not found in DB"<< endl;
      result = NDBT_FAILED;
      continue;
    }
    
    const void* pData2 = pTab2->getFrmData();
    Uint32 resultLen = pTab2->getFrmLength();
    if (dataLen != resultLen){
      g_err << "Length of data failure" << endl
	    << " expected = " << dataLen << endl
	    << " got = " << resultLen << endl;
      result = NDBT_FAILED;      
    }
    
    // Verfiy the frm data
    if (memcmp(pData, pData2, resultLen) != 0){
      g_err << "Wrong data recieved" << endl;
      for (size_t i = 0; i < dataLen; i++){
	unsigned char c = ((unsigned char*)pData2)[i];
	g_err << hex << c << ", ";
      }
      g_err << endl;
      result = NDBT_FAILED;
    }
    
    if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
      g_err << "It can NOT be dropped" << endl;
      result = NDBT_FAILED;
    } 
  }
  
  return result;
}

int runStoreFrmError(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  const NdbDictionary::Table* pTab = ctx->getTab();
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();

  for (int l = 0; l < loops && result == NDBT_OK ; l++){

    const Uint32 dataLen = MAX_FRM_DATA_SIZE + 10;
    unsigned char data[dataLen];

    char start = l + 248;
    for(Uint32 i = 0; i < dataLen; i++){
      data[i] = start;
      start++;
    }
#if 0
    ndbout << "dataLen="<<dataLen<<endl;
    for (Uint32 i = 0; i < dataLen; i++){
      unsigned char c = data[i];
      ndbout << hex << c << ", ";
    }
    ndbout << endl;
#endif

    NdbDictionary::Table newTab(* pTab);
        
    void* pData = &data;
    newTab.setFrm(pData, dataLen);
    
    // Try to create table in db
    if (newTab.createTableInDb(pNdb) == 0){
      result = NDBT_FAILED;
      continue;
    }
    
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 != NULL){
      g_err << pTab->getName() << " was found in DB"<< endl;
      result = NDBT_FAILED;
      if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
	g_err << "It can NOT be dropped" << endl;
	result = NDBT_FAILED;
      } 
      
      continue;
    } 
    
  }

  return result;
}

int verifyTablesAreEqual(const NdbDictionary::Table* pTab, const NdbDictionary::Table* pTab2){
  // Verify that getPrimaryKey only returned true for primary keys
  for (int i = 0; i < pTab2->getNoOfColumns(); i++){
    const NdbDictionary::Column* col = pTab->getColumn(i);
    const NdbDictionary::Column* col2 = pTab2->getColumn(i);
    if (col->getPrimaryKey() != col2->getPrimaryKey()){
      g_err << "col->getPrimaryKey() != col2->getPrimaryKey()" << endl;
      return NDBT_FAILED;
    }
  }
  
  if (!pTab->equal(*pTab2)){
    g_err << "equal failed" << endl;
    g_info << *(NDBT_Table*)pTab; // gcc-4.1.2
    g_info << *(NDBT_Table*)pTab2;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runGetPrimaryKey(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  g_info << *(NDBT_Table*)pTab;
  // Try to create table in db
  if (pTab->createTableInDb(pNdb) != 0){
    return NDBT_FAILED;
  }

  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }

  int result = NDBT_OK;
  if (verifyTablesAreEqual(pTab, pTab2) != NDBT_OK)
    result = NDBT_FAILED;
  
  
#if 0
  // Create an index on the table and see what 
  // the function returns now
  char name[200];
  sprintf(name, "%s_X007", pTab->getName());
  NDBT_Index* pInd = new NDBT_Index(name);
  pInd->setTable(pTab->getName());
  pInd->setType(NdbDictionary::Index::UniqueHashIndex);
  //  pInd->setLogging(false);
  for (int i = 0; i < 2; i++){
    const NDBT_Attribute* pAttr = pTab->getAttribute(i);
    pInd->addAttribute(*pAttr);
  }
  g_info << "Create index:" << endl << *pInd;
  if (pInd->createIndexInDb(pNdb, false) != 0){
    result = NDBT_FAILED;
  }  
  delete pInd;

  const NdbDictionary::Table* pTab3 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab3 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }

  if (verifyTablesAreEqual(pTab, pTab3) != NDBT_OK)
    result = NDBT_FAILED;
  if (verifyTablesAreEqual(pTab2, pTab3) != NDBT_OK)
    result = NDBT_FAILED;
#endif

#if 0  
  if (pTab2->getDictionary()->dropTable(pNdb) != 0){
    ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
    return NDBT_FAILED;
  }
  
  // Verify that table is not in db     
  const NdbDictionary::Table* pTab4 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab4 != NULL){
    ndbout << pTab4->getName() << " was found in DB"<< endl;
    return NDBT_FAILED;
  }
#endif

  return result;
}

struct ErrorCodes { int error_id; bool crash;};
ErrorCodes
NF_codes[] = {
  {6003, true}
  ,{6004, true}
  //,6005, true,
  //{7173, false}
};

int
runNF1(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  if(restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  myRandom48Init(NdbTick_CurrentMillisecond());
  
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  dict->dropTable(pTab->getName());

  int result = NDBT_OK;

  const int loops = ctx->getNumLoops();
  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    const int sz = sizeof(NF_codes)/sizeof(NF_codes[0]);
    for(int i = 0; i<sz; i++){
      int rand = myRandom48(restarter.getNumDbNodes());
      int nodeId = restarter.getRandomNotMasterNodeId(rand);
      struct ErrorCodes err_struct = NF_codes[i];
      int error = err_struct.error_id;
      bool crash = err_struct.crash;
      
      g_info << "NF1: node = " << nodeId << " error code = " << error << endl;
      
      int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 3};
      
      CHECK2(restarter.dumpStateOneNode(nodeId, val2, 2) == 0,
	     "failed to set RestartOnErrorInsert");

      CHECK2(restarter.insertErrorInNode(nodeId, error) == 0,
	     "failed to set error insert");
      
      CHECK2(dict->createTable(* pTab) == 0,
	     "failed to create table");
      
      if (crash) {
        CHECK2(restarter.waitNodesNoStart(&nodeId, 1) == 0,
	    "waitNodesNoStart failed");

        if(myRandom48(100) > 50){
  	  CHECK2(restarter.startNodes(&nodeId, 1) == 0,
	       "failed to start node");
          
	  CHECK2(restarter.waitClusterStarted() == 0,
	       "waitClusterStarted failed");

  	  CHECK2(dict->dropTable(pTab->getName()) == 0,
	       "drop table failed");
        } else {
	  CHECK2(dict->dropTable(pTab->getName()) == 0,
	       "drop table failed");
	
	  CHECK2(restarter.startNodes(&nodeId, 1) == 0,
	       "failed to start node");
          
	  CHECK2(restarter.waitClusterStarted() == 0,
	       "waitClusterStarted failed");
        }
      }
    }
  }
 end:  
  dict->dropTable(pTab->getName());
  
  return result;
}
  
#define APIERROR(error) \
  { g_err << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << endl; \
  }

int
runCreateAutoincrementTable(NDBT_Context* ctx, NDBT_Step* step){

  Uint32 startvalues[5] = {256-2, 0, 256*256-2, ~Uint32(0), 256*256*256-2};

  int ret = NDBT_OK;

  for (int jj = 0; jj < 5 && ret == NDBT_OK; jj++) {
    char tabname[] = "AUTOINCTAB";
    Uint32 startvalue = startvalues[jj];

    NdbDictionary::Table myTable;
    NdbDictionary::Column myColumn;

    Ndb* myNdb = GETNDB(step);
    NdbDictionary::Dictionary* myDict = myNdb->getDictionary();


    if (myDict->getTable(tabname) != NULL) {
      g_err << "NDB already has example table: " << tabname << endl;
      APIERROR(myNdb->getNdbError());
      return NDBT_FAILED;
    }

    myTable.setName(tabname);

    myColumn.setName("ATTR1");
    myColumn.setType(NdbDictionary::Column::Unsigned);
    myColumn.setLength(1);
    myColumn.setPrimaryKey(true);
    myColumn.setNullable(false);
    myColumn.setAutoIncrement(true);
    if (startvalue != ~Uint32(0)) // check that default value starts with 1
      myColumn.setAutoIncrementInitialValue(startvalue);
    myTable.addColumn(myColumn);

    if (myDict->createTable(myTable) == -1) {
      g_err << "Failed to create table " << tabname << endl;
      APIERROR(myNdb->getNdbError());
      return NDBT_FAILED;
    }


    if (startvalue == ~Uint32(0)) // check that default value starts with 1
      startvalue = 1;

    for (int i = 0; i < 16; i++) {

      Uint64 value;
      if (myNdb->getAutoIncrementValue(tabname, value, 1) == -1) {
        g_err << "getAutoIncrementValue failed on " << tabname << endl;
        APIERROR(myNdb->getNdbError());
        return NDBT_FAILED;
      }
      else if (value != (startvalue+i)) {
        g_err << "value = " << value << " expected " << startvalue+i << endl;;
        APIERROR(myNdb->getNdbError());
        //      ret = NDBT_FAILED;
        //      break;
      }
    }

    if (myDict->dropTable(tabname) == -1) {
      g_err << "Failed to drop table " << tabname << endl;
      APIERROR(myNdb->getNdbError());
      ret = NDBT_FAILED;
    }
  }

  return ret;
}

int
runTableRename(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    const NdbDictionary::Table* pTab = ctx->getTab();

    // Try to create table in db
    if (pTab->createTableInDb(pNdb) != 0){
      return NDBT_FAILED;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    // Load table
    HugoTransactions hugoTrans(*ctx->getTab());
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    // Rename table
    BaseString pTabName(pTab->getName());
    BaseString pTabNewName(pTabName);
    pTabNewName.append("xx");
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable = *oldTable;
      newTable.setName(pTabNewName.c_str());
      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableRename failed");
    }
    else {
      result = NDBT_FAILED;
    }
    
    // Verify table contents
    NdbDictionary::Table pNewTab(pTabNewName.c_str());
    
    UtilTransactions utilTrans(pNewTab);
    if (utilTrans.clearTable(pNdb,  records) != 0){
      continue;
    }    

    // Drop table
    dict->dropTable(pNewTab.getName());
  }
 end:

  return result;
}

int
runTableRenameNF(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  if(restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    const NdbDictionary::Table* pTab = ctx->getTab();

    // Try to create table in db
    if (pTab->createTableInDb(pNdb) != 0){
      return NDBT_FAILED;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    // Load table
    HugoTransactions hugoTrans(*ctx->getTab());
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    BaseString pTabName(pTab->getName());
    BaseString pTabNewName(pTabName);
    pTabNewName.append("xx");
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable = *oldTable;
      newTable.setName(pTabNewName.c_str());
      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableRename failed");
    }
    else {
      result = NDBT_FAILED;
    }
    
    // Restart one node at a time
    
    /**
     * Need to run LCP at high rate otherwise
     * packed replicas become "to many"
     */
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    if(restarter.dumpStateAllNodes(&val, 1) != 0){
      do { CHECK(0); } while(0);
      g_err << "Failed to set LCP to min value" << endl;
      return NDBT_FAILED;
    }
    
    const int numNodes = restarter.getNumDbNodes();
    for(int i = 0; i<numNodes; i++){
      int nodeId = restarter.getDbNodeId(i);
      int error = NF_codes[i].error_id;

      g_info << "NF1: node = " << nodeId << " error code = " << error << endl;

      CHECK2(restarter.restartOneDbNode(nodeId) == 0,
	     "failed to set restartOneDbNode");

      CHECK2(restarter.waitClusterStarted() == 0,
	     "waitClusterStarted failed");

    }

    // Verify table contents
    NdbDictionary::Table pNewTab(pTabNewName.c_str());
    
    UtilTransactions utilTrans(pNewTab);
    if (utilTrans.clearTable(pNdb,  records) != 0){
      continue;
    }    

    // Drop table
    dict->dropTable(pTabNewName.c_str());
  }
 end:    
  return result;
}

int
runTableRenameSR(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  if(restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    // Rename table
    const NdbDictionary::Table* pTab = ctx->getTab();

    // Try to create table in db
    if (pTab->createTableInDb(pNdb) != 0){
      return NDBT_FAILED;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    // Load table
    HugoTransactions hugoTrans(*ctx->getTab());
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    BaseString pTabName(pTab->getName());
    BaseString pTabNewName(pTabName);
    pTabNewName.append("xx");
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable = *oldTable;
      newTable.setName(pTabNewName.c_str());
      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableRename failed");
    }
    else {
      result = NDBT_FAILED;
    }
    
    // Restart cluster
    
    /**
     * Need to run LCP at high rate otherwise
     * packed replicas become "to many"
     */
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    if(restarter.dumpStateAllNodes(&val, 1) != 0){
      do { CHECK(0); } while(0);
      g_err << "Failed to set LCP to min value" << endl;
      return NDBT_FAILED;
    }
    
    CHECK2(restarter.restartAll() == 0,
	   "failed to set restartOneDbNode");
    
    CHECK2(restarter.waitClusterStarted() == 0,
	   "waitClusterStarted failed");
    
    // Verify table contents
    NdbDictionary::Table pNewTab(pTabNewName.c_str());
    
    UtilTransactions utilTrans(pNewTab);
    if (utilTrans.clearTable(pNdb,  records) != 0){
      continue;
    }    

    // Drop table
    dict->dropTable(pTabNewName.c_str());
  }
 end:    
  return result;
}

/*
  Run online alter table add attributes.
 */
int
runTableAddAttrs(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  NdbDictionary::Table myTab= *(ctx->getTab());

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    // Try to create table in db

    if (NDBT_Tables::createTable(pNdb, myTab.getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, myTab.getName());
    if (pTab2 == NULL){
      ndbout << myTab.getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    /*
      Check that table already has a varpart, otherwise add attr is
      not possible.
    */
    if (pTab2->getForceVarPart() == false)
    {
      const NdbDictionary::Column *col;
      for (Uint32 i= 0; (col= pTab2->getColumn(i)) != 0; i++)
      {
        if (col->getStorageType() == NDB_STORAGETYPE_MEMORY &&
            (col->getDynamic() || col->getArrayType() != NDB_ARRAYTYPE_FIXED))
          break;
      }
      if (col == 0)
      {
        /* Alter table add attribute not applicable, just mark success. */
        dict->dropTable(pTab2->getName());
        break;
      }
    }

    // Load table
    HugoTransactions beforeTrans(*ctx->getTab());
    if (beforeTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    // Add attributes to table.
    BaseString pTabName(pTab2->getName());
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable= *oldTable;

      NDBT_Attribute newcol1("NEWKOL1", NdbDictionary::Column::Unsigned, 1,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol1);
      NDBT_Attribute newcol2("NEWKOL2", NdbDictionary::Column::Char, 14,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol2);
      NDBT_Attribute newcol3("NEWKOL3", NdbDictionary::Column::Bit, 20,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol3);
      NDBT_Attribute newcol4("NEWKOL4", NdbDictionary::Column::Varbinary, 42,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol4);

      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableAddAttrs failed");
      /* Need to purge old version and reload new version after alter table. */
      dict->invalidateTable(pTabName.c_str());
    }
    else {
      result = NDBT_FAILED;
    }

    {
      HugoTransactions afterTrans(* dict->getTable(pTabName.c_str()));

      ndbout << "delete...";
      if (afterTrans.clearTable(pNdb) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "insert...";
      if (afterTrans.loadTable(pNdb, records) != 0){
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "update...";
      if (afterTrans.scanUpdateRecords(pNdb, records) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "delete...";
      if (afterTrans.clearTable(pNdb) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;
    }
    
    // Drop table.
    dict->dropTable(pTabName.c_str());
  }
 end:

  return result;
}

/*
  Run online alter table add attributes while running simultaneous
  transactions on it in separate thread.
 */
int
runTableAddAttrsDuring(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  int abortAlter = ctx->getProperty("AbortAlter", Uint32(0));

  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();
  NdbRestarter res;

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  NdbDictionary::Table myTab= *(ctx->getTab());

  if (myTab.getForceVarPart() == false)
  {
    const NdbDictionary::Column *col;
    for (Uint32 i= 0; (col= myTab.getColumn(i)) != 0; i++)
    {
      if (col->getStorageType() == NDB_STORAGETYPE_MEMORY &&
          (col->getDynamic() || col->getArrayType() != NDB_ARRAYTYPE_FIXED))
        break;
    }
    if (col == 0)
    {
      ctx->stopTest();
      return NDBT_OK;
    }
  }

  //if 

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    ndbout << l << ": " << endl;    

    Ndb* pNdb = GETNDB(step);
    NdbDictionary::Dictionary* dict = pNdb->getDictionary();

    /*
      Check that table already has a varpart, otherwise add attr is
      not possible.
    */

    // Add attributes to table.
    ndbout << "Altering table" << endl;
    
    const NdbDictionary::Table * oldTable = dict->getTable(myTab.getName());
    if (oldTable) {
      NdbDictionary::Table newTable= *oldTable;
      
      char name[256];
      BaseString::snprintf(name, sizeof(name), "NEWCOL%d", l);
      NDBT_Attribute newcol1(name, NdbDictionary::Column::Unsigned, 1,
                             false, true, 0,
                             NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol1);
      //ToDo: check #loops, how many columns l

      if (abortAlter == 0)
      {
        CHECK2(dict->alterTable(*oldTable, newTable) == 0,
               "TableAddAttrsDuring failed");
      }
      else
      {
        int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
        res.insertErrorInNode(nodeId, 4029);
        CHECK2(dict->alterTable(*oldTable, newTable) != 0,
               "TableAddAttrsDuring failed");
      }

      dict->invalidateTable(myTab.getName());
      const NdbDictionary::Table * newTab = dict->getTable(myTab.getName());
      HugoTransactions hugoTrans(* newTab);
      hugoTrans.scanUpdateRecords(pNdb, records);
    }
    else {
      result= NDBT_FAILED;
      break;
    }
  }
 end:

  ctx->stopTest();

  return result;
}

static void
f(const NdbDictionary::Column * col){
  if(col == 0){
    abort();
  }
}

int
runTestDictionaryPerf(NDBT_Context* ctx, NDBT_Step* step){
  Vector<char*> cols;
  Vector<const NdbDictionary::Table*> tabs;
  int i;

  Ndb* pNdb = GETNDB(step);  

  const Uint32 count = NDBT_Tables::getNumTables();
  for (i=0; i < (int)count; i++){
    const NdbDictionary::Table * tab = NDBT_Tables::getTable(i);
    pNdb->getDictionary()->createTable(* tab);
    
    const NdbDictionary::Table * tab2 = pNdb->getDictionary()->getTable(tab->getName());
    
    for(size_t j = 0; j<(size_t)tab->getNoOfColumns(); j++){
      cols.push_back((char*)tab2);
      cols.push_back(strdup(tab->getColumn(j)->getName()));
    }
  }

  const Uint32 times = 10000000;

  ndbout_c("%d tables and %d columns", 
	   NDBT_Tables::getNumTables(), cols.size()/2);

  char ** tcols = cols.getBase();

  srand(time(0));
  Uint32 size = cols.size() / 2;
  //char ** columns = &cols[0];
  Uint64 start = NdbTick_CurrentMillisecond();
  for(i = 0; i<(int)times; i++){
    int j = 2 * (rand() % size);
    const NdbDictionary::Table* tab = (const NdbDictionary::Table*)tcols[j];
    const char * col = tcols[j+1];
    const NdbDictionary::Column* column = tab->getColumn(col);
    f(column);
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  stop -= start;

  Uint64 per = stop;
  per *= 1000;
  per /= times;
  
  ndbout_c("%d random getColumn(name) in %Ld ms -> %u us/get",
	   times, stop, Uint32(per));

  return NDBT_OK;
}

int
runCreateLogfileGroup(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::LogfileGroup lg;
  lg.setName("DEFAULT-LG");
  lg.setUndoBufferSize(8*1024*1024);
  
  int res;
  res = pNdb->getDictionary()->createLogfileGroup(lg);
  if(res != 0){
    g_err << "Failed to create logfilegroup:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Undofile uf;
  uf.setPath("undofile01.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");
  
  res = pNdb->getDictionary()->createUndofile(uf);
  if(res != 0){
    g_err << "Failed to create undofile:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  uf.setPath("undofile02.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");
  
  res = pNdb->getDictionary()->createUndofile(uf);
  if(res != 0){
    g_err << "Failed to create undofile:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runCreateTablespace(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::Tablespace lg;
  lg.setName("DEFAULT-TS");
  lg.setExtentSize(1024*1024);
  lg.setDefaultLogfileGroup("DEFAULT-LG");

  int res;
  res = pNdb->getDictionary()->createTablespace(lg);
  if(res != 0){
    g_err << "Failed to create tablespace:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Datafile uf;
  uf.setPath("datafile01.dat");
  uf.setSize(10*1024*1024);
  uf.setTablespace("DEFAULT-TS");

  res = pNdb->getDictionary()->createDatafile(uf);
  if(res != 0){
    g_err << "Failed to create datafile:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}
int
runCreateDiskTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  

  NdbDictionary::Table tab = *ctx->getTab();
  tab.setTablespaceName("DEFAULT-TS");
  
  for(Uint32 i = 0; i<(Uint32)tab.getNoOfColumns(); i++)
    if(!tab.getColumn(i)->getPrimaryKey())
      tab.getColumn(i)->setStorageType(NdbDictionary::Column::StorageTypeDisk);
  
  int res;
  res = pNdb->getDictionary()->createTable(tab);
  if(res != 0){
    g_err << "Failed to create table:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

#include <NDBT_Tables.hpp>

int runFailAddFragment(NDBT_Context* ctx, NDBT_Step* step){
  static int acclst[] = { 3001, 6200, 6202 };
  static int tuplst[] = { 4007, 4008, 4009, 4010, 4011, 4012 };
  static int tuxlst[] = { 12001, 12002, 12003, 12004, 12005, 12006, 
                          6201, 6203 };
  static unsigned acccnt = sizeof(acclst)/sizeof(acclst[0]);
  static unsigned tupcnt = sizeof(tuplst)/sizeof(tuplst[0]);
  static unsigned tuxcnt = sizeof(tuxlst)/sizeof(tuxlst[0]);

  NdbRestarter restarter;
  int nodeId = restarter.getMasterNodeId();
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbDictionary::Table tab(*ctx->getTab());
  tab.setFragmentType(NdbDictionary::Object::FragAllLarge);

  int errNo = 0;
  char buf[100];
  if (NdbEnv_GetEnv("ERRNO", buf, sizeof(buf)))
  {
    errNo = atoi(buf);
    ndbout_c("Using errno: %u", errNo);
  }
  
  // ordered index on first few columns
  NdbDictionary::Index idx("X");
  idx.setTable(tab.getName());
  idx.setType(NdbDictionary::Index::OrderedIndex);
  idx.setLogging(false);
  for (int cnt = 0, i_hate_broken_compilers = 0;
       cnt < 3 &&
       i_hate_broken_compilers < tab.getNoOfColumns();
       i_hate_broken_compilers++) {
    if (NdbSqlUtil::check_column_for_ordered_index
        (tab.getColumn(i_hate_broken_compilers)->getType(), 0) == 0 &&
        tab.getColumn(i_hate_broken_compilers)->getStorageType() != 
        NdbDictionary::Column::StorageTypeDisk)
    {
      idx.addColumn(*tab.getColumn(i_hate_broken_compilers));
      cnt++;
    }
  }

  for (Uint32 i = 0; i<(Uint32)tab.getNoOfColumns(); i++)
  {
    if (tab.getColumn(i)->getStorageType() == 
        NdbDictionary::Column::StorageTypeDisk)
    {
      NDBT_Tables::create_default_tablespace(pNdb);
      break;
    }
  }

  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  (void)pDic->dropTable(tab.getName());

  int dump1 = DumpStateOrd::SchemaResourceSnapshot;
  int dump2 = DumpStateOrd::SchemaResourceCheckLeak;

  for (int l = 0; l < loops; l++) {
    for (unsigned i0 = 0; i0 < acccnt; i0++) {
      unsigned j = (l == 0 ? i0 : myRandom48(acccnt));
      int errval = acclst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_info << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);

      CHECK2(pDic->createTable(tab) != 0,
             "failed to fail after error insert " << errval);
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      CHECK2(pDic->createTable(tab) == 0,
             pDic->getNdbError());
      CHECK2(pDic->dropTable(tab.getName()) == 0,
             pDic->getNdbError());
    }
    for (unsigned i1 = 0; i1 < tupcnt; i1++) {
      unsigned j = (l == 0 ? i1 : myRandom48(tupcnt));
      int errval = tuplst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_info << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      CHECK2(pDic->createTable(tab) != 0,
             "failed to fail after error insert " << errval);
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      CHECK2(pDic->createTable(tab) == 0,
             pDic->getNdbError());
      CHECK2(pDic->dropTable(tab.getName()) == 0,
             pDic->getNdbError());
    }
    for (unsigned i2 = 0; i2 < tuxcnt; i2++) {
      unsigned j = (l == 0 ? i2 : myRandom48(tuxcnt));
      int errval = tuxlst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_info << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      CHECK2(pDic->createTable(tab) == 0,
             pDic->getNdbError());
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      CHECK2(pDic->createIndex(idx) != 0,
             "failed to fail after error insert " << errval);
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      CHECK2(pDic->createIndex(idx) == 0,
             pDic->getNdbError());
      CHECK2(pDic->dropTable(tab.getName()) == 0,
             pDic->getNdbError());
    }
  }
end:
  return result;
}

// NFNR

// Restarter controls dict ops : 1-run 2-pause 3-stop
// synced by polling...

static bool
send_dict_ops_cmd(NDBT_Context* ctx, Uint32 cmd)
{
  ctx->setProperty("DictOps_CMD", cmd);
  while (1) {
    if (ctx->isTestStopped())
      return false;
    if (ctx->getProperty("DictOps_ACK") == cmd)
      break;
    NdbSleep_MilliSleep(100);
  }
  return true;
}

static bool
recv_dict_ops_run(NDBT_Context* ctx)
{
  while (1) {
    if (ctx->isTestStopped())
      return false;
    Uint32 cmd = ctx->getProperty("DictOps_CMD");
    ctx->setProperty("DictOps_ACK", cmd);
    if (cmd == 1)
      break;
    if (cmd == 3)
      return false;
    NdbSleep_MilliSleep(100);
  }
  return true;
}

int
runRestarts(NDBT_Context* ctx, NDBT_Step* step)
{
  static int errlst_master[] = {   // non-crashing
    7175,       // send one fake START_PERMREF
    0 
  };
  static int errlst_node[] = {
    7174,       // crash before sending DICT_LOCK_REQ
    7176,       // pretend master does not support DICT lock
    7121,       // crash at receive START_PERMCONF
    0
  };
  const uint errcnt_master = sizeof(errlst_master)/sizeof(errlst_master[0]);
  const uint errcnt_node = sizeof(errlst_node)/sizeof(errlst_node[0]);

  myRandom48Init(NdbTick_CurrentMillisecond());
  NdbRestarter restarter;
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();

  for (int l = 0; l < loops && result == NDBT_OK; l++) {
    g_info << "1: === loop " << l << " ===" << endl;

    // assuming 2-way replicated

    int numnodes = restarter.getNumDbNodes();
    CHECK(numnodes >= 1);
    if (numnodes == 1)
      break;

    int masterNodeId = restarter.getMasterNodeId();
    CHECK(masterNodeId != -1);

    // for more complex cases need more restarter support methods

    int nodeIdList[2] = { 0, 0 };
    int nodeIdCnt = 0;

    if (numnodes >= 2) {
      int rand = myRandom48(numnodes);
      int nodeId = restarter.getRandomNotMasterNodeId(rand);
      CHECK(nodeId != -1);
      nodeIdList[nodeIdCnt++] = nodeId;
    }

    if (numnodes >= 4 && myRandom48(2) == 0) {
      int rand = myRandom48(numnodes);
      int nodeId = restarter.getRandomNodeOtherNodeGroup(nodeIdList[0], rand);
      CHECK(nodeId != -1);
      if (nodeId != masterNodeId)
        nodeIdList[nodeIdCnt++] = nodeId;
    }

    g_info << "1: master=" << masterNodeId << " nodes=" << nodeIdList[0] << "," << nodeIdList[1] << endl;

    const uint timeout = 60; //secs for node wait
    const unsigned maxsleep = 2000; //ms

    bool NF_ops = ctx->getProperty("Restart_NF_ops");
    uint NF_type = ctx->getProperty("Restart_NF_type");
    bool NR_ops = ctx->getProperty("Restart_NR_ops");
    bool NR_error = ctx->getProperty("Restart_NR_error");

    g_info << "1: " << (NF_ops ? "run" : "pause") << " dict ops" << endl;
    if (! send_dict_ops_cmd(ctx, NF_ops ? 1 : 2))
      break;
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    {
      for (int i = 0; i < nodeIdCnt; i++) {
        int nodeId = nodeIdList[i];

        bool nostart = true;
        bool abort = NF_type == 0 ? myRandom48(2) : (NF_type == 2);
        bool initial = myRandom48(2);

        char flags[40];
        strcpy(flags, "flags: nostart");
        if (abort)
          strcat(flags, ",abort");
        if (initial)
          strcat(flags, ",initial");

        g_info << "1: restart " << nodeId << " " << flags << endl;
        CHECK(restarter.restartOneDbNode(nodeId, initial, nostart, abort) == 0);
      }
    }

    g_info << "1: wait for nostart" << endl;
    CHECK(restarter.waitNodesNoStart(nodeIdList, nodeIdCnt, timeout) == 0);
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    int err_master = 0;
    int err_node[2] = { 0, 0 };

    if (NR_error) {
      err_master = errlst_master[l % errcnt_master];

      // limitation: cannot have 2 node restarts and crash_insert
      // one node may die for real (NF during startup)

      for (int i = 0; i < nodeIdCnt && nodeIdCnt == 1; i++) {
        err_node[i] = errlst_node[l % errcnt_node];

        // 7176 - no DICT lock protection

        if (err_node[i] == 7176) {
          g_info << "1: no dict ops due to error insert "
                 << err_node[i] << endl;
          NR_ops = false;
        }
      }
    }

    g_info << "1: " << (NR_ops ? "run" : "pause") << " dict ops" << endl;
    if (! send_dict_ops_cmd(ctx, NR_ops ? 1 : 2))
      break;
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "1: start nodes" << endl;
    CHECK(restarter.startNodes(nodeIdList, nodeIdCnt) == 0);

    if (NR_error) {
      {
        int err = err_master;
        if (err != 0) {
          g_info << "1: insert master error " << err << endl;
          CHECK(restarter.insertErrorInNode(masterNodeId, err) == 0);
        }
      }

      for (int i = 0; i < nodeIdCnt; i++) {
        int nodeId = nodeIdList[i];

        int err = err_node[i];
        if (err != 0) {
          g_info << "1: insert node " << nodeId << " error " << err << endl;
          CHECK(restarter.insertErrorInNode(nodeId, err) == 0);
        }
      }
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "1: wait cluster started" << endl;
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "1: restart done" << endl;
  }

  g_info << "1: stop dict ops" << endl;
  send_dict_ops_cmd(ctx, 3);

  return result;
}

int
runDictOps(NDBT_Context* ctx, NDBT_Step* step)
{
  myRandom48Init(NdbTick_CurrentMillisecond());
  int result = NDBT_OK;

  for (int l = 0; result == NDBT_OK; l++) {
    if (! recv_dict_ops_run(ctx))
      break;
    
    g_info << "2: === loop " << l << " ===" << endl;

    Ndb* pNdb = GETNDB(step);
    NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
    const NdbDictionary::Table* pTab = ctx->getTab();
    //const char* tabName = pTab->getName(); //XXX what goes on?
    char tabName[40];
    strcpy(tabName, pTab->getName());

    const unsigned long maxsleep = 100; //ms

    g_info << "2: create table" << endl;
    {
      uint count = 0;
    try_create:
      count++;
      if (pDic->createTable(*pTab) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << tabName << ": create failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_create;
      }
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "2: verify create" << endl;
    const NdbDictionary::Table* pTab2 = pDic->getTable(tabName);
    if (pTab2 == NULL) {
      const NdbError err = pDic->getNdbError();
      g_err << "2: " << tabName << ": verify create: " << err << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    // replace by the Retrieved table
    pTab = pTab2;

    // create indexes
    const char** indlist = NDBT_Tables::getIndexes(tabName);
    uint indnum = 0;
    while (indlist != 0 && *indlist != 0) {
      uint count = 0;
    try_create_index:
      count++;
      if (count == 1)
        g_info << "2: create index " << indnum << " " << *indlist << endl;
      NdbDictionary::Index ind;
      char indName[200];
      sprintf(indName, "%s_X%u", tabName, indnum);
      ind.setName(indName);
      ind.setTable(tabName);
      if (strcmp(*indlist, "UNIQUE") == 0) {
        ind.setType(NdbDictionary::Index::UniqueHashIndex);
        ind.setLogging(pTab->getLogging());
      } else if (strcmp(*indlist, "ORDERED") == 0) {
        ind.setType(NdbDictionary::Index::OrderedIndex);
        ind.setLogging(false);
      } else {
        assert(false);
      }
      const char** indtemp = indlist;
      while (*++indtemp != 0) {
        ind.addColumn(*indtemp);
      }
      if (pDic->createIndex(ind) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << indName << ": create failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_create_index;
      }
      indlist = ++indtemp;
      indnum++;
    }
    if (result == NDBT_FAILED)
      break;

    uint indcount = indnum;

    int records = myRandom48(ctx->getNumRecords());
    g_info << "2: load " << records << " records" << endl;
    HugoTransactions hugoTrans(*pTab);
    if (hugoTrans.loadTable(pNdb, records) != 0) {
      // XXX get error code from hugo
      g_err << "2: " << tabName << ": load failed" << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    // drop indexes
    indnum = 0;
    while (indnum < indcount) {
      uint count = 0;
    try_drop_index:
      count++;
      if (count == 1)
        g_info << "2: drop index " << indnum << endl;
      char indName[200];
      sprintf(indName, "%s_X%u", tabName, indnum);
      if (pDic->dropIndex(indName, tabName) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << indName << ": drop failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_drop_index;
      }
      indnum++;
    }
    if (result == NDBT_FAILED)
      break;

    g_info << "2: drop" << endl;
    {
      uint count = 0;
    try_drop:
      count++;
      if (pDic->dropTable(tabName) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << tabName << ": drop failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_drop;
      }
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "2: verify drop" << endl;
    const NdbDictionary::Table* pTab3 = pDic->getTable(tabName);
    if (pTab3 != NULL) {
      g_err << "2: " << tabName << ": verify drop: table exists" << endl;
      result = NDBT_FAILED;
      break;
    }
    if (pDic->getNdbError().code != 709 &&
        pDic->getNdbError().code != 723) {
      const NdbError err = pDic->getNdbError();
      g_err << "2: " << tabName << ": verify drop: " << err << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));
  }

  return result;
}

int
runBug21755(NDBT_Context* ctx, NDBT_Step* step)
{
  char buf[256];
  NdbRestarter res;
  NdbDictionary::Table pTab0 = * ctx->getTab();
  NdbDictionary::Table pTab1 = pTab0;

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  
  if (pDic->createTable(pTab0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Index idx0;
  BaseString::snprintf(buf, sizeof(buf), "%s-idx", pTab0.getName());  
  idx0.setName(buf);
  idx0.setType(NdbDictionary::Index::OrderedIndex);
  idx0.setTable(pTab0.getName());
  idx0.setStoredIndex(false);
  for (Uint32 i = 0; i<(Uint32)pTab0.getNoOfColumns(); i++)
  {
    const NdbDictionary::Column * col = pTab0.getColumn(i);
    if(col->getPrimaryKey()){
      idx0.addIndexColumn(col->getName());
    }
  }
  
  if (pDic->createIndex(idx0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  BaseString::snprintf(buf, sizeof(buf), "%s-2", pTab1.getName());
  pTab1.setName(buf);

  if (pDic->createTable(pTab1))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  {
    HugoTransactions t0 (*pDic->getTable(pTab0.getName()));
    t0.loadTable(pNdb, 1000);
  }

  {
    HugoTransactions t1 (*pDic->getTable(pTab1.getName()));
    t1.loadTable(pNdb, 1000);
  }
  
  int node = res.getRandomNotMasterNodeId(rand());
  res.restartOneDbNode(node, false, true, true);
  
  if (pDic->dropTable(pTab1.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  BaseString::snprintf(buf, sizeof(buf), "%s-idx2", pTab0.getName());    
  idx0.setName(buf);
  if (pDic->createIndex(idx0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  res.waitNodesNoStart(&node, 1);
  res.startNodes(&node, 1);
  
  if (res.waitClusterStarted())
  {
    return NDBT_FAILED;
  }
  
  if (pDic->dropTable(pTab0.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

static
int
create_tablespace(NdbDictionary::Dictionary* pDict, 
                  const char * lgname, 
                  const char * tsname, 
                  const char * dfname)
{
  NdbDictionary::Tablespace ts;
  ts.setName(tsname);
  ts.setExtentSize(1024*1024);
  ts.setDefaultLogfileGroup(lgname);
  
  if(pDict->createTablespace(ts) != 0)
  {
    g_err << "Failed to create tablespace:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  NdbDictionary::Datafile df;
  df.setPath(dfname);
  df.setSize(1*1024*1024);
  df.setTablespace(tsname);
  
  if(pDict->createDatafile(df) != 0)
  {
    g_err << "Failed to create datafile:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  return 0;
}

int
runBug24631(NDBT_Context* ctx, NDBT_Step* step)
{
  char tsname[256];
  char dfname[256];
  char lgname[256];
  char ufname[256];
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  
  NdbDictionary::Dictionary::List list;
  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;
  
  const char * lgfound = 0;
  
  for (Uint32 i = 0; i<list.count; i++)
  {
    switch(list.elements[i].type){
    case NdbDictionary::Object::LogfileGroup:
      lgfound = list.elements[i].name;
      break;
    default:
      break;
    }
    if (lgfound)
      break;
  }

  if (lgfound == 0)
  {
    BaseString::snprintf(lgname, sizeof(lgname), "LG-%u", rand());
    NdbDictionary::LogfileGroup lg;
    
    lg.setName(lgname);
    lg.setUndoBufferSize(8*1024*1024);
    if(pDict->createLogfileGroup(lg) != 0)
    {
      g_err << "Failed to create logfilegroup:"
	    << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }

    NdbDictionary::Undofile uf;
    BaseString::snprintf(ufname, sizeof(ufname), "%s-%u", lgname, rand());
    uf.setPath(ufname);
    uf.setSize(2*1024*1024);
    uf.setLogfileGroup(lgname);
    
    if(pDict->createUndofile(uf) != 0)
    {
      g_err << "Failed to create undofile:"
            << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    BaseString::snprintf(lgname, sizeof(lgname), "%s", lgfound);
  }

  BaseString::snprintf(tsname, sizeof(tsname), "TS-%u", rand());
  BaseString::snprintf(dfname, sizeof(dfname), "%s-%u.dat", tsname, rand());

  if (create_tablespace(pDict, lgname, tsname, dfname))
    return NDBT_FAILED;

  
  int node = res.getRandomNotMasterNodeId(rand());
  res.restartOneDbNode(node, false, true, true);
  NdbSleep_SecSleep(3);

  if (pDict->dropDatafile(pDict->getDatafile(0, dfname)) != 0)
  {
    g_err << "Failed to drop datafile: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDict->dropTablespace(pDict->getTablespace(tsname)) != 0)
  {
    g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (res.waitNodesNoStart(&node, 1))
    return NDBT_FAILED;
  
  res.startNodes(&node, 1);
  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  if (create_tablespace(pDict, lgname, tsname, dfname))
    return NDBT_FAILED;

  if (pDict->dropDatafile(pDict->getDatafile(0, dfname)) != 0)
  {
    g_err << "Failed to drop datafile: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDict->dropTablespace(pDict->getTablespace(tsname)) != 0)
  {
    g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  if (lgfound == 0)
  {
    if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lgname)) != 0)
      return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runBug29186(NDBT_Context* ctx, NDBT_Step* step)
{
  int lgError = 15000;
  int tsError = 16000;
  char lgname[256];
  char ufname[256];
  char tsname[256];
  char dfname[256];

  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  NdbDictionary::Dictionary::List list;

  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;

  // 1.create logfile group
  const char * lgfound = 0;

  for (Uint32 i = 0; i<list.count; i++)
  {
    switch(list.elements[i].type){
    case NdbDictionary::Object::LogfileGroup:
      lgfound = list.elements[i].name;
      break;
    default:
      break;
    }
    if (lgfound)
      break;
  }

  if (lgfound == 0)
  {
    BaseString::snprintf(lgname, sizeof(lgname), "LG-%u", rand());
    NdbDictionary::LogfileGroup lg;

    lg.setName(lgname);
    lg.setUndoBufferSize(8*1024*1024);
    if(pDict->createLogfileGroup(lg) != 0)
    {
      g_err << "Failed to create logfilegroup:"
            << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    BaseString::snprintf(lgname, sizeof(lgname), "%s", lgfound);
  }

  if(restarter.waitClusterStarted(60)){
    g_err << "waitClusterStarted failed"<< endl;
    return NDBT_FAILED;
  }
 
  if(restarter.insertErrorInAllNodes(lgError) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }

  g_info << "error inserted"  << endl;
  g_info << "waiting some before add log file"  << endl;
  g_info << "starting create log file group"  << endl;

  NdbDictionary::Undofile uf;
  BaseString::snprintf(ufname, sizeof(ufname), "%s-%u", lgname, rand());
  uf.setPath(ufname);
  uf.setSize(2*1024*1024);
  uf.setLogfileGroup(lgname);

  if(pDict->createUndofile(uf) == 0)
  {
    g_err << "Create log file group should fail on error_insertion " << lgError << endl;
    return NDBT_FAILED;
  }

  //clear lg error
  if(restarter.insertErrorInAllNodes(15099) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(5);

  //lg error has been cleared, so we can add undo file
  if(pDict->createUndofile(uf) != 0)
  {
    g_err << "Failed to create undofile:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if(restarter.waitClusterStarted(60)){
    g_err << "waitClusterStarted failed"<< endl;
    return NDBT_FAILED;
  }

  if(restarter.insertErrorInAllNodes(tsError) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  g_info << "error inserted"  << endl;
  g_info << "waiting some before create table space"  << endl;
  g_info << "starting create table space"  << endl;

  //r = runCreateTablespace(ctx, step);
  BaseString::snprintf(tsname,  sizeof(tsname), "TS-%u", rand());
  BaseString::snprintf(dfname, sizeof(dfname), "%s-%u-1.dat", tsname, rand());

  NdbDictionary::Tablespace ts;
  ts.setName(tsname);
  ts.setExtentSize(1024*1024);
  ts.setDefaultLogfileGroup(lgname);

  if(pDict->createTablespace(ts) != 0)
  {
    g_err << "Failed to create tablespace:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Datafile df;
  df.setPath(dfname);
  df.setSize(1*1024*1024);
  df.setTablespace(tsname);

  if(pDict->createDatafile(df) == 0)
  {
    g_err << "Create table space should fail on error_insertion " << tsError << endl;
    return NDBT_FAILED;
  }
  //Clear the inserted error
  if(restarter.insertErrorInAllNodes(16099) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(5);

  if (pDict->dropTablespace(pDict->getTablespace(tsname)) != 0)
  {
    g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (lgfound == 0)
  {
    if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lgname)) != 0)
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

struct RandSchemaOp
{
  RandSchemaOp(unsigned * randseed = 0) {
    if (randseed == 0)
    {
      ownseed = (unsigned)NdbTick_CurrentMillisecond();
      seed = &ownseed;
    }
    else
    {
      seed = randseed;
    }
  }
  struct Obj 
  { 
    BaseString m_name;
    Uint32 m_type;
    struct Obj* m_parent;
    Vector<Obj*> m_dependant;
  };

  Vector<Obj*> m_objects;

  int schema_op(Ndb*);
  int validate(Ndb*);
  int cleanup(Ndb*);

  Obj* get_obj(Uint32 mask);
  int create_table(Ndb*);
  int create_index(Ndb*, Obj*);
  int alter_table(Ndb*, Obj*);
  int drop_obj(Ndb*, Obj*);

  void remove_obj(Obj*);
private:
  unsigned * seed;
  unsigned ownseed;
};

template class Vector<RandSchemaOp::Obj*>;

int
RandSchemaOp::schema_op(Ndb* ndb)
{
  struct Obj* obj = 0;
  Uint32 type = 0;
loop:
  switch(ndb_rand_r(seed) % 5){
  case 0:
    return create_table(ndb);
  case 1:
    if ((obj = get_obj(1 << NdbDictionary::Object::UserTable)) == 0)
      goto loop;
    return create_index(ndb, obj);
  case 2:
    type = (1 << NdbDictionary::Object::UserTable);
    goto drop_object;
  case 3:
    type = 
      (1 << NdbDictionary::Object::UniqueHashIndex) |
      (1 << NdbDictionary::Object::OrderedIndex);    
    goto drop_object;
  case 4:
    if ((obj = get_obj(1 << NdbDictionary::Object::UserTable)) == 0)
      goto loop;
    return alter_table(ndb, obj);
  default:
    goto loop;
  }

drop_object:
  if ((obj = get_obj(type)) == 0)
    goto loop;
  return drop_obj(ndb, obj);
}

RandSchemaOp::Obj*
RandSchemaOp::get_obj(Uint32 mask)
{
  Vector<Obj*> tmp;
  for (Uint32 i = 0; i<m_objects.size(); i++)
  {
    if ((1 << m_objects[i]->m_type) & mask)
      tmp.push_back(m_objects[i]);
  }

  if (tmp.size())
  {
    return tmp[ndb_rand_r(seed)%tmp.size()];
  }
  return 0;
}

int
RandSchemaOp::create_table(Ndb* ndb)
{
  int numTables = NDBT_Tables::getNumTables();
  int num = ndb_rand_r(seed) % numTables;
  NdbDictionary::Table pTab = * NDBT_Tables::getTable(num);
  
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  pTab.setForceVarPart(true);

  if (pDict->getTable(pTab.getName()))
  {
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "%s-%d", 
                         pTab.getName(), ndb_rand_r(seed));
    pTab.setName(buf);
    if (pDict->createTable(pTab))
      return NDBT_FAILED;
  }
  else
  {
    if (NDBT_Tables::createTable(ndb, pTab.getName()))
    {
      return NDBT_FAILED;
    }
  }

  ndbout_c("create table %s",  pTab.getName());
  const NdbDictionary::Table* tab2 = pDict->getTable(pTab.getName());
  HugoTransactions trans(*tab2);
  trans.loadTable(ndb, 1000);

  Obj *obj = new Obj;
  obj->m_name.assign(pTab.getName());
  obj->m_type = NdbDictionary::Object::UserTable;
  obj->m_parent = 0;
  m_objects.push_back(obj);
  
  return NDBT_OK;
}

int
RandSchemaOp::create_index(Ndb* ndb, Obj* tab)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  const NdbDictionary::Table * pTab = pDict->getTable(tab->m_name.c_str());

  if (pTab == 0)
  {
    return NDBT_FAILED;
  }

  bool ordered = ndb_rand_r(seed) & 1;
  bool stored = ndb_rand_r(seed) & 1;

  Uint32 type = ordered ? 
    NdbDictionary::Index::OrderedIndex :
    NdbDictionary::Index::UniqueHashIndex;
  
  char buf[255];
  BaseString::snprintf(buf, sizeof(buf), "%s-%s", 
                       pTab->getName(),
                       ordered ? "OI" : "UI");
  
  if (pDict->getIndex(buf, pTab->getName()))
  {
    // Index exists...let it be ok
    return NDBT_OK;
  }
  
  ndbout_c("create index %s", buf);
  NdbDictionary::Index idx0;
  idx0.setName(buf);
  idx0.setType((NdbDictionary::Index::Type)type);
  idx0.setTable(pTab->getName());
  idx0.setStoredIndex(ordered ? false : stored);

  for (Uint32 i = 0; i<(Uint32)pTab->getNoOfColumns(); i++)
  {
    if (pTab->getColumn(i)->getPrimaryKey())
      idx0.addColumn(pTab->getColumn(i)->getName());
  }
  if (pDict->createIndex(idx0))
  {
    ndbout << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  Obj *obj = new Obj;
  obj->m_name.assign(buf);
  obj->m_type = type;
  obj->m_parent = tab;
  m_objects.push_back(obj);
  
  tab->m_dependant.push_back(obj);
  return NDBT_OK;
}

int
RandSchemaOp::drop_obj(Ndb* ndb, Obj* obj)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  
  if (obj->m_type == NdbDictionary::Object::UserTable)
  {
    ndbout_c("drop table %s", obj->m_name.c_str());
    /**
     * Drop of table automatically drops all indexes
     */
    if (pDict->dropTable(obj->m_name.c_str()))
    {
      return NDBT_FAILED;
    }
    while(obj->m_dependant.size())
    {
      remove_obj(obj->m_dependant[0]);
    }
    remove_obj(obj);
  }
  else if (obj->m_type == NdbDictionary::Object::UniqueHashIndex ||
           obj->m_type == NdbDictionary::Object::OrderedIndex)
  {
    ndbout_c("drop index %s", obj->m_name.c_str());
    if (pDict->dropIndex(obj->m_name.c_str(),
                         obj->m_parent->m_name.c_str()))
    {
      return NDBT_FAILED;
    }
    remove_obj(obj);
  }
  return NDBT_OK;
}

void
RandSchemaOp::remove_obj(Obj* obj)
{
  Uint32 i;
  if (obj->m_parent)
  {
    bool found = false;
    for (i = 0; i<obj->m_parent->m_dependant.size(); i++)
    {
      if (obj->m_parent->m_dependant[i] == obj)
      {
        found = true;
        obj->m_parent->m_dependant.erase(i);
        break;
      }
    }
    assert(found);
  }

  {
    bool found = false;
    for (i = 0; i<m_objects.size(); i++)
    {
      if (m_objects[i] == obj)
      {
        found = true;
        m_objects.erase(i);
        break;
      }
    }
    assert(found);
  }
  delete obj;
}

int
RandSchemaOp::alter_table(Ndb* ndb, Obj* obj)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  const NdbDictionary::Table * pOld = pDict->getTable(obj->m_name.c_str());
  NdbDictionary::Table tNew = * pOld;

  BaseString ops;
  unsigned mask = 3;

  unsigned type;
  while (ops.length() == 0 && (mask != 0))
  {
    switch((type = (ndb_rand_r(seed) & 1))){
    default:
    case 0:{
      if ((mask & (1 << type)) == 0)
        break;
      BaseString name;
      name.assfmt("newcol_%d", tNew.getNoOfColumns());
      NdbDictionary::Column col(name.c_str());
      col.setType(NdbDictionary::Column::Unsigned);
      col.setDynamic(true);
      col.setPrimaryKey(false);
      col.setNullable(true);
      NdbDictionary::Table save = tNew;
      tNew.addColumn(col);
      if (!pDict->supportedAlterTable(* pOld, tNew))
      {
        ndbout_c("not supported...");
        mask &= ~(1 << type);
        tNew = save;
        break;
      }
      ops.append(" addcol");
      break;
    }
    case 1:{
      BaseString name;
      do
      {
        unsigned no = ndb_rand_r(seed);
        name.assfmt("%s_%u", pOld->getName(), no);
      } while (pDict->getTable(name.c_str()));
      tNew.setName(name.c_str());
      ops.appfmt(" rename: %s", name.c_str());
      break;
    }

    }
  }

  if (ops.length())
  {
    ndbout_c("altering %s ops: %s", pOld->getName(), ops.c_str());
    if (pDict->alterTable(*pOld, tNew) != 0)
    {
      g_err << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    pDict->invalidateTable(pOld->getName());
    if (strcmp(pOld->getName(), tNew.getName()))
    {
      obj->m_name.assign(tNew.getName());
    }
  }

  return NDBT_OK;
}


int
RandSchemaOp::validate(Ndb* ndb)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  for (Uint32 i = 0; i<m_objects.size(); i++)
  {
    if (m_objects[i]->m_type == NdbDictionary::Object::UserTable)
    {
      const NdbDictionary::Table* tab2 = 
        pDict->getTable(m_objects[i]->m_name.c_str());
      HugoTransactions trans(*tab2);
      trans.scanUpdateRecords(ndb, 1000);
      trans.clearTable(ndb);
      trans.loadTable(ndb, 1000);
    }
  }
  
  return NDBT_OK;
}

/*
      SystemTable = 1,        ///< System table
      UserTable = 2,          ///< User table (may be temporary)
      UniqueHashIndex = 3,    ///< Unique un-ordered hash index
      OrderedIndex = 6,       ///< Non-unique ordered index
      HashIndexTrigger = 7,   ///< Index maintenance, internal
      IndexTrigger = 8,       ///< Index maintenance, internal
      SubscriptionTrigger = 9,///< Backup or replication, internal
      ReadOnlyConstraint = 10,///< Trigger, internal
      Tablespace = 20,        ///< Tablespace
      LogfileGroup = 21,      ///< Logfile group
      Datafile = 22,          ///< Datafile
      Undofile = 23           ///< Undofile
*/

int
RandSchemaOp::cleanup(Ndb* ndb)
{
  Int32 i;
  for (i = m_objects.size() - 1; i >= 0; i--)
  {
    switch(m_objects[i]->m_type){
    case NdbDictionary::Object::UniqueHashIndex:
    case NdbDictionary::Object::OrderedIndex:        
      if (drop_obj(ndb, m_objects[i]))
        return NDBT_FAILED;
      
      break;
    default:
      break;
    }
  }

  for (i = m_objects.size() - 1; i >= 0; i--)
  {
    switch(m_objects[i]->m_type){
    case NdbDictionary::Object::UserTable:
      if (drop_obj(ndb, m_objects[i]))
        return NDBT_FAILED;
      break;
    default:
      break;
    }
  }
  
  assert(m_objects.size() == 0);
  return NDBT_OK;
}

extern unsigned opt_seed;

int
runDictRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();

  unsigned seed = opt_seed;
  NdbMixRestarter res(&seed);
  RandSchemaOp dict(&seed);
  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  if (res.init(ctx, step))
    return NDBT_FAILED;
  
  for (int i = 0; i<loops; i++)
  {
    for (Uint32 j = 0; j<10; j++)
      if (dict.schema_op(pNdb))
        return NDBT_FAILED;
    
    if (res.dostep(ctx, step))
      return NDBT_FAILED;

    if (dict.validate(pNdb))
      return NDBT_FAILED;
  }

  if (res.finish(ctx, step))
    return NDBT_FAILED;

  if (dict.validate(pNdb))
    return NDBT_FAILED;
  
  if (dict.cleanup(pNdb))
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int
runBug29501(NDBT_Context* ctx, NDBT_Step* step) {
  NdbRestarter res;
  NdbDictionary::LogfileGroup lg;
  lg.setName("DEFAULT-LG");
  lg.setUndoBufferSize(8*1024*1024);

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  int node = res.getRandomNotMasterNodeId(rand());
  res.restartOneDbNode(node, true, true, false);

  if(pDict->createLogfileGroup(lg) != 0){
    g_err << "Failed to create logfilegroup:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Undofile uf;
  uf.setPath("undofile01.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");

  if(pDict->createUndofile(uf) != 0){
    g_err << "Failed to create undofile:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  res.waitNodesNoStart(&node, 1);
  res.startNodes(&node, 1);

  if (res.waitClusterStarted()){
  	g_err << "Node restart failed"
  	<< endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
  }

  if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lg.getName())) != 0){
  	g_err << "Drop of LFG Failed"
  	<< endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runDropDDObjects(NDBT_Context* ctx, NDBT_Step* step){
  //Purpose is to drop all tables, data files, Table spaces and LFG's
  Uint32 i = 0;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  
  NdbDictionary::Dictionary::List list;
  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;
  
  //Search the list and drop all tables found
  const char * tableFound = 0;
  for (i = 0; i < list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::UserTable:
        tableFound = list.elements[i].name;
        if(tableFound != 0){
          if(strcmp(list.elements[i].database, "TEST_DB") == 0 &&
             !is_prefix(tableFound, "NDB$BLOB"))
          { 
      	    if(pDict->dropTable(tableFound) != 0){
              g_err << "Failed to drop table: " << tableFound << pDict->getNdbError() << endl;
              return NDBT_FAILED;
            }
          }
        }
        tableFound = 0;
        break;
      default:
        break;
    }
  }
 
  //Search the list and drop all data file found
  const char * dfFound = 0;
  for (i = 0; i < list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::Datafile:
        dfFound = list.elements[i].name;
        if(dfFound != 0){
      	  if(pDict->dropDatafile(pDict->getDatafile(0, dfFound)) != 0){
            g_err << "Failed to drop datafile: " << pDict->getNdbError() << endl;
            return NDBT_FAILED;
          }
        }
        dfFound = 0;
        break;
      default:
        break;
    }
  }

  //Search the list and drop all Table Spaces Found 
  const char * tsFound  = 0;
  for (i = 0; i <list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::Tablespace:
        tsFound = list.elements[i].name;
        if(tsFound != 0){
          if(pDict->dropTablespace(pDict->getTablespace(tsFound)) != 0){
            g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
            return NDBT_FAILED;
          }
        }
        tsFound = 0;
        break;
      default:
        break;
    }
  }

  //Search the list and drop all LFG Found
  //Currently only 1 LGF is supported, but written for future 
  //when more then one is supported. 
  const char * lgFound  = 0;
  for (i = 0; i < list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::LogfileGroup:
        lgFound = list.elements[i].name;
        if(lgFound != 0){
          if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lgFound)) != 0){
            g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
            return NDBT_FAILED;
          }
       }   
        lgFound = 0;
        break;
      default:
        break;
    }
  }

  return NDBT_OK;
}

int
runWaitStarted(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  restarter.waitClusterStarted(300);

  NdbSleep_SecSleep(3);
  return NDBT_OK;
}

int
testDropDDObjectsSetup(NDBT_Context* ctx, NDBT_Step* step){
  //Purpose is to setup to test DropDDObjects
  char tsname[256];
  char dfname[256];

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  NdbDictionary::LogfileGroup lg;
  lg.setName("DEFAULT-LG");
  lg.setUndoBufferSize(8*1024*1024);


  if(pDict->createLogfileGroup(lg) != 0){
    g_err << "Failed to create logfilegroup:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Undofile uf;
  uf.setPath("undofile01.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");

  if(pDict->createUndofile(uf) != 0){
    g_err << "Failed to create undofile:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  BaseString::snprintf(tsname, sizeof(tsname), "TS-%u", rand());
  BaseString::snprintf(dfname, sizeof(dfname), "%s-%u.dat", tsname, rand());

  if (create_tablespace(pDict, lg.getName(), tsname, dfname)){
  	g_err << "Failed to create undofile:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runBug36072(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  NdbRestarter res;

  int err[] = { 6016, 6017 };
  for (Uint32 i = 0; i<2; i++)
  {
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    if (res.dumpStateAllNodes(val2, 2))
      return NDBT_FAILED;

    if (res.insertErrorInAllNodes(932)) // arbit
      return NDBT_FAILED;

    if (res.insertErrorInAllNodes(err[i]))
      return NDBT_FAILED;

    NdbDictionary::LogfileGroup lg;
    lg.setName("DEFAULT-LG");
    lg.setUndoBufferSize(8*1024*1024);

    NdbDictionary::Undofile uf;
    uf.setPath("undofile01.dat");
    uf.setSize(5*1024*1024);
    uf.setLogfileGroup("DEFAULT-LG");

    int r = pDict->createLogfileGroup(lg);
    if (i != 0)
    {
      if (r)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
      pDict->createUndofile(uf);
    }

    if (res.waitClusterNoStart())
      return NDBT_FAILED;

    res.startAll();
    if (res.waitClusterStarted())
      return NDBT_FAILED;

    if (i == 0)
    {
      NdbDictionary::LogfileGroup lg2 = pDict->getLogfileGroup("DEFAULT-LG");
      NdbError err= pDict->getNdbError();
      if( (int) err.classification == (int) ndberror_cl_none)
      {
        ndbout << __LINE__ << endl;
        return NDBT_FAILED;
      }

      if (pDict->createLogfileGroup(lg) != 0)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }
    else
    {
      NdbDictionary::Undofile uf2 = pDict->getUndofile(0, "undofile01.dat");
      NdbError err= pDict->getNdbError();
      if( (int) err.classification == (int) ndberror_cl_none)
      {
        ndbout << __LINE__ << endl;
        return NDBT_FAILED;
      }

      if (pDict->createUndofile(uf) != 0)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }

    {
      NdbDictionary::LogfileGroup lg2 = pDict->getLogfileGroup("DEFAULT-LG");
      NdbError err= pDict->getNdbError();
      if( (int) err.classification != (int) ndberror_cl_none)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }

      if (pDict->dropLogfileGroup(lg2))
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}

int
restartClusterInitial(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;

  res.restartAll(NdbRestarter::NRRF_INITIAL |
                 NdbRestarter::NRRF_NOSTART |
                 NdbRestarter::NRRF_ABORT);
  if (res.waitClusterNoStart())
    return NDBT_FAILED;

  res.startAll();
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  return NDBT_OK;
}


int
DropDDObjectsVerify(NDBT_Context* ctx, NDBT_Step* step){
  //Purpose is to verify test DropDDObjects worked
  Uint32 i = 0;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  NdbDictionary::Dictionary::List list;
  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;

    bool ddFound  = false;
  for (i = 0; i <list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::Tablespace:
        ddFound = true;
        break;
      case NdbDictionary::Object::LogfileGroup:
        ddFound = true;
        break;
      default:
        break;
    }
    if(ddFound == true){
      g_err << "DropDDObjects Failed: DD found:"
        << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

// Bug48604

// string messages between local/remote steps identified by stepNo-1
// each Msg<loc><rem> waits for Ack<loc><rem>

static const uint MaxMsg = 100;

static bool
send_msg(NDBT_Context* ctx, int loc, int rem, const char* msg)
{
  char msgName[20], ackName[20];
  sprintf(msgName, "Msg%d%d", loc, rem);
  sprintf(ackName, "Ack%d%d", loc, rem);
  g_info << loc << ": send to:" << rem << " msg:" << msg << endl;
  ctx->setProperty(msgName, msg);
  int cnt = 0;
  while (1)
  {
    if (ctx->isTestStopped())
      return false;
    int ret;
    if ((ret = ctx->getProperty(ackName, (Uint32)0)) != 0)
      break;
    if (++cnt % 100 == 0)
      g_info << loc << ": send to:" << rem << " wait for ack" << endl;
    NdbSleep_MilliSleep(10);
  }
  ctx->setProperty(ackName, (Uint32)0);
  return true;
}

static bool
poll_msg(NDBT_Context* ctx, int loc, int rem, char* msg)
{
  char msgName[20], ackName[20];
  sprintf(msgName, "Msg%d%d", rem, loc);
  sprintf(ackName, "Ack%d%d", rem, loc);
  const char* ptr;
  if ((ptr = ctx->getProperty(msgName, (char*)0)) != 0 && ptr[0] != 0)
  {
    assert(strlen(ptr) < MaxMsg);
    memset(msg, 0, MaxMsg);
    strcpy(msg, ptr);
    g_info << loc << ": recv from:" << rem << " msg:" << msg << endl;
    ctx->setProperty(msgName, "");
    ctx->setProperty(ackName, (Uint32)1);
    return true;
  }
  return false;
}

static int
recv_msg(NDBT_Context* ctx, int loc, int rem, char* msg)
{
  uint cnt = 0;
  while (1)
  {
    if (ctx->isTestStopped())
      return false;
    if (poll_msg(ctx, loc, rem, msg))
      break;
    if (++cnt % 100 == 0)
      g_info << loc << ": recv from:" << rem << " wait for msg" << endl;
    NdbSleep_MilliSleep(10);
  }
  return true;
}

const char* tabName_Bug48604 = "TBug48604";
const char* indName_Bug48604 = "TBug48604X1";

static const NdbDictionary::Table*
runBug48604createtable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Table* pTab = 0;
  int result = NDBT_OK;
  do
  {
    NdbDictionary::Table tab(tabName_Bug48604);
    {
      NdbDictionary::Column col("a");
      col.setType(NdbDictionary::Column::Unsigned);
      col.setPrimaryKey(true);
      tab.addColumn(col);
    }
    {
      NdbDictionary::Column col("b");
      col.setType(NdbDictionary::Column::Unsigned);
      col.setNullable(false);
      tab.addColumn(col);
    }
    CHECK(pDic->createTable(tab) == 0);
    CHECK((pTab = pDic->getTable(tabName_Bug48604)) != 0);
  }
  while (0);
  return pTab;
}

static const NdbDictionary::Index*
runBug48604createindex(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Index* pInd = 0;
  int result = NDBT_OK;
  do {
    NdbDictionary::Index ind(indName_Bug48604);
    ind.setTable(tabName_Bug48604);
    ind.setType(NdbDictionary::Index::OrderedIndex);
    ind.setLogging(false);
    ind.addColumn("b");
    g_info << "index create.." << endl;
    CHECK(pDic->createIndex(ind) == 0);
    CHECK((pInd = pDic->getIndex(indName_Bug48604, tabName_Bug48604)) != 0);
    g_info << "index created" << endl;
    return pInd;
  }
  while (0);
  return pInd;
}

int
runBug48604(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Table* pTab = 0;
  const NdbDictionary::Index* pInd = 0;
  (void)pDic->dropTable(tabName_Bug48604);
  int loc = step->getStepNo() - 1;
  assert(loc == 0);
  g_err << "main" << endl;
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  char msg[MaxMsg];

  do
  {
    CHECK((pTab = runBug48604createtable(ctx, step)) != 0);
    CHECK(send_msg(ctx, 0, 1, "s"));

    int loop = 0;
    while (result == NDBT_OK && loop++ < loops)
    {
      g_err << "loop:" << loop << endl;
      {
        // create index fully while uncommitted ops wait
        const char* ops[][3] =
        {
          { "ozin", "oc", "oa" },       // 0: before 1-2: after
          { "oziun", "oc", "oa" },
          { "ozidn", "oc", "oa" },
          { "ozicun", "oc", "oa" },
          { "ozicuuun", "oc", "oa" },
          { "ozicdn", "oc", "oa" },
          { "ozicdin", "oc", "oa" },
          { "ozicdidiuuudidn", "oc", "oa" },
          { "ozicdidiuuudidin", "oc", "oa" }
        };
        const int cnt = sizeof(ops)/sizeof(ops[0]);
        int i;
        for (i = 0; result == NDBT_OK && i < cnt; i++)
        {
          int j;
          for (j = 1; result == NDBT_OK && j <= 2; j++)
          {
            if (ops[i][j] == 0)
              continue;
            CHECK(send_msg(ctx, 0, 1, ops[i][0]));
            CHECK(recv_msg(ctx, 0, 1, msg) && msg[0] == 'o');
            CHECK((pInd = runBug48604createindex(ctx, step)) != 0);
            CHECK(send_msg(ctx, 0, 1, ops[i][j]));
            CHECK(recv_msg(ctx, 0, 1, msg) && msg[0] == 'o');

            CHECK(pDic->dropIndex(indName_Bug48604, tabName_Bug48604) == 0);
            g_info << "index dropped" << endl;
          }
        }
      }
    }
  }
  while (0);

  (void)send_msg(ctx, 0, 1, "x");
  ctx->stopTest();
  g_err << "main: exit:" << result << endl;
  return result;
}

int
runBug48604ops(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Table* pTab = 0;
  //const NdbDictionary::Index* pInd = 0;
  int loc = step->getStepNo() - 1;
  assert(loc > 0);
  g_err << "ops: loc:" << loc << endl;
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  char msg[MaxMsg];

  do
  {
    CHECK(recv_msg(ctx, loc, 0, msg));
    assert(msg[0] == 's');
    CHECK((pTab = pDic->getTable(tabName_Bug48604)) != 0);
    HugoOperations ops(*pTab);
    bool have_trans = false;
    int opseq = 0;

    while (result == NDBT_OK && !ctx->isTestStopped())
    {
      CHECK(recv_msg(ctx, loc, 0, msg));
      if (msg[0] == 'x')
        break;
      if (msg[0] == 'o')
      {
        char* p = &msg[1];
        int c;
        while (result == NDBT_OK && (c = *p++) != 0)
        {
          if (c == 'n')
          {
            assert(have_trans);
            CHECK(ops.execute_NoCommit(pNdb) == 0);
            g_info << loc << ": not committed" << endl;
            continue;
          }
          if (c == 'c')
          {
            assert(have_trans);
            CHECK(ops.execute_Commit(pNdb) == 0);
            ops.closeTransaction(pNdb);
            have_trans = false;
            g_info << loc << ": committed" << endl;
            continue;
          }
          if (c == 'a')
          {
            assert(have_trans);
            CHECK(ops.execute_Rollback(pNdb) == 0);
            ops.closeTransaction(pNdb);
            have_trans = false;
            g_info << loc << ": aborted" << endl;
            continue;
          }
          if (c == 'i' || c == 'u' || c == 'd')
          {
            if (!have_trans)
            {
              CHECK(ops.startTransaction(pNdb) == 0);
              have_trans = true;
              g_info << loc << ": trans started" << endl;
            }
            int i;
            for (i = 0; result == NDBT_OK && i < records; i++)
            {
              if (c == 'i')
                  CHECK(ops.pkInsertRecord(pNdb, i, 1, opseq) == 0);
              if (c == 'u')
                CHECK(ops.pkUpdateRecord(pNdb, i, 1, opseq) == 0);
              if (c == 'd')
                CHECK(ops.pkDeleteRecord(pNdb, i, 1) == 0);
            }
            char op_str[2];
            sprintf(op_str, "%c", c);
            g_info << loc << ": op:" << op_str << " records:" << records << endl;
            opseq++;
            continue;
          }
          if (c == 'z')
          {
            CHECK(ops.clearTable(pNdb) == 0);
            continue;
          }
          assert(false);
        }
        CHECK(send_msg(ctx, loc, 0, "o"));
        continue;
      }
      assert(false);
    }
  } while (0);

  g_err << "ops: loc:" << loc << " exit:" << result << endl;
  if (result != NDBT_OK)
    ctx->stopTest();
  return result;
}

int
runBug54651(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  for (Uint32 j = 0; j< 2; j++)
  {
    pDic->createTable(* ctx->getTab());
    
    const NdbDictionary::Table * pTab =pDic->getTable(ctx->getTab()->getName());
    NdbDictionary::Table copy = * pTab;
    BaseString name;
    name.assfmt("%s_1", pTab->getName());
    copy.setName(name.c_str());
    
    if (pDic->createTable(copy))
    {
      ndbout_c("Failed to create table...");
      ndbout << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    
    NdbDictionary::Table alter = * pTab;
    alter.setName(name.c_str());
    for (Uint32 i = 0; i<2; i++)
    {
      // now rename org table to same name...
      if (pDic->alterTable(* pTab, alter) == 0)
      {
        ndbout << "Alter with duplicate name succeeded!!" << endl;
        return NDBT_FAILED;
      }
      
      ndbout << "Alter with duplicate name failed...good" << endl
             << pDic->getNdbError() << endl;
    }
    
    pDic->dropTable(copy.getName());
    pDic->dropTable(ctx->getTab()->getName());
  }
  return NDBT_OK;
}

// Bug58277 + Bug57057

#define require(b) \
  if (!(b)) { \
    g_err << "ABORT: " << #b << " failed at line " << __LINE__ << endl; \
    abort(); \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    result = NDBT_FAILED; \
    break; \
  }

// allow list of expected error codes which do not cause NDBT_FAILED
#define CHK3(b, e, x) \
  if (!(b)) { \
    int n = sizeof(x)/sizeof(x[0]); \
    int i; \
    for (i = 0; i < n; i++) { \
      int s = (x[i] >= 0 ? +1 : -1); \
      if (e.code == s * x[i]) { \
        if (s == +1) \
          g_info << "OK: " << #b << " failed at line " << __LINE__ \
                << ": " << e << endl; \
        break; \
      } \
    } \
    if (i == n) { \
      g_err << "ERR: " << #b << " failed at line " << __LINE__ \
            << ": " << e << endl; \
      result = NDBT_FAILED; \
    } \
    break; \
  }

const char* tabName_Bug58277 = "TBug58277";
const char* indName_Bug58277 = "TBug58277X1";

static void
sync_main_step(NDBT_Context* ctx, NDBT_Step* step, const char* state)
{
  // total sub-steps
  Uint32 sub_steps = ctx->getProperty("SubSteps", (Uint32)0);
  require(sub_steps != 0);
  // count has been reset before
  require(ctx->getProperty("SubCount", (Uint32)0) == 0);
  // set the state
  g_info << "step main: set " << state << endl;
  require(ctx->getProperty(state, (Uint32)0) == 0);
  ctx->setProperty(state, (Uint32)1);
  // wait for sub-steps
  ctx->getPropertyWait("SubCount", sub_steps);
  if (ctx->isTestStopped())
    return;
  g_info << "step main: sub-steps got " << state << endl;
  // reset count and state
  ctx->setProperty("SubCount", (Uint32)0);
  ctx->setProperty(state, (Uint32)0);
}

static void
sync_sub_step(NDBT_Context* ctx, NDBT_Step* step, const char* state)
{
  // wait for main step to set state
  g_info << "step " << step->getStepNo() << ": wait for " << state << endl;
  ctx->getPropertyWait(state, (Uint32)1);
  if (ctx->isTestStopped())
    return;
  // add to sub-step counter
  ctx->incProperty("SubCount");
  g_info << "step " << step->getStepNo() << ": got " << state << endl;
  // continue to run until next sync
}

static int
runBug58277createtable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;

  do
  {
    CHK2(rows > 0, "cannot use --records=0"); // others require this
    g_info << "create table " << tabname << endl;
    NdbDictionary::Table tab(tabname);
    const char* name[] = { "a", "b" };
    for (int i = 0; i <= 1; i++)
    {
      NdbDictionary::Column c(name[i]);
      c.setType(NdbDictionary::Column::Unsigned);
      c.setPrimaryKey(i == 0);
      c.setNullable(false);
      tab.addColumn(c);
    }
    if (rand() % 3 != 0)
    {
      g_info << "set FragAllLarge" << endl;
      tab.setFragmentType(NdbDictionary::Object::FragAllLarge);
    }
    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
  }
  while (0);
  return result;
}

static int
runBug58277loadtable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;

  do
  {
    g_info << "load table" << endl;
    const NdbDictionary::Table* pTab = 0;
    CHK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());

    int cnt = 0;
    for (int i = 0; i < rows; i++)
    {
      NdbTransaction* pTx = 0;
      CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());

      NdbOperation* pOp = 0;
      CHK2((pOp = pTx->getNdbOperation(pTab)) != 0, pTx->getNdbError());
      CHK2(pOp->insertTuple() == 0, pOp->getNdbError());
      Uint32 aVal = i;
      Uint32 bVal = rand() % rows;
      CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
      CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());

      do
      {
        int x[] = {
         -630
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        cnt++;
      }
      while (0);
      CHK2(result == NDBT_OK, "load failed");
      pNdb->closeTransaction(pTx);
    }
    CHK2(result == NDBT_OK, "load failed");
    g_info << "load " << cnt << " rows" << endl;
  }
  while (0);
  return result;
}

static int
runBug58277createindex(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;

  do
  {
    g_info << "create index " << indname << endl;
    NdbDictionary::Index ind(indname);
    ind.setTable(tabname);
    ind.setType(NdbDictionary::Index::OrderedIndex);
    ind.setLogging(false);
    ind.addColumn("b");
    CHK2(pDic->createIndex(ind) == 0, pDic->getNdbError());

    const NdbDictionary::Index* pInd = 0;
    CHK2((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError());
  }
  while (0);
  return result;
}

// separate error handling test
int
runBug58277errtest(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  NdbRestarter restarter;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  (void)pDic->dropTable(tabname);

  const int errloops = loops < 5 ? loops : 5;
  int errloop = 0;
  while (!ctx->isTestStopped() && errloop < errloops)
  {
    g_info << "===== errloop " << errloop << " =====" << endl;

    if (errloop == 0)
    {
      CHK2(runBug58277createtable(ctx, step) == NDBT_OK, "create table failed");
      CHK2(runBug58277loadtable(ctx, step) == NDBT_OK, "load table failed");
      CHK2(runBug58277createindex(ctx, step) == NDBT_OK, "create index failed");
    }
    const NdbDictionary::Index* pInd = 0;
    CHK2((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError());

    int errins[] = {
      12008, 909,  // TuxNoFreeScanOp
      12009, 4259  // InvalidBounds
    };
    const int errcnt = (int)(sizeof(errins)/sizeof(errins[0]));
    for (int i = 0; i < errcnt; i += 2)
    {
      const int ei = errins[i + 0];
      const int ec = errins[i + 1];
      CHK2(restarter.insertErrorInAllNodes(ei) == 0, "value " << ei);

      NdbTransaction* pSTx = 0;
      CHK2((pSTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
      NdbIndexScanOperation* pSOp = 0;
      CHK2((pSOp = pSTx->getNdbIndexScanOperation(pInd)) != 0, pSTx->getNdbError());

      NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
      Uint32 flags = 0;
      CHK2(pSOp->readTuples(lm, flags) == 0, pSOp->getNdbError());

      Uint32 aVal = 0;
      CHK2(pSOp->getValue("a", (char*)&aVal) != 0, pSOp->getNdbError());
      CHK2(pSTx->execute(NoCommit) == 0, pSTx->getNdbError());
      // before fixes 12009 failed to fail at once here
      CHK2(pSOp->nextResult(true) == -1, "failed to fail on " << ei);
      CHK2(pSOp->getNdbError().code == ec, "expect " << ec << " got " << pSOp->getNdbError());
      pNdb->closeTransaction(pSTx);

      g_info << "error " << ei << " " << ec << " ok" << endl;
      CHK2(restarter.insertErrorInAllNodes(0) == 0, "value " << 0);
    }
    CHK2(result == NDBT_OK, "test error handling failed");

    errloop++;
    if (errloop == errloops)
    {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_info << "table " << tabname << " dropped" << endl;
    }
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

int
runBug58277drop(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  int dropms = 0;

  while (!ctx->isTestStopped())
  {
    sync_sub_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;
    dropms = ctx->getProperty("DropMs", (Uint32)0);
    NdbSleep_MilliSleep(dropms);

    g_info << "drop index " << indname << endl;
    CHK2(pDic->dropIndex(indname, tabname) == 0, pDic->getNdbError());
    pDic->invalidateIndex(indname, tabname);
    CHK2(pDic->getIndex(indname, tabname) == 0, "failed");
    g_info << "drop index done" << endl;

    sync_sub_step(ctx, step, "Stop");
    if (ctx->isTestStopped())
      break;
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

static int
runBug58277scanop(NDBT_Context* ctx, NDBT_Step* step, int cnt[1+3])
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  const int range_max = ctx->getProperty("RANGE_MAX", (Uint32)0);
  require(range_max > 0);
  const bool scan_delete = ctx->getProperty("SCAN_DELETE", (Uint32)0);

  do
  {
    const NdbDictionary::Index* pInd = 0;
    {
      int x[] = {
        4243  // Index not found
      };
      pDic->invalidateIndex(indname, tabname);
      CHK3((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError(), x);
    }

    NdbTransaction* pSTx = 0;
    CHK2((pSTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
    NdbIndexScanOperation* pSOp = 0;
    CHK2((pSOp = pSTx->getNdbIndexScanOperation(pInd)) != 0, pSTx->getNdbError());
    NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
    Uint32 flags = 0;
    int range_cnt = rand() % range_max;
    if (range_cnt > 1 || rand() % 5 == 0)
      flags |= NdbIndexScanOperation::SF_MultiRange;
    CHK2(pSOp->readTuples(lm, flags) == 0, pSOp->getNdbError());
    g_info << "range cnt " << range_cnt << endl;
    for (int i = 0; i < range_cnt; )
    {
      int tlo = -1;
      int thi = -1;
      if (rand() % 5 == 0)
      {
        if (rand() % 5 != 0)
          tlo = 0 + rand() % 2;
        if (rand() % 5 != 0)
          thi = 2 + rand() % 2;
      }
      else
        tlo = 4;
      // apparently no bounds is not allowed (see also bug#57396)
      if (tlo == -1 && thi == -1)
        continue;
      Uint32 blo = 0;
      Uint32 bhi = 0;
      if (tlo != -1)
      {
        blo = rand() % rows;
        CHK2(pSOp->setBound("b", tlo, &blo) == 0, pSOp->getNdbError());
      }
      if (thi != -1)
      {
        bhi = rand() % (rows + 1);
        if (bhi < blo)
          bhi = rand() % (rows + 1);
        CHK2(pSOp->setBound("b", thi, &bhi) == 0, pSOp->getNdbError());
      }
      CHK2(pSOp->end_of_bound() == 0, pSOp->getNdbError());
      i++;
    }
    CHK2(result == NDBT_OK, "set bound ranges failed");

    Uint32 aVal = 0;
    CHK2(pSOp->getValue("a", (char*)&aVal) != 0, pSOp->getNdbError());
    CHK2(pSTx->execute(NoCommit) == 0, pSTx->getNdbError());

    while (1)
    {
      int ret;
      {
        int x[] = {
          241,  // Invalid schema object version
          274,  // Time-out in NDB, probably caused by deadlock
          283,  // Table is being dropped
          284,  // Table not defined in transaction coordinator
          910,  // Index is being dropped
          1226  // Table is being dropped
        };
        CHK3((ret = pSOp->nextResult(true)) != -1, pSOp->getNdbError(), x);
      }
      require(ret == 0 || ret == 1);
      if (ret == 1)
        break;

      NdbTransaction* pTx = 0;
      CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());

      while (1)
      {
        int type = 1 + rand() % 3;
        if (type == 2) // insert->update
          type = 1;
        if (scan_delete)
          type = 3;
        do
        {
          if (type == 1)
          {
            NdbOperation* pOp = 0;
            CHK2((pOp = pSOp->updateCurrentTuple(pTx)) != 0, pSOp->getNdbError());
            Uint32 bVal = (Uint32)(rand() % rows);
            CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());
            break;
          }
          if (type == 3)
          {
            NdbOperation* pOp = 0;
            CHK2(pSOp->deleteCurrentTuple(pTx) == 0, pSOp->getNdbError());
            break;
          }
          require(false);
        }
        while (0);
        CHK2(result == NDBT_OK, "scan takeover error");
        cnt[type]++;
        {
          int x[] = {
            266,  // Time-out in NDB, probably caused by deadlock
            499,  // Scan take over error
            631,  // 631
            4350  // Transaction already aborted
          };
          CHK3(pTx->execute(NoCommit) == 0, pTx->getNdbError(), x);
        }

        CHK2((ret = pSOp->nextResult(false)) != -1, pSOp->getNdbError());
        require(ret == 0 || ret == 2);
        if (ret == 2)
          break;
      }
      CHK2(result == NDBT_OK, "batch failed");

      {
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
          4350  // Transaction already aborted
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
      }
      pNdb->closeTransaction(pTx);
    }
    CHK2(result == NDBT_OK, "batch failed");
    pNdb->closeTransaction(pSTx);
  }
  while (0);
  return result;
}

int
runBug58277scan(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  while (!ctx->isTestStopped())
  {
    sync_sub_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;
    g_info << "start scan loop" << endl;
    while (!ctx->isTestStopped())
    {
      g_info << "start scan" << endl;
      int cnt[1+3] = { 0, 0, 0, 0 };
      CHK2(runBug58277scanop(ctx, step, cnt) == NDBT_OK, "scan failed");
      g_info << "scan ops " << cnt[1] << "/-/" << cnt[3] << endl;

      if (ctx->getProperty("Stop", (Uint32)0) == 1)
      {
        sync_sub_step(ctx, step, "Stop");
        break;
      }
    }
    CHK2(result == NDBT_OK, "scan loop failed");
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

static int
runBug58277pkop(NDBT_Context* ctx, NDBT_Step* step, int cnt[1+3])
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;

  do
  {
    const NdbDictionary::Table* pTab = 0;
    CHK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());

    NdbTransaction* pTx = 0;
    CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
    NdbOperation* pOp = 0;
    CHK2((pOp = pTx->getNdbOperation(pTab)) != 0, pTx->getNdbError());
    int type = 1 + rand() % 3;
    Uint32 aVal = rand() % rows;
    Uint32 bVal = rand() % rows;

    do
    {
      if (type == 1)
      {
        CHK2(pOp->updateTuple() == 0, pOp->getNdbError());
        CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
        CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
         -626   // Tuple did not exist
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        break;
      }
      if (type == 2)
      {
        CHK2(pOp->insertTuple() == 0, pOp->getNdbError());
        CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
        CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
         -630   // Tuple already existed when attempting to insert
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        break;
      }
      if (type == 3)
      {
        CHK2(pOp->deleteTuple() == 0, pOp->getNdbError());
        CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
         -626   // Tuple did not exist
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        break;
      }
      require(false);
    }
    while (0);
    CHK2(result == NDBT_OK, "pk op failed");

    pNdb->closeTransaction(pTx);
    cnt[type]++;
  }
  while (0);
  return result;
}

int
runBug58277pk(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  while (!ctx->isTestStopped())
  {
    sync_sub_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;

    g_info << "start pk loop" << endl;
    int cnt[1+3] = { 0, 0, 0, 0 };
    while (!ctx->isTestStopped())
    {
      CHK2(runBug58277pkop(ctx, step, cnt) == NDBT_OK, "pk op failed");

      if (ctx->getProperty("Stop", (Uint32)0) == 1)
      {
        sync_sub_step(ctx, step, "Stop");
        break;
      }
    }
    CHK2(result == NDBT_OK, "pk loop failed");
    g_info << "pk ops " << cnt[1] << "/" << cnt[2] << "/" << cnt[3] << endl;
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

int
runBug58277rand(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbRestarter restarter;

  while (!ctx->isTestStopped())
  {
    int sleepms = rand() % 5000;
    g_info << "rand sleep " << sleepms << " ms" << endl;
    NdbSleep_MilliSleep(sleepms);
    if (rand() % 5 == 0)
    {
      g_info << "rand force LCP" << endl;
      int dump1[] = { DumpStateOrd::DihStartLcpImmediately };
      CHK2(restarter.dumpStateAllNodes(dump1, 1) == 0, "failed");
    }
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  g_info << "rand exit" << endl;
  return result;
}

int
runBug58277(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  const bool rss_check = ctx->getProperty("RSS_CHECK", (Uint32)0);
  NdbRestarter restarter;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  (void)pDic->dropTable(tabname);

  int loop = 0;
  while (!ctx->isTestStopped())
  {
    g_info << "===== loop " << loop << " =====" << endl;

    if (loop == 0)
    {
      CHK2(runBug58277createtable(ctx, step) == NDBT_OK, "create table failed");
      CHK2(runBug58277loadtable(ctx, step) == NDBT_OK, "load table failed");
    }

    if (rss_check)
    {
      g_info << "save all resource usage" << endl;
      int dump1[] = { DumpStateOrd::SchemaResourceSnapshot };
      CHK2(restarter.dumpStateAllNodes(dump1, 1) == 0, "failed");
    }

    CHK2(runBug58277createindex(ctx, step) == NDBT_OK, "create index failed");

    int dropmin = 1000;
    int dropmax = 9000;
    int dropms = dropmin + rand() % (dropmax - dropmin + 1);
    g_info << "drop in " << dropms << " ms" << endl;
    ctx->setProperty("DropMs", dropms);

    sync_main_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;

    // vary Stop time a bit in either direction
    int stopvar = rand() % 100;
    int stopsgn = (rand() % 2 == 0 ? +1 : -1);
    int stopms = dropms + stopsgn * stopvar;
    NdbSleep_MilliSleep(stopms);

    sync_main_step(ctx, step, "Stop");
    if (ctx->isTestStopped())
      break;

    // index must have been dropped
    pDic->invalidateIndex(indname, tabname);
    CHK2(pDic->getIndex(indname, tabname) == 0, "failed");

    if (rss_check)
    {
      g_info << "check all resource usage" << endl;
      int dump2[] = { DumpStateOrd::SchemaResourceCheckLeak };
      CHK2(restarter.dumpStateAllNodes(dump2, 1) == 0, "failed");

      g_info << "check cluster is up" << endl;
      CHK2(restarter.waitClusterStarted() == 0, "failed");
    }

    if (++loop == loops)
    {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_info << "table " << tabname << " dropped" << endl;
      break;
    }
  }

  g_info << "stop test at line " << __LINE__ << endl;
  ctx->stopTest();
  return result;
}

int
runBug57057(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  const bool rss_check = ctx->getProperty("RSS_CHECK", (Uint32)0);
  NdbRestarter restarter;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  (void)pDic->dropTable(tabname);

  int loop = 0;
  while (!ctx->isTestStopped())
  {
    g_info << "===== loop " << loop << " =====" << endl;

    if (loop == 0)
    {
      CHK2(runBug58277createtable(ctx, step) == NDBT_OK, "create table failed");
      CHK2(runBug58277createindex(ctx, step) == NDBT_OK, "create index failed");
    }

    CHK2(runBug58277loadtable(ctx, step) == NDBT_OK, "load table failed");

    if (rss_check)
    {
      g_info << "save all resource usage" << endl;
      int dump1[] = { DumpStateOrd::SchemaResourceSnapshot };
      CHK2(restarter.dumpStateAllNodes(dump1, 1) == 0, "failed");
    }

    int dropmin = 1000;
    int dropmax = 2000;
    int dropms = dropmin + rand() % (dropmax - dropmin + 1);
    int stopms = dropms;

    sync_main_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;

    g_info << "stop in " << stopms << " ms" << endl;
    NdbSleep_MilliSleep(stopms);

    sync_main_step(ctx, step, "Stop");
    if (ctx->isTestStopped())
      break;

    if (rss_check)
    {
      g_info << "check all resource usage" << endl;
      int dump2[] = { DumpStateOrd::SchemaResourceCheckLeak };
      CHK2(restarter.dumpStateAllNodes(dump2, 1) == 0, "failed");

      g_info << "check cluster is up" << endl;
      CHK2(restarter.waitClusterStarted() == 0, "failed");
    }

    if (++loop == loops)
    {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_info << "table " << tabname << " dropped" << endl;
      break;
    }
  }

  g_info << "stop test at line " << __LINE__ << endl;
  ctx->stopTest();
  return result;
}


static const char* control = "DropTabWorkerState";
enum WorkerStates
{
  WS_INIT,
  WS_IDLE,
  WS_ACTIVE
};

int
runDropTabWorker(NDBT_Context* ctx, NDBT_Step* step)
{
  while (!ctx->isTestStopped())
  {
    ctx->setProperty(control, WS_IDLE);
    ctx->getPropertyWait(control, WS_ACTIVE);
    if (ctx->isTestStopped())
      return NDBT_OK;

    Ndb* pNdb = GETNDB(step);
    const char* tabName = ctx->getTab()->getName();
    ndbout_c("Dropping table %s", tabName);
    int rc = pNdb->getDictionary()->dropTable(tabName);
    ndbout_c("Table drop return code : %d",
             rc);
  }
  return NDBT_OK;
}

struct DropTabNFScenario
{
  Uint32 errorCode;
  bool masterVictim;
};

static const DropTabNFScenario DropTabNFScenarios[] =
{
  { 6028, false }     /* Kill slave at top of PREP_DROP_TAB_REQ */
  ,{ 6027, false }      /* Kill slave at top of DROP_TAB_REQ */
//  ,{ 6028, true }      /* Kill master at top of PREP_DROP_TAB_REQ */
  ,{ 6027, true }      /* Kill master at top of DROP_TAB_REQ */

};

int
runDropTabNF(NDBT_Context* ctx, NDBT_Step* step)
{
  /* 
     1. Create table
     2. Insert error(s) on slave node
     3. Drop table
     4. Kill slave node
     5. Wait for drop to complete
     6. Wait for restart to complete

     Variants
     1.  Insert on slave/master
     2.  Error code types
  */
  
  NdbRestarter restarter;
  Uint32 numScenarios = sizeof(DropTabNFScenarios) / sizeof(DropTabNFScenario);
  int numLoops = ctx->getNumLoops();

  for (int r=0; r < numLoops; r++)
  {
    ndbout_c("**** loop %d ****", r);
    for (int n=0; n < numScenarios; n++)
    {
      ndbout_c("Creating table");
      if (runCreateTheTable(ctx, step) != NDBT_OK)
      {
        return NDBT_FAILED;
      }
      
      Uint32 errorCode = DropTabNFScenarios[n].errorCode;
      int victimNode = 0;
      const char* role;
      if (DropTabNFScenarios[n].masterVictim)
      {
        victimNode = restarter.getMasterNodeId();
        role = "M";
      }
      else
      {
        victimNode = restarter.getRandomNotMasterNodeId(rand());
        role = "S";
      }
      ndbout_c("Chosen victim node : %u (%s)", victimNode, role);
      
      restarter.insertErrorInNode(victimNode, errorCode);
      
      ndbout_c("Inserted error %u in node %u", errorCode, victimNode);
      
      ndbout_c("Requesting drop tab");
      ctx->getPropertyWait(control, WS_IDLE);
      ctx->setProperty(control, WS_ACTIVE);
      
      ndbout_c("Restarting node %u", victimNode);
      restarter.restartOneDbNode(victimNode);
      ndbout_c("Node restarting....");
      
      ndbout_c("Waiting for drop table to complete...");
      ctx->getPropertyWait(control, WS_IDLE);
      ndbout_c("Drop table completed");
      
      ndbout_c("Waiting for node to recover");
      restarter.waitNodesStarted(&victimNode, 1);
      ndbout_c("Node started");
    }
  }

  ndbout_c("**** stop ****");
  ctx->stopTest();

  return NDBT_OK;
}


NDBT_TESTSUITE(testDict);
TESTCASE("testDropDDObjects",
         "* 1. start cluster\n"
         "* 2. Create LFG\n"
         "* 3. create TS\n"
         "* 4. run DropDDObjects\n"
         "* 5. Verify DropDDObjectsRestart worked\n"){
INITIALIZER(runWaitStarted);
INITIALIZER(runDropDDObjects);
INITIALIZER(testDropDDObjectsSetup);
STEP(runDropDDObjects);
FINALIZER(DropDDObjectsVerify);
}

TESTCASE("Bug29501",
         "* 1. start cluster\n"
         "* 2. Restart 1 node -abort -nostart\n"
         "* 3. create LFG\n"
         "* 4. Restart data node\n"
         "* 5. Restart 1 node -nostart\n"
         "* 6. Drop LFG\n"){
INITIALIZER(runWaitStarted);
INITIALIZER(runDropDDObjects);
STEP(runBug29501);
FINALIZER(runDropDDObjects);
}
TESTCASE("CreateAndDrop", 
	 "Try to create and drop the table loop number of times\n"){
  INITIALIZER(runCreateAndDrop);
}
TESTCASE("CreateAndDropAtRandom",
	 "Try to create and drop table at random loop number of times\n"
         "Uses all available tables\n"
         "Uses error insert 4013 to make TUP verify table descriptor"){
  INITIALIZER(runCreateAndDropAtRandom);
}
TESTCASE("CreateAndDropWithData", 
	 "Try to create and drop the table when it's filled with data\n"
	 "do this loop number of times\n"){
  INITIALIZER(runCreateAndDropWithData);
}
TESTCASE("CreateAndDropDuring", 
	 "Try to create and drop the table when other thread is using it\n"
	 "do this loop number of times\n"){
  STEP(runCreateAndDropDuring);
  STEP(runUseTableUntilStopped);
}
TESTCASE("CreateInvalidTables", 
	 "Try to create the invalid tables we have defined\n"){ 
  INITIALIZER(runCreateInvalidTables);
}
TESTCASE("CreateTableWhenDbIsFull", 
	 "Try to create a new table when db already is full\n"){ 
  INITIALIZER(runCreateTheTable);
  INITIALIZER(runFillTable);
  INITIALIZER(runCreateTableWhenDbIsFull);
  INITIALIZER(runDropTableWhenDbIsFull);
  FINALIZER(runDropTheTable);
}
TESTCASE("FragmentTypeSingle", 
	 "Create the table with fragment type Single\n"){
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragSingle);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("FragmentTypeAllSmall", 
	 "Create the table with fragment type AllSmall\n"){ 
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragAllSmall);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("FragmentTypeAllMedium", 
	 "Create the table with fragment type AllMedium\n"){ 
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragAllMedium);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("FragmentTypeAllLarge", 
	 "Create the table with fragment type AllLarge\n"){ 
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragAllLarge);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("TemporaryTables", 
	 "Create the table as temporary and make sure it doesn't\n"
	 "contain any data when system is restarted\n"){ 
  INITIALIZER(runTestTemporaryTables);
}
TESTCASE("CreateMaxTables", 
	 "Create tables until db says that it can't create any more\n"){
  TC_PROPERTY("tables", 1000);
  INITIALIZER(runCreateMaxTables);
  INITIALIZER(runDropMaxTables);
}
TESTCASE("PkSizes", 
	 "Create tables with all different primary key sizes.\n"\
	 "Test all data operations insert, update, delete etc.\n"\
	 "Drop table."){
  INITIALIZER(runPkSizes);
}
TESTCASE("StoreFrm", 
	 "Test that a frm file can be properly stored as part of the\n"
	 "data in Dict."){
  INITIALIZER(runStoreFrm);
}
TESTCASE("GetPrimaryKey", 
	 "Test the function NdbDictionary::Column::getPrimaryKey\n"
	 "It should return true only if the column is part of \n"
	 "the primary key in the table"){
  INITIALIZER(runGetPrimaryKey);
}
TESTCASE("StoreFrmError", 
	 "Test that a frm file with too long length can't be stored."){
  INITIALIZER(runStoreFrmError);
}
TESTCASE("NF1", 
	 "Test that create table can handle NF (not master)"){
  INITIALIZER(runNF1);
}
TESTCASE("TableRename",
	 "Test basic table rename"){
  INITIALIZER(runTableRename);
}
TESTCASE("TableRenameNF",
	 "Test that table rename can handle node failure"){
  INITIALIZER(runTableRenameNF);
}
TESTCASE("TableRenameSR",
	 "Test that table rename can handle system restart"){
  INITIALIZER(runTableRenameSR);
}
TESTCASE("DictionaryPerf",
	 ""){
  INITIALIZER(runTestDictionaryPerf);
}
TESTCASE("CreateLogfileGroup", ""){
  INITIALIZER(runCreateLogfileGroup);
}
TESTCASE("CreateTablespace", ""){
  INITIALIZER(runCreateTablespace);
}
TESTCASE("CreateDiskTable", ""){
  INITIALIZER(runCreateDiskTable);
}
TESTCASE("FailAddFragment",
         "Fail add fragment or attribute in ACC or TUP or TUX\n"){
  INITIALIZER(runFailAddFragment);
}
TESTCASE("Restart_NF1",
         "DICT ops during node graceful shutdown (not master)"){
  TC_PROPERTY("Restart_NF_ops", 1);
  TC_PROPERTY("Restart_NF_type", 1);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("Restart_NF2",
         "DICT ops during node shutdown abort (not master)"){
  TC_PROPERTY("Restart_NF_ops", 1);
  TC_PROPERTY("Restart_NF_type", 2);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("Restart_NR1",
         "DICT ops during node startup (not master)"){
  TC_PROPERTY("Restart_NR_ops", 1);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("Restart_NR2",
         "DICT ops during node startup with crash inserts (not master)"){
  TC_PROPERTY("Restart_NR_ops", 1);
  TC_PROPERTY("Restart_NR_error", 1);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("TableAddAttrs",
	 "Add attributes to an existing table using alterTable()"){
  INITIALIZER(runTableAddAttrs);
}
TESTCASE("TableAddAttrsDuring",
	 "Try to add attributes to the table when other thread is using it\n"
	 "do this loop number of times\n"){
  INITIALIZER(runCreateTheTable);
  STEP(runTableAddAttrsDuring);
  STEP(runUseTableUntilStopped2);
  STEP(runUseTableUntilStopped3);
  FINALIZER(runDropTheTable);
}
TESTCASE("TableAddAttrsDuringError",
	 "Try to add attributes to the table when other thread is using it\n"
	 "do this loop number of times\n"){
  TC_PROPERTY("AbortAlter", 1);
  INITIALIZER(runCreateTheTable);
  STEP(runTableAddAttrsDuring);
  STEP(runUseTableUntilStopped2);
  STEP(runUseTableUntilStopped3);
  FINALIZER(runDropTheTable);
}
TESTCASE("Bug21755",
         ""){
  INITIALIZER(runBug21755);
}
TESTCASE("DictRestart",
         ""){
  INITIALIZER(runDictRestart);
}
TESTCASE("Bug24631",
         ""){
  INITIALIZER(runBug24631);
}
TESTCASE("Bug36702", "")
{
  INITIALIZER(runDropDDObjects);
  INITIALIZER(runBug36072);
  FINALIZER(restartClusterInitial);
}
TESTCASE("Bug29186",
         ""){
  INITIALIZER(runBug29186);
}
TESTCASE("Bug48604",
         "Online ordered index build.\n"
         "Complements testOIBasic -case f"){
  STEP(runBug48604);
  STEP(runBug48604ops);
#if 0 // for future MT test
  STEP(runBug48604ops);
  STEP(runBug48604ops);
  STEP(runBug48604ops);
#endif
}
TESTCASE("Bug54651", ""){
  INITIALIZER(runBug54651);
}
TESTCASE("Bug58277",
         "Dropping busy ordered index can crash data node.\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("RSS_CHECK", (Uint32)true);
  TC_PROPERTY("RANGE_MAX", (Uint32)5);
  INITIALIZER(runBug58277errtest);
  STEP(runBug58277);
  // sub-steps 2-8 synced with main step
  TC_PROPERTY("SubSteps", 7);
  STEP(runBug58277drop);
  /*
   * A single scan update can show the bug but this is not likely.
   * Add more scan updates.  Also add PK ops for other asserts.
   */
  STEP(runBug58277scan);
  STEP(runBug58277scan);
  STEP(runBug58277scan);
  STEP(runBug58277scan);
  STEP(runBug58277pk);
  STEP(runBug58277pk);
  // kernel side scans (eg. LCP) for resource usage check
  STEP(runBug58277rand);
}
TESTCASE("Bug57057",
         "MRR + delete leaks stored procs (fixed under Bug58277).\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("RSS_CHECK", (Uint32)true);
  TC_PROPERTY("RANGE_MAX", (Uint32)100);
  TC_PROPERTY("SCAN_DELETE", (Uint32)1);
  STEP(runBug57057);
  TC_PROPERTY("SubSteps", 1);
  STEP(runBug58277scan);
}
TESTCASE("DropTabNF",
         "Drop table and node failure causes hang")
{
  STEP(runDropTabWorker);
  STEP(runDropTabNF);
}

NDBT_TESTSUITE_END(testDict);

int main(int argc, const char** argv){
  ndb_init();
  // Tables should not be auto created
  testDict.setCreateTable(false);
  myRandom48Init(NdbTick_CurrentMillisecond());
  return testDict.execute(argc, argv);
}
