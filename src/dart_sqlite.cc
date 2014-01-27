// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <sqlite3.h>
#include <string.h>
#include <stdio.h>

#include </Users/freewind/Downloads/dart/dart-sdk/include/dart_api.h>

#define DART_ARG(name, i)                                     \
  Dart_Handle name = Dart_GetNativeArgument(arguments, i);
#define DART_ARGS_0()                                         \
  Dart_EnterScope();
#define DART_ARGS_1(arg0)                                     \
  DART_ARGS_0()                                               \
  DART_ARG(arg0, 0)
#define DART_ARGS_2(arg0, arg1)                               \
  DART_ARGS_1(arg0);                                          \
  DART_ARG(arg1, 1)
#define DART_ARGS_3(arg0, arg1, arg2)                         \
  DART_ARGS_2(arg0, arg1);                                    \
  DART_ARG(arg2, 2)
#define DART_ARGS_4(arg0, arg1, arg2, arg3)                   \
  DART_ARGS_3(arg0, arg1, arg2);                              \
  DART_ARG(arg3, 3)

#define DART_FUNCTION(name)                                   \
  static void name(Dart_NativeArguments arguments)

#define DART_RETURN(expr)                                     \
  {                                                           \
    Dart_SetReturnValue(arguments, expr);                     \
    Dart_ExitScope();                                         \
    return;                                                   \
  }

Dart_NativeFunction ResolveName(Dart_Handle name, int argc, bool* auto_setup_scope);

static Dart_PersistentHandle g_library;

typedef struct {
  sqlite3* db;
  sqlite3_stmt* stmt;
  Dart_WeakPersistentHandle finalizer;
} statement_peer;

DART_EXPORT Dart_Handle dart_sqlite_Init(Dart_Handle parent_library) {
  if (Dart_IsError(parent_library)) { return parent_library; }

  Dart_Handle result_code = Dart_SetNativeResolver(parent_library, ResolveName);
  if (Dart_IsError(result_code)) return result_code;

  g_library = Dart_NewPersistentHandle(parent_library);
  return parent_library;
}

void Throw(const char* message) {
  Dart_Handle library = Dart_HandleFromPersistent(g_library);
  Dart_Handle messageHandle = Dart_NewStringFromCString(message);
  Dart_Handle exceptionType = Dart_GetType(
      library, Dart_NewStringFromCString("SqliteException"), 0, NULL);
  Dart_ThrowException(Dart_New(
      exceptionType,
      Dart_NewStringFromCString("_internal"),
      1,
      &messageHandle));
}

void CheckSqlError(sqlite3* db, int result) {
  if (result) Throw(sqlite3_errmsg(db));
}

Dart_Handle CheckDartError(Dart_Handle result) {
  if (Dart_IsError(result)) Throw(Dart_GetError(result));
  return result;
}

sqlite3* get_db(Dart_Handle db_handle) {
  int64_t db_addr;
  Dart_IntegerToInt64(db_handle, &db_addr);
  return reinterpret_cast<sqlite3*>(db_addr);
}

statement_peer* get_statement(Dart_Handle statement_handle) {
  int64_t statement_addr;
  Dart_IntegerToInt64(statement_handle, &statement_addr);
  return reinterpret_cast<statement_peer*>(statement_addr);
}

DART_FUNCTION(New) {
  DART_ARGS_1(path);

  sqlite3* db;
  const char* cpath;
  CheckDartError(Dart_StringToCString(path, &cpath));
  CheckSqlError(db, sqlite3_open(cpath, &db));
  sqlite3_busy_timeout(db, 100);
  DART_RETURN(Dart_NewInteger((int64_t) db));
}

DART_FUNCTION(Close) {
  DART_ARGS_1(db_handle);

  sqlite3* db = get_db(db_handle);
  sqlite3_stmt* statement = NULL;
  int count = 0;
  while ((statement = sqlite3_next_stmt(db, statement))) {
    sqlite3_finalize(statement);
    count++;
  }
  if (count) {
    fprintf(stderr, "Warning: sqlite.Database.close(): %d statements still "
                    "open.\n", count);
  }
  CheckSqlError(db, sqlite3_close(db));
  DART_RETURN(Dart_Null());
}

DART_FUNCTION(Version) {
  DART_ARGS_0();

  DART_RETURN(Dart_NewStringFromCString(sqlite3_version));
}

void finalize_statement(Dart_WeakPersistentHandle handle, void* ctx) {
  static bool warned = false;
  statement_peer* statement = reinterpret_cast<statement_peer*>(ctx);
  sqlite3_finalize(statement->stmt);
  if (!warned) {
    fprintf(stderr, "Warning: sqlite.Statement was not closed before garbage "
                    "collection.\n");
    warned = true;
  }
  sqlite3_free(statement);
  Dart_DeleteWeakPersistentHandle(statement->finalizer);
}

DART_FUNCTION(PrepareStatement) {
  DART_ARGS_3(db_handle, sql_handle, statement_object);

  sqlite3* db = get_db(db_handle);
  const char* sql;
  sqlite3_stmt* stmt;
  CheckDartError(Dart_StringToCString(sql_handle, &sql));
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL)) {
    Dart_Handle library = Dart_HandleFromPersistent(g_library);
    Dart_Handle params[2];
    params[0] = Dart_NewStringFromCString(sqlite3_errmsg(db));
    params[1] = sql_handle;
    Dart_Handle syntaxExceptionType = Dart_GetType(
        library, Dart_NewStringFromCString("SqliteSyntaxException"), 0 , NULL);
    CheckDartError(syntaxExceptionType);
    Dart_ThrowException(Dart_New(
        syntaxExceptionType,
        Dart_NewStringFromCString("_internal"),
        2,
        params));
  }
  statement_peer* peer = static_cast<statement_peer*>(
      sqlite3_malloc(sizeof(statement_peer)));
  peer->db = db;
  peer->stmt = stmt;
  Dart_WeakPersistentHandle finalizer = Dart_NewWeakPersistentHandle(
      statement_object, peer, finalize_statement);
  CheckDartError(Dart_HandleFromWeakPersistent(finalizer));
  peer->finalizer = finalizer;
  DART_RETURN(Dart_NewInteger((int64_t) peer));
}

DART_FUNCTION(Reset) {
  DART_ARGS_1(statement_handle);

  statement_peer* statement = get_statement(statement_handle);
  CheckSqlError(statement->db, sqlite3_clear_bindings(statement->stmt));
  CheckSqlError(statement->db, sqlite3_reset(statement->stmt));
  DART_RETURN(Dart_Null());
}

DART_FUNCTION(Bind) {
  DART_ARGS_2(statement_handle, args);

  statement_peer* statement = get_statement(statement_handle);
  if (!Dart_IsList(args)) {
    Throw("args must be a List");
  }
  intptr_t count;
  Dart_ListLength(args, &count);
  if (sqlite3_bind_parameter_count(statement->stmt) != count) {
    Throw("Number of arguments doesn't match number of placeholders");
  }
  int ret_code;
  for (int i = 0; i < count; i++) {
    Dart_Handle value = Dart_ListGetAt(args, i);
    if (Dart_IsInteger(value)) {
      int64_t result;
      Dart_IntegerToInt64(value, &result);
      CheckSqlError(statement->db,
                    sqlite3_bind_int64(statement->stmt, i + 1, result));
    } else if (Dart_IsDouble(value)) {
      double result;
      Dart_DoubleValue(value, &result);
      CheckSqlError(statement->db,
                    sqlite3_bind_double(statement->stmt, i + 1, result));
    } else if (Dart_IsNull(value)) {
      CheckSqlError(statement->db, sqlite3_bind_null(statement->stmt, i + 1));
    } else if (Dart_IsString(value)) {
      const char* result;
      CheckDartError(Dart_StringToCString(value, &result));
      ret_code = sqlite3_bind_text(
          statement->stmt, i + 1, result, strlen(result), SQLITE_TRANSIENT);
      CheckSqlError(statement->db, ret_code);
    } else if (Dart_GetTypeOfTypedData(value) == Dart_TypedData_kByteData) {
      Dart_TypedData_Type type;
      void* data;
      intptr_t len;
      Dart_Handle internal_data = Dart_TypedDataAcquireData(
          value, &type, &data, &len);
      CheckDartError(internal_data);
      // Copy over using sqlite3_malloc
      unsigned char* result = (unsigned char*) sqlite3_malloc(count);
      for (int j = 0; j < len; j++) {
        result[j] = ((unsigned char*) data)[j];
      }
      ret_code = sqlite3_bind_blob(
          statement->stmt, i + 1, result, len, sqlite3_free);
      CheckSqlError(statement->db, ret_code);
    } else {
      Throw("Invalid parameter type");
    }
  }
  DART_RETURN(Dart_Null());
}

Dart_Handle get_column_value(statement_peer* statement, int col) {
  int count;
  const unsigned char* binary_data;
  Dart_Handle result;
  switch (sqlite3_column_type(statement->stmt, col)) {
    case SQLITE_INTEGER:
      return Dart_NewInteger(sqlite3_column_int64(statement->stmt, col));
    case SQLITE_FLOAT:
      return Dart_NewDouble(sqlite3_column_double(statement->stmt, col));
    case SQLITE_TEXT:
      return Dart_NewStringFromCString(
          reinterpret_cast<const char*>(
              sqlite3_column_text(statement->stmt, col)));
    case SQLITE_BLOB:
      {
        count = sqlite3_column_bytes(statement->stmt, col);
        result = CheckDartError(Dart_NewTypedData(
            Dart_TypedData_kByteData, count));
        binary_data = reinterpret_cast<const unsigned char*>(
            sqlite3_column_blob(statement->stmt, col));
        Dart_TypedData_Type type;
        void* data;
        intptr_t len;
        Dart_Handle internal_data = Dart_TypedDataAcquireData(
            result, &type, &data, &len);
        CheckDartError(internal_data);
        for (int i = 0; i < count; i++) {
          ((unsigned char*) data)[i] = binary_data[i];
        }
      }
      return result;
    case SQLITE_NULL:
      return Dart_Null();
    default:
      Throw("Unknown result type");
      return Dart_Null();
  }
}

Dart_Handle get_last_row(statement_peer* statement) {
  int count = sqlite3_column_count(statement->stmt);
  Dart_Handle list = CheckDartError(Dart_NewList(count));
  for (int i = 0; i < count; i++) {
    Dart_ListSetAt(list, i, get_column_value(statement, i));
  }
  return list;
}

DART_FUNCTION(ColumnInfo) {
  DART_ARGS_1(statement_handle);

  statement_peer* statement = get_statement(statement_handle);
  int count = sqlite3_column_count(statement->stmt);
  Dart_Handle result = Dart_NewList(count);
  for (int i = 0; i < count; i++) {
    Dart_ListSetAt(result, i, Dart_NewStringFromCString(
        sqlite3_column_name(statement->stmt, i)));
  }
  DART_RETURN(result);
}

DART_FUNCTION(Step) {
  DART_ARGS_1(statement_handle);

  statement_peer* statement = get_statement(statement_handle);
  while (true) {
    int status = sqlite3_step(statement->stmt);
    switch (status) {
      case SQLITE_BUSY:
        // TODO(xxx): roll back transaction?
        continue;
      case SQLITE_DONE:
        {
          int changes = 0;
          if (sqlite3_stmt_readonly(statement->stmt) == 0) {
            changes = sqlite3_changes(statement->db);
          }
          DART_RETURN(Dart_NewInteger(changes));
        }
      case SQLITE_ROW:
        DART_RETURN(get_last_row(statement));
      default:
        CheckSqlError(statement->db, status);
        Throw("Unreachable");
    }
  }
}

DART_FUNCTION(CloseStatement) {
  DART_ARGS_1(statement_handle);

  statement_peer* statement = get_statement(statement_handle);
  CheckSqlError(statement->db, sqlite3_finalize(statement->stmt));
  Dart_DeleteWeakPersistentHandle(statement->finalizer);
  sqlite3_free(statement);
  DART_RETURN(Dart_Null());
}

#define EXPORT(func, args)                     \
  if (!strcmp(#func, cname) && argc == args) { \
    return func;                               \
  }
Dart_NativeFunction ResolveName(Dart_Handle name, int argc, bool* auto_setup_scope) {
  const char* cname;
  Dart_Handle check_error = Dart_StringToCString(name, &cname);
  if (Dart_IsError(check_error)) Dart_PropagateError(check_error);

  EXPORT(New, 1);
  EXPORT(Close, 1);
  EXPORT(Version, 0);
  EXPORT(PrepareStatement, 3);
  EXPORT(Reset, 1);
  EXPORT(Bind, 2);
  EXPORT(Step, 1);
  EXPORT(ColumnInfo, 1);
  EXPORT(CloseStatement, 1);
  return NULL;
}
