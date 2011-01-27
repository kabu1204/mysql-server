/* Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

/**
   This is a unit test for the 'meta data locking' classes.
   It is written to illustrate how we can use Google Test for unit testing
   of MySQL code.
   For documentation on Google Test, see http://code.google.com/p/googletest/
   and the contained wiki pages GoogleTestPrimer and GoogleTestAdvancedGuide.
   The code below should hopefully be (mostly) self-explanatory.
 */

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#include "mdl.h"
#include <mysqld_error.h>

#include "thr_malloc.h"
#include "thread_utils.h"
#include "test_mdl_context_owner.h"

pthread_key(MEM_ROOT**,THR_MALLOC);
pthread_key(THD*, THR_THD);
mysql_mutex_t LOCK_open;
uint    opt_debug_sync_timeout= 0;

/*
  A mock error handler.
*/
static uint expected_error= 0;
extern "C" void test_error_handler_hook(uint err, const char *str, myf MyFlags)
{
  EXPECT_EQ(expected_error, err) << str;
}

/*
  A mock out-of-memory handler.
  We do not expect this to be called during testing.
*/
extern "C" void sql_alloc_error_handler(void)
{
  ADD_FAILURE();
}

/*
  Mock away this global function.
  We don't need DEBUG_SYNC functionality in a unit test.
 */
void debug_sync(THD *thd, const char *sync_point_name, size_t name_len)
{
  DBUG_PRINT("debug_sync_point", ("hit: '%s'", sync_point_name));
  FAIL() << "Not yet implemented.";
}

/*
  Putting everything in an unnamed namespace prevents any (unintentional)
  name clashes with the code under test.
*/
namespace {

using thread::Notification;
using thread::Thread;

const char db_name[]= "some_database";
const char table_name1[]= "some_table1";
const char table_name2[]= "some_table2";
const char table_name3[]= "some_table3";
const char table_name4[]= "some_table4";
const ulong zero_timeout= 0;
const ulong long_timeout= (ulong) 3600L*24L*365L;


class MDLTest : public ::testing::Test, public Test_MDL_context_owner
{
protected:
  MDLTest()
  : m_null_ticket(NULL),
    m_null_request(NULL)
  {
  }

  static void SetUpTestCase()
  {
    error_handler_hook= test_error_handler_hook;
  }

  void SetUp()
  {
    expected_error= 0;
    mdl_init();
    m_mdl_context.init(this);
    EXPECT_FALSE(m_mdl_context.has_locks());
    m_global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                          MDL_TRANSACTION);
  }

  void TearDown()
  {
    m_mdl_context.destroy();
    mdl_destroy();
  }

  virtual bool notify_shared_lock(MDL_context_owner *in_use,
                                  bool needs_thr_lock_abort)
  {
    return in_use->notify_shared_lock(NULL, needs_thr_lock_abort);
  }

  // A utility member for testing single lock requests.
  void test_one_simple_shared_lock(enum_mdl_type lock_type);

  const MDL_ticket  *m_null_ticket;
  const MDL_request *m_null_request;
  MDL_context        m_mdl_context;
  MDL_request        m_request;
  MDL_request        m_global_request;
  MDL_request_list   m_request_list;
private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(MDLTest);
};


/*
  Will grab a lock on table_name of given type in the run() function.
  The two notifications are for synchronizing with the main thread.
  Does *not* take ownership of the notifications.
*/
class MDL_thread : public Thread, public Test_MDL_context_owner
{
public:
  MDL_thread(const char   *table_name,
             enum_mdl_type mdl_type,
             Notification *lock_grabbed,
             Notification *release_locks)
  : m_table_name(table_name),
    m_mdl_type(mdl_type),
    m_lock_grabbed(lock_grabbed),
    m_release_locks(release_locks),
    m_ignore_notify(false)
  {
    m_mdl_context.init(this);
  }

  ~MDL_thread()
  {
    m_mdl_context.destroy();
  }

  virtual void run();
  void ignore_notify() { m_ignore_notify= true; }

  virtual bool notify_shared_lock(MDL_context_owner *in_use,
                                  bool needs_thr_lock_abort)
  {
    if (in_use)
      return in_use->notify_shared_lock(NULL, needs_thr_lock_abort);

    if (m_ignore_notify)
      return false;
    m_release_locks->notify();
    return true;
  }

private:
  const char    *m_table_name;
  enum_mdl_type  m_mdl_type;
  Notification  *m_lock_grabbed;
  Notification  *m_release_locks;
  bool           m_ignore_notify;
  MDL_context    m_mdl_context;
};


void MDL_thread::run()
{
  MDL_request request;
  MDL_request global_request;
  MDL_request_list request_list;
  global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                      MDL_TRANSACTION);
  request.init(MDL_key::TABLE, db_name, m_table_name, m_mdl_type,
               MDL_TRANSACTION);

  request_list.push_front(&request);
  if (m_mdl_type >= MDL_SHARED_NO_WRITE)
    request_list.push_front(&global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&request_list, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, m_table_name, m_mdl_type));

  // Tell the main thread that we have grabbed our locks.
  m_lock_grabbed->notify();
  // Hold on to locks until we are told to release them
  m_release_locks->wait_for_notification();

  m_mdl_context.release_transactional_locks();
}

// Google Test recommends DeathTest suffix for classes use in death tests.
typedef MDLTest MDLDeathTest;


/*
  Verifies that we die with a DBUG_ASSERT if we destry a non-empty MDL_context.
 */
#if GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)
TEST_F(MDLDeathTest, DieWhenMTicketsNonempty)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_DEATH(m_mdl_context.destroy(),
               ".*Assertion.*MDL_TRANSACTION.*is_empty.*");
  m_mdl_context.release_transactional_locks();
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)



/*
  The most basic test: just construct and destruct our test fixture.
 */
TEST_F(MDLTest, ConstructAndDestruct)
{
}


void MDLTest::test_one_simple_shared_lock(enum_mdl_type lock_type)
{
  m_request.init(MDL_key::TABLE, db_name, table_name1, lock_type,
                 MDL_TRANSACTION);

  EXPECT_EQ(lock_type, m_request.type);
  EXPECT_EQ(m_null_ticket, m_request.ticket);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_NE(m_null_ticket, m_request.ticket);
  EXPECT_TRUE(m_mdl_context.has_locks());
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, lock_type));

  MDL_request request_2;
  request_2.init(&m_request.key, lock_type, MDL_TRANSACTION);
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_2));
  EXPECT_EQ(m_request.ticket, request_2.ticket);

  m_mdl_context.release_transactional_locks();
  EXPECT_FALSE(m_mdl_context.has_locks());
}


/*
  Acquires one lock of type MDL_SHARED.
 */
TEST_F(MDLTest, OneShared)
{
  test_one_simple_shared_lock(MDL_SHARED);
}


/*
  Acquires one lock of type MDL_SHARED_HIGH_PRIO.
 */
TEST_F(MDLTest, OneSharedHighPrio)
{
  test_one_simple_shared_lock(MDL_SHARED_HIGH_PRIO);
}


/*
  Acquires one lock of type MDL_SHARED_READ.
 */
TEST_F(MDLTest, OneSharedRead)
{
  test_one_simple_shared_lock(MDL_SHARED_READ);
}


/*
  Acquires one lock of type MDL_SHARED_WRITE.
 */
TEST_F(MDLTest, OneSharedWrite)
{
  test_one_simple_shared_lock(MDL_SHARED_WRITE);
}


/*
  Acquires one lock of type MDL_EXCLUSIVE.  
 */
TEST_F(MDLTest, OneExclusive)
{
  const enum_mdl_type lock_type= MDL_EXCLUSIVE;
  m_request.init(MDL_key::TABLE, db_name, table_name1, lock_type,
                 MDL_TRANSACTION);
  EXPECT_EQ(m_null_ticket, m_request.ticket);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));

  EXPECT_NE(m_null_ticket, m_request.ticket);
  EXPECT_NE(m_null_ticket, m_global_request.ticket);
  EXPECT_TRUE(m_mdl_context.has_locks());
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, lock_type));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE));
  EXPECT_TRUE(m_request.ticket->is_upgradable_or_exclusive());

  m_mdl_context.release_transactional_locks();
  EXPECT_FALSE(m_mdl_context.has_locks());
}


/*
  Acquires two locks, on different tables, of type MDL_SHARED.
  Verifies that they are independent.
 */
TEST_F(MDLTest, TwoShared)
{
  MDL_request request_2;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED, MDL_EXPLICIT);
  request_2.init(MDL_key::TABLE, db_name, table_name2, MDL_SHARED, MDL_EXPLICIT);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_2));
  EXPECT_TRUE(m_mdl_context.has_locks());
  ASSERT_NE(m_null_ticket, m_request.ticket);
  ASSERT_NE(m_null_ticket, request_2.ticket);

  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name3, MDL_SHARED));

  m_mdl_context.release_lock(m_request.ticket);
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.has_locks());

  m_mdl_context.release_lock(request_2.ticket);
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.has_locks());
}


/*
  Verifies that two different contexts can acquire a shared lock
  on the same table.
 */
TEST_F(MDLTest, SharedLocksBetweenContexts)
{
  MDL_context  mdl_context2;
  mdl_context2.init(this);
  MDL_request request_2;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  request_2.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_FALSE(mdl_context2.try_acquire_lock(&request_2));

  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(mdl_context2.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));

  m_mdl_context.release_transactional_locks();
  mdl_context2.release_transactional_locks();
}


/*
  Verifies that we can upgrade a shared lock to exclusive.
 */
TEST_F(MDLTest, UpgradeSharedUpgradable)
{
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED_NO_WRITE,
                 MDL_TRANSACTION);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));
  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock_to_exclusive(m_request.ticket, long_timeout));
  EXPECT_EQ(MDL_EXCLUSIVE, m_request.ticket->get_type());

  // Another upgrade should be a no-op.
  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock_to_exclusive(m_request.ticket, long_timeout));
  EXPECT_EQ(MDL_EXCLUSIVE, m_request.ticket->get_type());

  m_mdl_context.release_transactional_locks();
}


/*
  Verifies that only upgradable locks can be upgraded to exclusive.
 */
TEST_F(MDLDeathTest, DieUpgradeShared)
{
  MDL_request request_2;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  request_2.init(MDL_key::TABLE, db_name, table_name2, MDL_SHARED_NO_READ_WRITE,
                 MDL_TRANSACTION);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&request_2);
  m_request_list.push_front(&m_global_request);
  
  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));

#if GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(m_mdl_context.
                            upgrade_shared_lock_to_exclusive(m_request.ticket,
                                                             long_timeout),
                            ".*MDL_SHARED_NO_.*");
#endif
  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock_to_exclusive(request_2.ticket, long_timeout));
  m_mdl_context.release_transactional_locks();
}


/*
  Verfies that locks are released when we roll back to a savepoint.
 */
TEST_F(MDLTest, SavePoint)
{
  MDL_request request_2;
  MDL_request request_3;
  MDL_request request_4;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  request_2.init(MDL_key::TABLE, db_name, table_name2, MDL_SHARED,
                 MDL_TRANSACTION);
  request_3.init(MDL_key::TABLE, db_name, table_name3, MDL_SHARED,
                 MDL_TRANSACTION);
  request_4.init(MDL_key::TABLE, db_name, table_name4, MDL_SHARED,
                 MDL_TRANSACTION);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_2));
  MDL_savepoint savepoint= m_mdl_context.mdl_savepoint();
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_3));
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_4));

  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name3, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name4, MDL_SHARED));

  m_mdl_context.rollback_to_savepoint(savepoint);
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name3, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name4, MDL_SHARED));

  m_mdl_context.release_transactional_locks();
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
}


/*
  Verifies that we can grab shared locks concurrently, in different threads.
 */
TEST_F(MDLTest, ConcurrentShared)
{
  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED, &lock_grabbed, &release_locks);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);

  EXPECT_FALSE(m_mdl_context.acquire_lock(&m_request, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));

  release_locks.notify();
  mdl_thread.join();

  m_mdl_context.release_transactional_locks();
}


/*
  Verifies that we cannot grab an exclusive lock on something which
  is locked with a shared lock in a different thread.
 */
TEST_F(MDLTest, ConcurrentSharedExclusive)
{
  expected_error= ER_LOCK_WAIT_TIMEOUT;

  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED, &lock_grabbed, &release_locks);
  mdl_thread.ignore_notify();
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_EXCLUSIVE,
                 MDL_TRANSACTION);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  // We should *not* be able to grab the lock here.
  EXPECT_TRUE(m_mdl_context.acquire_locks(&m_request_list, zero_timeout));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE,
                             db_name, table_name1, MDL_EXCLUSIVE));

  release_locks.notify();
  mdl_thread.join();

  // Now we should be able to grab the lock.
  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, zero_timeout));
  EXPECT_NE(m_null_ticket, m_request.ticket);

  m_mdl_context.release_transactional_locks();
}


/*
  Verifies that we cannot we cannot grab a shared lock on something which
  is locked exlusively in a different thread.
 */
TEST_F(MDLTest, ConcurrentExclusiveShared)
{
  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_EXCLUSIVE,
                        &lock_grabbed, &release_locks);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);

  // We should *not* be able to grab the lock here.
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_EQ(m_null_ticket, m_request.ticket);

  release_locks.notify();

  // The other thread should eventually release its locks.
  EXPECT_FALSE(m_mdl_context.acquire_lock(&m_request, long_timeout));
  EXPECT_NE(m_null_ticket, m_request.ticket);

  mdl_thread.join();
  m_mdl_context.release_transactional_locks();
}


/*
  Verifies the following scenario:
  Thread 1: grabs a shared upgradable lock.
  Thread 2: grabs a shared lock.
  Thread 1: asks for an upgrade to exclusive (needs to wait for thread 2)
  Thread 2: gets notified, and releases lock.
  Thread 1: gets the exclusive lock.
 */
TEST_F(MDLTest, ConcurrentUpgrade)
{
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED_NO_WRITE,
                 MDL_TRANSACTION);
  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE,
                            db_name, table_name1, MDL_SHARED_NO_WRITE));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE,
                             db_name, table_name1, MDL_EXCLUSIVE));

  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED, &lock_grabbed, &release_locks);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock_to_exclusive(m_request.ticket, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE,
                            db_name, table_name1, MDL_EXCLUSIVE));

  mdl_thread.join();
  m_mdl_context.release_transactional_locks();
}

}  // namespace
