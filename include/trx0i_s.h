/******************************************************
INFORMATION SCHEMA innodb_trx, innodb_locks and
innodb_lock_waits tables cache structures and public
functions.

(c) 2007 Innobase Oy

Created July 17, 2007 Vasil Dimov
*******************************************************/

#ifndef trx0i_s_h
#define trx0i_s_h

#include "univ.i"
#include "ut0ut.h"

/* the maximum length of a string that can be stored in
i_s_locks_row_t::lock_data */
#define TRX_I_S_LOCK_DATA_MAX_LEN	8192

typedef struct i_s_locks_row_struct	i_s_locks_row_t;
typedef struct i_s_hash_chain_struct	i_s_hash_chain_t;

/* Objects of this type are added to the hash table
trx_i_s_cache_t::locks_hash */
struct i_s_hash_chain_struct {
	i_s_locks_row_t*	value;
	i_s_hash_chain_t*	next;
};

/* This structure represents INFORMATION_SCHEMA.innodb_locks row */
struct i_s_locks_row_struct {
	ullint		lock_trx_id;
	const char*	lock_mode;
	const char*	lock_type;
	const char*	lock_table;
	const char*	lock_index;
	ulint		lock_space;
	ulint		lock_page;
	ulint		lock_rec;
	const char*	lock_data;

	/* The following are auxiliary and not included in the table */
	ullint		lock_table_id;
	i_s_hash_chain_t hash_chain; /* this object is added to the hash
				    table
				    trx_i_s_cache_t::locks_hash */
};

/* This structure represents INFORMATION_SCHEMA.innodb_trx row */
typedef struct i_s_trx_row_struct {
	ullint			trx_id;
	const char*		trx_state;
	ib_time_t		trx_started;
	const i_s_locks_row_t*	wait_lock_row;
	ib_time_t		trx_wait_started;
	ulint			trx_mysql_thread_id;
} i_s_trx_row_t;

/* This structure represents INFORMATION_SCHEMA.innodb_lock_waits row */
typedef struct i_s_lock_waits_row_struct {
	const i_s_locks_row_t*	wait_lock_row;
	const i_s_locks_row_t*	waited_lock_row;
} i_s_lock_waits_row_t;

/* This type is opaque and is defined in trx/trx0i_s.c */
typedef struct trx_i_s_cache_struct	trx_i_s_cache_t;

/* Auxiliary enum used by functions that need to select one of the
INFORMATION_SCHEMA tables */
enum i_s_table {
	I_S_INNODB_TRX,
	I_S_INNODB_LOCKS,
	I_S_INNODB_LOCK_WAITS
};

/* This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
extern trx_i_s_cache_t*	trx_i_s_cache;

/***********************************************************************
Initialize INFORMATION SCHEMA trx related cache. */

void
trx_i_s_cache_init(
/*===============*/
	trx_i_s_cache_t*	cache);	/* out: cache to init */

/***********************************************************************
Issue a shared/read lock on the tables cache. */

void
trx_i_s_cache_start_read(
/*=====================*/
	trx_i_s_cache_t*	cache);	/* in: cache */

/***********************************************************************
Release a shared/read lock on the tables cache. */

void
trx_i_s_cache_end_read(
/*===================*/
	trx_i_s_cache_t*	cache);	/* in: cache */

/***********************************************************************
Issue an exclusive/write lock on the tables cache. */

void
trx_i_s_cache_start_write(
/*======================*/
	trx_i_s_cache_t*	cache);	/* in: cache */

/***********************************************************************
Release an exclusive/write lock on the tables cache. */

void
trx_i_s_cache_end_write(
/*====================*/
	trx_i_s_cache_t*	cache);	/* in: cache */


/***********************************************************************
Retrieves the number of used rows in the cache for a given
INFORMATION SCHEMA table. */

ullint
trx_i_s_cache_get_rows_used(
/*========================*/
					/* out: number of rows */
	trx_i_s_cache_t*	cache,	/* in: cache */
	enum i_s_table		table);	/* in: which table */

/***********************************************************************
Retrieves the nth row in the cache for a given INFORMATION SCHEMA
table. */

void*
trx_i_s_cache_get_nth_row(
/*======================*/
					/* out: row */
	trx_i_s_cache_t*	cache,	/* in: cache */
	enum i_s_table		table,	/* in: which table */
	ulint			n);	/* in: row number */

/***********************************************************************
Update the transactions cache if it has not been read for some time. */

int
trx_i_s_possibly_fetch_data_into_cache(
/*===================================*/
					/* out: 0 - fetched, 1 - not */
	trx_i_s_cache_t*	cache);	/* in/out: cache */

/* The maximum length that may be required by lock_id_sz in
trx_i_s_create_lock_id(). "%llu:%lu:%lu:%lu" -> 84 chars */

#define TRX_I_S_LOCK_ID_MAX_LEN	84

/***********************************************************************
Crafts a lock id string from a i_s_locks_row_t object. Returns its
second argument. This function aborts if there is not enough space in
lock_id. Be sure to provide at least TRX_I_S_LOCK_ID_MAX_LEN if you want
to be 100% sure that it will not abort. */

char*
trx_i_s_create_lock_id(
/*===================*/
					/* out: resulting lock id */
	const i_s_locks_row_t*	row,	/* in: innodb_locks row */
	char*			lock_id,/* out: resulting lock_id */
	ulint			lock_id_size);/* in: size of the lock id
					buffer */

#endif /* trx0i_s_h */
