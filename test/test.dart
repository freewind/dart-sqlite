// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import "dart:io" as io;
import "dart:math";
import "package:unittest/unittest.dart";

import "../lib/sqlite.dart" as sqlite;

createBlogTable(sqlite.Database db) {
  db.execute("CREATE TABLE posts (title text, body text)");
}

testFirst(sqlite.Database db) {
  var row = db.first("SELECT ?+2, UPPER(?)", [3, "hEll0"]);
  expect(row[0], equals(5));
  expect(row[1], equals("HELL0"));
}

testRow(sqlite.Database db) {
  var row = db.first("SELECT 42 AS foo");
  expect(row.index, equals(0));
  expect(row[0], equals(42));
  expect(row['foo'], equals(42));
  expect(row.foo, equals(42));
  expect(row.asList(), equals([42]));
  expect(row.asMap(), equals({"foo": 42}));
}

testBulk(sqlite.Database db) {
  createBlogTable(db);
  var insert = db.prepare("INSERT INTO posts (title, body) VALUES (?,?)");
  try {
    expect(insert.execute(["hi", "hello world"]), 1);
    expect(insert.execute(["bye", "goodbye cruel world"]), 1);
  } finally {
    insert.close();
  }
  var rows = [];
  expect(db.execute("SELECT * FROM posts", [], (row) {
    rows.add(row); 
  }), 2);
  expect(rows.length, equals(2));
  expect(rows[0].title, "hi");
  expect(rows[1].title, "bye");
  expect(rows[0].index, 0);
  expect(rows[1].index, 1);
  rows = [];
  expect(db.execute("SELECT * FROM posts", [], (row) {
    rows.add(row);
    return true;
  }), 1);
  expect(rows.length, 1);
  expect(rows[0].title, "hi");
}

testTransactionSuccess(sqlite.Database db) {
  createBlogTable(db);
  expect(db.transaction(() {
    db.execute("INSERT INTO posts (title, body) VALUES (?,?)", ["", ""]);
    return 42;
  }), 42);
  expect(db.execute("SELECT * FROM posts"), 1);
}

class UnsupportedOperationException implements Exception {
  final String _msg;
  
  const UnsupportedOperationException([this._msg]);
  
  String toString() {
    if (_msg == null) {
      return "Exception";
    }
    return "Exception: ${_msg}";
  }
}

testTransactionFailure(sqlite.Database db) {
  createBlogTable(db);
  try {
    db.transaction(() {
      db.execute("INSERT INTO posts (title, body) VALUES (?,?)", ["", ""]);
      throw new UnsupportedOperationException("Abort, please!");
    });
    fail("Exception should have been propagated");
  } on UnsupportedOperationException catch(e) {}
  expect(db.execute("SELECT * FROM posts", [], (row) {
    fail("Callback for rows should not be called.");
  }), 0);
}

testSyntaxError(sqlite.Database db) {
  expect(() => db.execute("random non sql"), 
    throwsA(predicate((e) => e is sqlite.SqliteSyntaxException, 
                      "is a SqliteSyntaxException")));
}

testColumnError(sqlite.Database db) {
  expect(() => db.first("select 2+2")['qwerty'], 
      throwsA(predicate((e) => e is sqlite.SqliteException, 
      "is a SqliteException")));
}

main() {
  group('inMemory', () {
    sqlite.Database db = null;
    setUp(() {
      db = new sqlite.Database.inMemory();
      expect(db, isNotNull);
    });
    tearDown(() {
      expect(db, isNotNull);
      db.close();
    });
    test("OpenClose", () {});
    test("BasicSQL", () { testFirst(db); });
    test("TestRow", () { testRow(db); });
    test("TestBulk", () { testBulk(db); });
    test("TransactionSuccess", () { testTransactionSuccess(db); });
    test("TransactionFailure", () { testTransactionFailure(db); });
    test("SyntaxError", () { testSyntaxError(db); });
    test("ColumnError", () { testColumnError(db); });
  });
  
  var nonce = new Random().nextInt(9999);
  String db_file = "./sqlite-dbtest-${nonce}.db";
  group('file', () {
    sqlite.Database db = null;
    setUp(() {
      db = new sqlite.Database(db_file);
      expect(db, isNotNull);
    });
    tearDown(() {
      expect(db, isNotNull);
      db.close();
      var f = new io.File(db_file);
      if (f.existsSync()) f.deleteSync();
    });
    test("OpenClose", () {});
    test("BasicSQL", () { testFirst(db); });
    test("TestRow", () { testRow(db); });
    test("TestBulk", () { testBulk(db); });
    test("TransactionSuccess", () { testTransactionSuccess(db); });
    test("TransactionFailure", () { testTransactionFailure(db); });
    test("SyntaxError", () { testSyntaxError(db); });
    test("ColumnError", () { testColumnError(db); });
  });
}
