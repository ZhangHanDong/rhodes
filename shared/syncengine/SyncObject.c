/*
 *  SyncObject.h
 *  RhoSyncClient
 *  Represents a synchronized object
 *
 *  Copyright (C) 2008 Lars Burgess. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SyncObject.h"
#include "Constants.h"
#include "Utils.h"
#include "SyncOperation.h"

/* Database access statements */
static sqlite3_stmt *insert_statement = NULL;
static sqlite3_stmt *init_statement = NULL;
static sqlite3_stmt *delete_statement = NULL;
static sqlite3_stmt *hydrate_statement = NULL;
static sqlite3_stmt *dehydrate_statement = NULL;
static sqlite3_stmt *select_statement = NULL;
static sqlite3_stmt *cleanup_statement = NULL;
static sqlite3_stmt *fetch_statement = NULL;

/* Update type database statements */
static sqlite3_stmt *delete_type_statement = NULL;
static sqlite3_stmt *update_type_statement = NULL;
static sqlite3_stmt *create_type_statement = NULL;

static char* new_entry = "!!placeholder!!";
static int new_source_id = -1;

/* Finalize (delete) all of the SQLite compiled queries. */
void finalize_statements() {
    if (insert_statement) sqlite3_finalize(insert_statement);
    if (init_statement) sqlite3_finalize(init_statement);
    if (delete_statement) sqlite3_finalize(delete_statement);
    if (hydrate_statement) sqlite3_finalize(hydrate_statement);
    if (dehydrate_statement) sqlite3_finalize(dehydrate_statement);
	if (select_statement) sqlite3_finalize(select_statement);
	if (cleanup_statement) sqlite3_finalize(cleanup_statement);
	if (fetch_statement) sqlite3_finalize(fetch_statement);
	if (delete_type_statement) sqlite3_finalize(delete_type_statement);
	if (update_type_statement) sqlite3_finalize(update_type_statement);
	if (create_type_statement) sqlite3_finalize(create_type_statement);
}

pSyncObject SyncObjectCreate() {
	pSyncObject sync;
	sync = malloc(sizeof(SyncObject));
	sync->_attrib = NULL;
	sync->_source_id = new_source_id;
	sync->_object = NULL;
	sync->_value = NULL;
	sync->_created_at = NULL;
	sync->_updated_at = NULL;
	sync->_update_type = NULL;
	sync->_primary_key = 0;
	sync->_hydrated = 0;
	sync->_dirty = 0;
	return sync;
}

pSyncObject SyncObjectCreateWithValues(sqlite3* db, int primary_key, 
									  char *attrib, int source_id, char *object, 
									  char *value, char *update_type, int hydrated, int dirty) {
	pSyncObject sync;
	sync = malloc(sizeof(SyncObject));
	sync->_database = db;
	sync->_primary_key = primary_key;
	sync->_attrib = str_assign(attrib);
	sync->_source_id = source_id;
	sync->_object = str_assign(object);
	sync->_value = str_assign(value);
	sync->_created_at = NULL;
	sync->_updated_at = NULL;
	sync->_update_type = str_assign(update_type);
	sync->_hydrated = hydrated;
	sync->_dirty = dirty;
	return sync;
}

pSyncObject SyncObjectCopy(pSyncObject new_object) {
	pSyncObject sync;
	sync = malloc(sizeof(SyncObject));
	sync->_database = new_object->_database;
	sync->_primary_key = new_object->_primary_key;
	sync->_attrib = str_assign(new_object->_attrib);
	sync->_source_id = new_object->_source_id;
	sync->_object = str_assign(new_object->_object);
	sync->_value = str_assign(new_object->_value);
	sync->_created_at = str_assign(new_object->_created_at);
	sync->_updated_at = str_assign(new_object->_updated_at);
	sync->_update_type = str_assign(new_object->_update_type);
	sync->_hydrated = new_object->_hydrated;
	sync->_dirty = new_object->_dirty;
	return sync;
}

/* lookup by id and return 1 if a row exists in the database, otherwise return 0 */
int exists_in_database(pSyncObject ref) {
  int success;
	if (ref->_primary_key == 0) {
		return 0;
	}
    prepare_db_statement("SELECT value FROM object_values where id=?",
						 ref->_database,
						 &select_statement);
	sqlite3_bind_int(select_statement, 1, ref->_primary_key);
	success = sqlite3_step(select_statement);
	/* we have a row with the same value, return 1 */
    if (success == SQLITE_ROW) {
		char *tmp_check = str_assign((char *)sqlite3_column_text(select_statement, 0));
		if(strcmp(tmp_check, ref->_value) == 0) {
			sqlite3_reset(select_statement);
			return 1;
		}
    }
	sqlite3_reset(select_statement);
	sqlite3_finalize(select_statement);
	return 0;
}

/* insert object into database, returns SYNC_OBJECT_DUPLICATE, SYNC_OBJECT_ERROR, or SYNC_OBJECT_SUCCESS */
int insert_into_database(pSyncObject ref) {
  int success;
	if (exists_in_database(ref)) {
		return SYNC_OBJECT_DUPLICATE;
	} else {
		prepare_db_statement("INSERT INTO object_values (id, attrib, source_id, object, value, created_at, updated_at, update_type) VALUES(?,?,?,?,?,?,?,?)",
							 ref->_database,
							 &insert_statement);
		sqlite3_bind_int(insert_statement, 1, ref->_primary_key);
		sqlite3_bind_text(insert_statement, 2, new_entry, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(insert_statement, 3, new_source_id);
		sqlite3_bind_text(insert_statement, 4, new_entry, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert_statement, 5, new_entry, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert_statement, 6, new_entry, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert_statement, 7, new_entry, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert_statement, 8, new_entry, -1, SQLITE_TRANSIENT);
		success = sqlite3_step(insert_statement);
		sqlite3_reset(insert_statement);
		if (success == SQLITE_ERROR) {
			printf("Error: failed to insert into the database with message '%s'.", sqlite3_errmsg(ref->_database));
			return 0;
		} 
		ref->_hydrated = 0;
		sqlite3_finalize(insert_statement);
		return SYNC_OBJECT_SUCCESS;
	}
}

/* Remove all objects from database */
int delete_from_database_by_source(sqlite3 *db, int source) {
    int success = 0;
	prepare_db_statement("DELETE FROM object_values where source_id=?",
						 db,
						 &delete_statement);
	sqlite3_bind_int(delete_statement, 1, source);
	success = sqlite3_step(delete_statement);
	sqlite3_reset(delete_statement);
	if (success != SQLITE_DONE) {
		printf("Error: failed to delete from database with message '%s'.", sqlite3_errmsg(db));
		return 1;
	}
	sqlite3_finalize(delete_statement);
	delete_statement = NULL;
	return 0;
}
			 
/* Brings the rest of the object data into memory. If already in memory, no action is taken (harmless no-op). */
pSyncObject hydrate(pSyncObject ref) {
    int success;
    if (ref->_hydrated) return ref;
    prepare_db_statement("SELECT attrib, source_id, object, value, created_at, updated_at, update_type FROM object_values WHERE id=?",
						 ref->_database,
						 &hydrate_statement);
    sqlite3_bind_int(hydrate_statement, 1, ref->_primary_key);
    success =sqlite3_step(hydrate_statement);
	
    if (success == SQLITE_ROW) {
		ref->_attrib = str_assign((char *)sqlite3_column_text(hydrate_statement, 0));
		ref->_source_id = (int)sqlite3_column_int(hydrate_statement, 1);
		ref->_object = str_assign((char *)sqlite3_column_text(hydrate_statement, 2));
		ref->_value = str_assign((char *)sqlite3_column_text(hydrate_statement, 3));
        ref->_created_at = str_assign((char *)sqlite3_column_text(hydrate_statement, 4));
		ref->_updated_at = str_assign((char *)sqlite3_column_text(hydrate_statement, 5));
		ref->_update_type = str_assign((char *)sqlite3_column_text(hydrate_statement, 6));
    } else {
        /* The query did not return */
		char *unknown = "Unknown";
        ref->_attrib = str_assign(unknown);
		ref->_source_id = -1;
		ref->_object = str_assign(unknown);
		ref->_value = str_assign(unknown);
        ref->_created_at = str_assign(unknown);
		ref->_updated_at = str_assign(unknown);
		ref->_update_type = str_assign(unknown);
    }
    sqlite3_reset(hydrate_statement);
	sqlite3_finalize(hydrate_statement);
    /* Update object state with respect to hydration. */
    ref->_hydrated = 1;
	return ref;
}

/* Insert the object to the database */
void dehydrate(pSyncObject ref) {
  int success;
    if (ref->_dirty) {		
		prepare_db_statement("UPDATE object_values SET attrib=?, source_id=?, object=?, value=?, created_at=?, updated_at=?, update_type=? WHERE id=?",
							 ref->_database,
							 &dehydrate_statement);
        sqlite3_bind_text(dehydrate_statement, 1, ref->_attrib, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(dehydrate_statement, 2, ref->_source_id);
        sqlite3_bind_text(dehydrate_statement, 3, ref->_object, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(dehydrate_statement, 4, ref->_value, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(dehydrate_statement, 5, ref->_created_at, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(dehydrate_statement, 6, ref->_updated_at, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(dehydrate_statement, 7, ref->_update_type, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(dehydrate_statement, 8, ref->_primary_key);
        success = sqlite3_step(dehydrate_statement);
        if (success == SQLITE_ERROR) {
            printf("Error: failed to dehydrate with message '%s'.", sqlite3_errmsg(ref->_database));
        }
		sqlite3_reset(dehydrate_statement);
		sqlite3_finalize(dehydrate_statement);
        ref->_dirty = 0;
    }
    ref->_hydrated = 0;
} 

void free_ob_list(pSyncObject *list, int available) {
	int k;
	/* Free up our ob_list */
	for(k = 0; k < available; k++) {
		SyncObjectRelease(list[k]);
	}
}

void SyncObjectRelease(pSyncObject ref) {
	if (ref != NULL) {
		if (ref->_attrib != NULL) {
			free(ref->_attrib);
		} 
		if(ref->_object != NULL) {
			free(ref->_object);
		} 
		if(ref->_value != NULL) {
			free(ref->_value);
		} 
		if(ref->_created_at != NULL) {
			free(ref->_created_at);
		} 
		if(ref->_updated_at != NULL) {
			free(ref->_updated_at);
		}
		if(ref->_update_type != NULL) {
			free(ref->_update_type);
		}
		free(ref);
	}
}