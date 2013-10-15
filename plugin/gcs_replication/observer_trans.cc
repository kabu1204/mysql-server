/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_plugin.h"
#include "observer_trans.h"
#include <log_event.h>
#include <log.h>


/*
  Internal auxiliary functions signatures.
*/
static bool reinit_cache(IO_CACHE *cache,
                         enum cache_type type,
                         my_off_t position);

static bool copy_cache(IO_CACHE *dest, IO_CACHE *src);


/*
  Transaction lifecycle events observers.
*/
int gcs_trans_before_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_commit");
  int error= 0;

  bool is_real_trans= param->flags & TRANS_IS_REAL_TRANS;
  if (!is_real_trans)
    DBUG_RETURN(0);

  if (!is_gcs_rpl_running())
    DBUG_RETURN(0);

  // GCS cache.
  Transaction_context_log_event *tcle= NULL;
  rpl_gno snapshot_timestamp;
  IO_CACHE cache;

  // Binlog cache.
  bool is_dml= true;
  IO_CACHE *cache_log= NULL;
  my_off_t cache_log_position= 0;
  const my_off_t trx_cache_log_position= my_b_tell(param->trx_cache_log);
  const my_off_t stmt_cache_log_position= my_b_tell(param->stmt_cache_log);

  if (trx_cache_log_position > 0 && stmt_cache_log_position == 0)
  {
    cache_log= param->trx_cache_log;
    cache_log_position= trx_cache_log_position;
  }
  else if (trx_cache_log_position == 0 && stmt_cache_log_position > 0)
  {
    cache_log= param->stmt_cache_log;
    cache_log_position= stmt_cache_log_position;
    is_dml= false;
  }
  else
  {
    sql_print_error("We can only use one cache type at a time");
    error= 1;
    goto err;
  }

  DBUG_ASSERT(cache_log->type == WRITE_CACHE);
  DBUG_PRINT("cache_log", ("thread_id: %lu, trx_cache_log_position: %llu,"
                           " stmt_cache_log_position: %llu",
                           param->thread_id, trx_cache_log_position,
                           stmt_cache_log_position));

  // Get transaction snapshot timestamp.
  snapshot_timestamp= get_last_gno_without_gaps(gcs_cluster_sidno);
  DBUG_PRINT("snapshot_timestamp", ("snapshot_timestamp: %llu",
                                    snapshot_timestamp));

  // Open GCS cache.
  if (open_cached_file(&cache, mysql_tmpdir, "gcs_trans_before_commit_cache",
                       param->cache_log_max_size, MYF(MY_WME)))
  {
    sql_print_error("Failed to create gcs commit cache");
    error= 1;
    goto err;
  }

  // Reinit binlog cache to read.
  if (reinit_cache(cache_log, READ_CACHE, 0))
  {
    sql_print_error("Failed to reopen binlog cache log for read");
    error= 1;
    goto err;
  }

  // Create transaction context.
  tcle= new Transaction_context_log_event(param->server_uuid,
                                          param->thread_id,
                                          snapshot_timestamp);

  // TODO: For now DDL won't have write-set, it will be added by
  // WL#6823 and WL#6824.
  if (is_dml)
  {
    // TODO: WL#6834: add write set
  }

  // Write transaction context to GCS cache.
  tcle->write(&cache);

  // Copy binlog cache content to GCS cache.
  if (copy_cache(&cache, cache_log))
  {
    sql_print_error("Failed while writing binlog cache to GCS cache");
    error= 1;
    goto err;
  }

  // Reinit binlog cache to write (revert what we did).
  if (reinit_cache(cache_log, WRITE_CACHE, cache_log_position))
  {
    sql_print_error("Failed to reopen binlog cache log for write");
    error= 1;
    goto err;
  }

  // Reinit GCS cache to read.
  if (reinit_cache(&cache, READ_CACHE, 0))
  {
    sql_print_error("Failed to reopen GCS cache log for read");
    error= 1;
    goto err;
  }

  // TODO: WL#6855: broadcast GCS cache content

  // TODO: WL#6826: wait for certification decision

err:
  delete tcle;
  close_cached_file(&cache);

  DBUG_RETURN(error);
}

int gcs_trans_before_rollback(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_rollback");
  DBUG_RETURN(0);
}

int gcs_trans_after_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_after_commit");
  DBUG_RETURN(0);
}

int gcs_trans_after_rollback(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_after_rollback");
  DBUG_RETURN(0);
}

Trans_observer trans_observer = {
  sizeof(Trans_observer),

  gcs_trans_before_commit,
  gcs_trans_before_rollback,
  gcs_trans_after_commit,
  gcs_trans_after_rollback,
};


/*
  Internal auxiliary functions.
*/

/*
  Reinit IO_cache type.

  @param[in] cache     cache
  @param[in] type      type to which cache will change
  @param[in] position  position to which cache will seek
*/
static bool reinit_cache(IO_CACHE *cache,
                         enum cache_type type,
                         my_off_t position)
{
  DBUG_ENTER("reinit_cache");

  if (READ_CACHE == type && flush_io_cache(cache))
    DBUG_RETURN(true);

  if (reinit_io_cache(cache, type, position, 0, 0))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

/*
  Copy one cache content to another cache.

  @param[in] dest  cache to where data will be written
  @param[in] src   cache from which data will be read
*/
static bool copy_cache(IO_CACHE *dest, IO_CACHE *src)
{
  DBUG_ENTER("copy_cache");
  size_t length;

  DBUG_ASSERT(src->type == READ_CACHE && dest->type == WRITE_CACHE);

  while ((length= my_b_fill(src)) > 0)
  {
    if (src->error)
      DBUG_RETURN(true);

    if (my_b_write(dest, src->read_pos, length))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}
