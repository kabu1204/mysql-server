/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "zgroups.h"
#include "sql_class.h"
#include "binlog.h"
#include "transaction.h"

#ifdef HAVE_UGID


/**
  Acquire group ownership for a single group.  This is used to start a
  master-super-group when @@SESSION.UGID_NEXT_LIST = NULL and
  @@SESSION.UGID_NEXT = SID:GNO.
*/
static enum_ugid_statement_status
ugid_acquire_group_ownership(THD *thd, Checkable_rwlock *lock,
                             Group_log_state *gls,
                             rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("ugid_acquire_group_ownership");
  //printf("ugid_acquire_group_ownership(sidno=%d gno=%lld)\n", sidno, gno);
  enum_ugid_statement_status ret= UGID_STATEMENT_EXECUTE;
  lock->assert_some_rdlock();
  gls->lock_sidno(sidno);
  while (true) {
    if (gls->is_ended(sidno, gno))
    {
      DBUG_ASSERT(gls->get_owner(sidno, gno).is_none());
      ret= UGID_STATEMENT_SKIP;
      break;
    }
    Rpl_owner_id owner= gls->get_owner(sidno, gno);
    if (owner.is_none())
    {
      gls->acquire_ownership(sidno, gno, thd);
      break;
    }
    else if (owner.equals(thd))
    {
      break;
    }
    else
    {
      //printf("other owner. waiting.\n");
      lock->unlock();
      Group g= { sidno, gno };
      gls->wait_for_sidno(thd, &mysql_bin_log.sid_map, g, owner);
      lock->rdlock();
      if (thd->killed || abort_loop)
        DBUG_RETURN(UGID_STATEMENT_CANCEL);
      gls->lock_sidno(sidno);
    }
  } while (false);
  gls->unlock_sidno(sidno);
  DBUG_RETURN(ret);
}


/**
  Acquire ownership of all groups in a Group_set.  This is used to
  begin a master-super-group when @@SESSION.UGID_NEXT_LIST != NULL.
*/
static int
ugid_acquire_group_ownerships(THD *thd, Checkable_rwlock *lock,
                              Group_log_state *gls, const Group_set *gs)
{
  DBUG_ENTER("ugid_acquire_group_ownerships");
  //printf("ugid_acquire_group_ownerships\n");
  lock->assert_some_rdlock();
  // first check if we need to wait for any group
  while (true)
  {
    Group_set::Group_iterator git(gs);
    Group g= git.get();
    Rpl_owner_id owner;
    owner.set_to_none();
    rpl_sidno last_sidno= 0;
    DBUG_ASSERT(g.sidno != 0);
    do {
      // lock all SIDNOs in order
      if (g.sidno != last_sidno)
        gls->lock_sidno(g.sidno);
      if (!gls->is_ended(g.sidno, g.gno))
      {
        owner= gls->get_owner(g.sidno, g.gno);
        // break the do-loop and wait for the sid to be updated
        if (!owner.is_none() && !owner.equals(thd) &&
            !gls->is_partial(g.sidno, g.gno))
          break;
      }
      last_sidno= g.sidno;
      git.next();
      g= git.get();
    } while (g.sidno != 0);

    // we don't need to wait for any groups, and all SIDNOs in the
    // set are locked
    if (g.sidno == 0)
      break;

    // unlock all previous sidnos to avoid blocking them
    // while waiting.  keep lock on g.sidno
    for (rpl_sidno sidno= 1; sidno < g.sidno; sidno++)
      if (gs->contains_sidno(sidno))
        gls->unlock_sidno(sidno);
    lock->unlock();

    // wait
    gls->wait_for_sidno(thd, &mysql_bin_log.sid_map, g, owner);

    // at this point, we don't hold any locks. re-acquire the global
    // read lock that was held when this function was invoked
    lock->rdlock();
    if (thd->killed || abort_loop)
      DBUG_RETURN(1);
  }

  // now we know that we don't have to wait for any other
  // thread. so we acquire ownership of all groups that we need
  int ret= 0;
  Group_set::Group_iterator git(gs);
  Group g= git.get();
  do {
    if (!gls->is_ended(g.sidno, g.gno))
    {
      Rpl_owner_id owner= gls->get_owner(g.sidno, g.gno);
      if (owner.is_none())
      {
        if (gls->acquire_ownership(g.sidno, g.gno, thd) != GS_SUCCESS)
        {
          ret= 1;
          break;
        }
      }
      else
        // in the previous loop, we waited for all groups owned
        // by other threads to become partial or ended
        DBUG_ASSERT(owner.equals(thd) ||
                    gls->is_partial(g.sidno, g.gno) ||
                    gls->is_ended(g.sidno, g.gno));
    }
    git.next();
    g= git.get();
  } while (g.sidno != 0);

  // unlock all sidnos
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      gls->unlock_sidno(sidno);

  DBUG_RETURN(ret);
}


/**
  Check that the @@SESSION.UGID_* variables are consistent.
*/
static int
ugid_before_statement_check_session_variables(
  const THD *thd, Checkable_rwlock *lock,
  const Group_cache *gsc, const Group_cache *gtc,
  const Group_set *ugid_next_list, const Ugid_specification *ugid_next)
{
  DBUG_ENTER("ugid_before_statement_check_session_variables");

  // The group statement cache must be empty in any case when a new
  // statement starts.
  DBUG_ASSERT(gsc->is_empty());

  if (ugid_next_list != NULL)
  {
    // If UGID_NEXT==SID:GNO, then SID:GNO must be listed in UGID_NEXT_LIST
    if (ugid_next->type == Ugid_specification::UGID &&
        !ugid_next_list->contains_group(ugid_next->group.sidno,
                                        ugid_next->group.gno))
    {
      char buf[Ugid_specification::MAX_TEXT_LENGTH + 1];
      lock->rdlock();
      ugid_next->to_string(buf);
      lock->unlock();
      my_error(ER_UGID_NEXT_IS_NOT_IN_UGID_NEXT_LIST, MYF(0), buf);
      DBUG_RETURN(1);
    }

    // UGID_NEXT cannot be "AUTOMATIC" when UGID_NEXT_LIST != NULL.
    if (ugid_next->type == Ugid_specification::AUTOMATIC)
    {
      my_error(ER_UGID_NEXT_CANT_BE_AUTOMATIC_IF_UGID_NEXT_LIST_IS_NON_NULL,
               MYF(0));
      DBUG_RETURN(1);
    }
  }

  // If UGID_NEXT=="SID:GNO", then SID:GNO must not be ended in this
  // master-super-group.
  if (ugid_next->type == Ugid_specification::UGID &&
      gtc->group_is_ended(ugid_next->group.sidno, ugid_next->group.gno))
  {
    char buf[Ugid_specification::MAX_TEXT_LENGTH + 1];
    lock->rdlock();
    ugid_next->to_string(buf);
    lock->unlock();
    my_error(ER_UGID_NEXT_IS_ENDED_IN_GROUP_CACHE, MYF(0), buf);
    DBUG_RETURN(1);
  }

  // If UGID_END==1, then UGID_NEXT must not be "AUTOMATIC" or
  // "ANOYMOUS".
  if ((ugid_next->type == Ugid_specification::AUTOMATIC ||
       ugid_next->type == Ugid_specification::ANONYMOUS) &&
      thd->variables.ugid_end)
  {
    my_error(ER_UGID_END_IS_ON_BUT_UGID_NEXT_IS_AUTO_OR_ANON, MYF(0));
    DBUG_RETURN(1);
  }

  // If UGID_NEXT_LIST == NULL and UGID_NEXT == "SID:GNO", then
  // UGID_END cannot be 1 unless UGID_COMMIT is 1.  Rationale:
  // otherwise there would be no way to end the master-super-group.
  if (ugid_next_list == NULL &&
      ugid_next->type == Ugid_specification::UGID &&
      thd->variables.ugid_end &&
      !thd->variables.ugid_commit)
  {
    my_error(ER_UGID_END_REQUIRES_UGID_COMMIT_WHEN_UGID_NEXT_LIST_IS_NULL, MYF(0));
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


/**
  Begin master-super-group, i.e., acquire ownership of all groups to
  be re-executed.
*/
static enum_ugid_statement_status
ugid_before_statement_begin_master_super_group(
  THD *thd, Checkable_rwlock *lock, Group_log_state *gls,
  const Group_set *ugid_next_list, const Ugid_specification *ugid_next)
{
  DBUG_ENTER("ugid_before_statement_begin_master_super_group");

  lock->assert_some_rdlock();

  if (!thd->variables.ugid_has_ongoing_super_group)
  {
    if (gls->ensure_sidno() != GS_SUCCESS)
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(0), ER(ER_OUT_OF_RESOURCES));
      DBUG_RETURN(UGID_STATEMENT_CANCEL);
    }

    if (ugid_next_list != NULL)
    {
      // acquire group ownership for Group_set.
      if (!ugid_next_list->is_empty())
        if (ugid_acquire_group_ownerships(thd, lock, gls, ugid_next_list))
        {
          my_error(ER_OUT_OF_RESOURCES, MYF(0), ER(ER_OUT_OF_RESOURCES));
          DBUG_RETURN(UGID_STATEMENT_CANCEL);
        }
      thd->variables.ugid_has_ongoing_super_group= 1;
    }
    else
    {
      if (ugid_next->type == Ugid_specification::UGID)
      {
        // acquire group ownership for single group.
        DBUG_PRINT("info", ("acquire group ownership of single group %d %lld", ugid_next->group.sidno, ugid_next->group.gno));
        enum_ugid_statement_status ret=
          ugid_acquire_group_ownership(thd, lock, gls,
                                       ugid_next->group.sidno,
                                       ugid_next->group.gno);
        if (ret != UGID_STATEMENT_CANCEL)
          thd->variables.ugid_has_ongoing_super_group= 1;
        DBUG_RETURN(ret);
      }
      else if (ugid_next->type == Ugid_specification::ANONYMOUS)
      {
        // No need to acquire group ownership.  But we are entering a
        // master-super-group, so set the flag.
        thd->variables.ugid_has_ongoing_super_group= 1;
      }
      else
      {
        DBUG_ASSERT(ugid_next->type == Ugid_specification::AUTOMATIC);
        // We are not entering a master-super-group; do nothing.
      }
    }
  }
  DBUG_RETURN(UGID_STATEMENT_EXECUTE);
}


/**
  Begin a master-group, i.e., check if the statement should be skipped
  or not.
*/
static int
ugid_before_statement_begin_group(const THD *thd, Checkable_rwlock *lock,
                                  const Group_log_state *gls,
                                  const Ugid_specification *ugid_next)
{
  DBUG_ENTER("ugid_before_statement_begin_group");

  lock->assert_some_rdlock();

  if (ugid_next->type == Ugid_specification::UGID)
    if (!gls->get_owner(ugid_next->group.sidno,
                        ugid_next->group.gno).equals(thd))
      DBUG_RETURN(1);
  DBUG_RETURN(0);
}


enum_ugid_statement_status
ugid_before_statement(THD *thd, Checkable_rwlock *lock, Group_log_state *gls,
                      Group_cache *gsc, Group_cache *gtc)
{
  DBUG_ENTER("ugid_before_statement");

  const Group_set *ugid_next_list= thd->get_ugid_next_list();
  const Ugid_specification *ugid_next= &thd->variables.ugid_next;

  // Sanity check session variables.
  if (ugid_before_statement_check_session_variables(thd, lock, gsc, gtc,
                                                    ugid_next_list, ugid_next))
    DBUG_RETURN(UGID_STATEMENT_CANCEL);

  lock->rdlock();

  // begin master-super-group, i.e., acquire group ownerships and set
  // the thd->variables.has_ongoing_super_group to true.
  enum_ugid_statement_status ret=
    ugid_before_statement_begin_master_super_group(thd, lock, gls,
                                                   ugid_next_list, ugid_next);
  if (ret == UGID_STATEMENT_CANCEL)
    DBUG_RETURN(ret);

  // Begin the group, i.e., check if this statement should be skipped
  // or not.
  if (ret == UGID_STATEMENT_EXECUTE)
    if (ugid_before_statement_begin_group(thd, lock, gls, ugid_next))
      ret= UGID_STATEMENT_SKIP;

  // Generate a warning if the group should be skipped.  We do not
  // generate warning if log_warnings is false, partially because that
  // would make unit tests fail (ER() is not safe to use in unit
  // tests).
  if (ret == UGID_STATEMENT_SKIP && global_system_variables.log_warnings)
  {
    char buf[Group::MAX_TEXT_LENGTH + 1];
    ugid_next->to_string(buf);
    /*
      @todo: tests fail when this is enabled. figure out why /sven
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_SKIPPING_LOGGED_GROUP,
                        ER(ER_SKIPPING_LOGGED_GROUP), buf);
    */
  }
//#endif

  lock->unlock();

  DBUG_RETURN(ret);
}


int ugid_flush_group_cache(THD *thd, Checkable_rwlock *lock,
                           Group_log_state *gls, Group_cache *gc,
                           Group_cache *trx_cache,
                           rpl_binlog_pos offset_after_last_statement)
{
  DBUG_ENTER("ugid_flush_group_cache");
  lock->rdlock();
  gc->generate_automatic_gno(thd, gls);
  if (gc->write_to_log(trx_cache, offset_after_last_statement) != GS_SUCCESS)
    DBUG_RETURN(1);
  if (gc->update_group_log_state(thd, gls) != GS_SUCCESS)
    DBUG_RETURN(1);
  lock->unlock();
  gc->clear();
  DBUG_RETURN(0);
}


int ugid_before_flush_trx_cache(THD *thd, Checkable_rwlock *lock,
                                Group_log_state *gls, Group_cache *trx_cache)
{
  DBUG_ENTER("ugid_before_flush_trx_cache");
  enum_group_status ret= GS_SUCCESS;

  if (thd->variables.ugid_end)
  {
    Ugid_specification *ugid_next= &thd->variables.ugid_next;
    /*
      If UGID_NEXT != NULL and UGID_END = 1, but the group is not
      ended in the binary log and not ended in the transaction group
      cache, then we have to end it by a dummy subgroup.
    */
    if (ugid_next->type == Ugid_specification::UGID)
    {
      if (!trx_cache->group_is_ended(ugid_next->group.sidno, 
                                     ugid_next->group.gno))
      {
        lock->rdlock();
        if (!gls->is_ended(ugid_next->group.sidno, ugid_next->group.gno))
          ret= trx_cache->add_dummy_subgroup(ugid_next->group.sidno,
                                             ugid_next->group.gno, true);
        lock->unlock();
      }
    }
  }
  if (ret != GS_SUCCESS)
    DBUG_RETURN(1); /// @todo generate error in log /sven

  /*printf("ugid_before_flush_trx_cache commit=%d\n",
         thd->variables.ugid_commit);
  */
  if (thd->variables.ugid_commit)
  {
    /*
      If UGID_COMMIT = 1 and UGID_NEXT_LIST != NULL, then we have to
      add dummy groups for every group in UGID_NEXT_LIST that does not
      already exist in the cache or in the group log.
    */
    Group_set *ugid_next_list= thd->get_ugid_next_list();
    if (ugid_next_list != NULL)
    {
      lock->rdlock();
      //printf("hello\n");
      //ugid_next_list->print();
      ret= trx_cache->add_dummy_subgroups_if_missing(gls, ugid_next_list);
      lock->unlock();
    }
    else
    {
      /*
        If UGID_COMMIT = 1 and UGID_NEXT_LIST = NULL and UGID_NEXT !=
        NULL, then we have to add a dummy group if the group in
        UGID_NEXT does not already exist in the cache or in the group
        log.
      */
      Ugid_specification *ugid_next= &thd->variables.ugid_next;
      if (ugid_next->type == Ugid_specification::UGID)
      {
        lock->rdlock();
        ret= trx_cache->add_dummy_subgroup_if_missing(gls,
                                                      ugid_next->group.sidno,
                                                      ugid_next->group.gno);
        lock->unlock();
      }
    }
  }
  if (ret != GS_SUCCESS)
    DBUG_RETURN(1); /// @todo generate error in log /sven

  DBUG_RETURN(0);
}


#endif
