/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"

#include "item.h"
#include "sql_get_diagnostics.h"

namespace {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class GetDiagnosticsTest : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    Server_initializer::SetUpTestCase();
  }

  static void TearDownTestCase()
  {
    Server_initializer::TearDownTestCase();
  }

  virtual void SetUp()
  {
    initializer.SetUp();
  }

  virtual void TearDown()
  {
    initializer.TearDown();
  }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};


class FailHelper
{
public:
  void fail(const char *message)
  {
    FAIL() << message;
  }
};


LEX_STRING var_name1= {C_STRING_WITH_LEN("var1")};
LEX_STRING var_name2= {C_STRING_WITH_LEN("var2")};


class MockDiagInfoItem : public Diagnostics_information_item
{
public:
  MockDiagInfoItem(Item *target, int value)
    : Diagnostics_information_item(target), m_value(value)
  {}

  Item *get_value(THD *thd, const Diagnostics_area *da)
  {
    return new (thd->mem_root) Item_int(m_value);
  }

private:
  int m_value;
};


class MockDiagInfo : public Diagnostics_information,
                     private FailHelper
{
public:
  MockDiagInfo(List<MockDiagInfoItem> *items)
    : m_items(items)
  {}

protected:
  bool aggregate(THD *thd, const Diagnostics_area *da)
  {
    bool rv= false;
    MockDiagInfoItem *diag_info_item;
    List_iterator<MockDiagInfoItem> it(*m_items);

    while ((diag_info_item= it++))
    {
      if ((rv= evaluate(thd, diag_info_item, da)))
        break;
    }

    return rv;
  }

  ~MockDiagInfo()
  {
    fail("MockDiagInfo destructor invoked.");
  }

private:
  List<MockDiagInfoItem> *m_items;
};


// GET [CURRENT] DIAGNOSTICS @var1 = 1, @var2 = 2;
TEST_F(GetDiagnosticsTest, Cmd)
{
  Item *var;
  Sql_cmd *cmd;
  MockDiagInfo *info;
  MockDiagInfoItem *diag_info_item;
  List<MockDiagInfoItem> items;
  MEM_ROOT *mem_root= thd()->mem_root;

  // set var1 item
  var= new (mem_root) Item_func_get_user_var(var_name1);
  diag_info_item= new (mem_root) MockDiagInfoItem(var, 1);
  EXPECT_FALSE(items.push_back(diag_info_item));

  // set var2 item
  var= new (mem_root) Item_func_get_user_var(var_name2);
  diag_info_item= new (mem_root) MockDiagInfoItem(var, 2);
  EXPECT_FALSE(items.push_back(diag_info_item));

  // Information list and command
  info= new (mem_root) MockDiagInfo(&items);
  info->set_which_da(Diagnostics_information::CURRENT_AREA);
  cmd= new (mem_root) Sql_cmd_get_diagnostics(info);

  EXPECT_FALSE(cmd->execute(thd()));
  EXPECT_TRUE(thd()->get_stmt_da()->is_ok());

  // check var1 value
  var= new (mem_root) Item_func_get_user_var(var_name1);
  EXPECT_FALSE(var->fix_fields(thd(), &var));
  EXPECT_EQ(1, var->val_int());

  // check var2 value
  var= new (mem_root) Item_func_get_user_var(var_name2);
  EXPECT_FALSE(var->fix_fields(thd(), &var));
  EXPECT_EQ(2, var->val_int());
}


// Verifies death with a DBUG_ASSERT if target item is not settable.
//
// Although Google Test recommends DeathTest suffix for classes used
// in death tests, this is not done to avoid the server being started
// more than once.
#if GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)
TEST_F(GetDiagnosticsTest, DieWhenUnsettableItem)
{
  Item *var;
  Sql_cmd *cmd;
  MockDiagInfo *info;
  MockDiagInfoItem *diag_info_item;
  List<MockDiagInfoItem> items;
  MEM_ROOT *mem_root= thd()->mem_root;

  ::testing::FLAGS_gtest_death_test_style= "threadsafe";

  // Unsettable item
  var= new (mem_root) Item_int(1);
  diag_info_item= new (mem_root) MockDiagInfoItem(var, 1);
  EXPECT_FALSE(items.push_back(diag_info_item));

  // Information list and command
  info= new (mem_root) MockDiagInfo(&items);
  info->set_which_da(Diagnostics_information::CURRENT_AREA);
  cmd= new (mem_root) Sql_cmd_get_diagnostics(info);

  EXPECT_DEATH(cmd->execute(thd()), ".*Assertion.*srp.*");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)


class MockDiagInfoError : public Diagnostics_information
{
public:
  MockDiagInfoError(bool fatal_error)
    : m_fatal_error(fatal_error)
  {}

protected:
  bool aggregate(THD *thd, const Diagnostics_area *)
  {
    myf flag= m_fatal_error ? MYF(ME_FATALERROR) : MYF(0);
    my_message_sql(ER_UNKNOWN_ERROR, "Unknown error", flag);
    return thd->is_error();
  }

private:
  bool m_fatal_error;
};


// GET DIAGNOSTICS itself causes an error.
TEST_F(GetDiagnosticsTest, Error)
{
  Sql_cmd *cmd;
  MockDiagInfoError *info;
  MEM_ROOT *mem_root= thd()->mem_root;

  // Pre-existing warning
  push_warning_printf(thd(), MYSQL_ERROR::WARN_LEVEL_WARN,
                      WARN_DATA_TRUNCATED, "Data truncated");

  // Simulate GET DIAGNOSTICS as a new command separated
  // from the one that generated the warning
  thd()->reset_for_next_command();

  // Error bound "information" and command
  info= new (mem_root) MockDiagInfoError(false);
  info->set_which_da(Diagnostics_information::CURRENT_AREA);
  cmd= new (mem_root) Sql_cmd_get_diagnostics(info);

  initializer.set_expected_error(ER_UNKNOWN_ERROR);

  // Should succeed, not a fatal error
  EXPECT_FALSE(cmd->execute(thd()));
  EXPECT_TRUE(thd()->get_stmt_da()->is_ok());

  // New condition for the error
  EXPECT_EQ(1U, thd()->get_stmt_da()->statement_warn_count());

  // Counted as a error
  EXPECT_EQ(1U, thd()->get_stmt_da()->get_warning_info()->error_count());

  // Error is appended
  EXPECT_EQ(2U, thd()->get_stmt_da()->get_warning_info()->warn_count());
}


// GET DIAGNOSTICS itself causes a fatal error.
TEST_F(GetDiagnosticsTest, FatalError)
{
  Sql_cmd *cmd;
  MockDiagInfoError *info;
  MEM_ROOT *mem_root= thd()->mem_root;

  // Pre-existing warning
  push_warning_printf(thd(), MYSQL_ERROR::WARN_LEVEL_WARN,
                      WARN_DATA_TRUNCATED, "Data truncated");

  // Simulate GET DIAGNOSTICS as a new command separated
  // from the one that generated the warning
  thd()->reset_for_next_command();

  // Error bound "information" and command
  info= new (mem_root) MockDiagInfoError(true);
  info->set_which_da(Diagnostics_information::CURRENT_AREA);
  cmd= new (mem_root) Sql_cmd_get_diagnostics(info);

  initializer.set_expected_error(ER_UNKNOWN_ERROR);

  // Should not succeed due to a fatal error
  EXPECT_TRUE(cmd->execute(thd()));
  EXPECT_TRUE(thd()->get_stmt_da()->is_error());

  // No new condition for the error
  EXPECT_EQ(0U, thd()->get_stmt_da()->get_warning_info()->error_count());

  // Fatal error is set, not appended
  EXPECT_EQ(1U, thd()->get_stmt_da()->get_warning_info()->warn_count());
}


}
