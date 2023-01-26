// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// 暴露给用户的接口

#ifndef STORAGE_LEVELDB_INCLUDE_DB_H_
#define STORAGE_LEVELDB_INCLUDE_DB_H_

#include <cstdint>
#include <cstdio>

#include "leveldb/export.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace leveldb {

// Update CMakeLists.txt if you change these
static const int kMajorVersion = 1;
static const int kMinorVersion = 23;

// 这里构建了4个TYPE，是用#include "option.h"还是提前声明呢，提倡用头文件而不是提前声明forward declaration
// 如果是一个指针例如WriteOptions，forward declaration是可以的，编译器只在乎这个文件有没有被declare，并不在乎
// 有没有被用到，forward declaration in c++ pointer reference，不能是实体data member or a base class
// reference只是一个alias，是不占任何内存地址的，不去copy，函数是不占任何大小的，函数是只有在run time才调用
// 作为class的成员我们需要知道大小 declare可以定义不行，它的内容也不行，因为看不到内部
// 所以只要有头文件的存在，不仅会有声明也会有定义，compiler会自动去链接，就可以知道所有信息
struct Options;
struct ReadOptions;
struct WriteOptions;
class WriteBatch;

// Abstract handle to particular state of a DB.
// A Snapshot is an immutable object and can therefore be safely
// accessed from multiple threads without any external synchronization.
class LEVELDB_EXPORT Snapshot {
 protected:
  virtual ~Snapshot();
};

// A range of keys
struct LEVELDB_EXPORT Range {
  Range() = default;
  Range(const Slice& s, const Slice& l) : start(s), limit(l) {}

  Slice start;  // Included in the range
  Slice limit;  // Not included in the range
};

// A DB is a persistent ordered map from keys to values.
// A DB is safe for concurrent access from multiple threads without
// any external synchronization.
class LEVELDB_EXPORT DB {
 public:
  // Open the database with the specified "name".
  // Stores a pointer to a heap-allocated database in *dbptr and returns
  // OK on success.
  // Stores nullptr in *dbptr and returns a non-OK status on error.
  // Caller should delete *dbptr when it is no longer needed.

  // 把static mark as const没有任何意义，不被允许
  // 必须要加static，如果不加的话，那么就要实例化db，但是db不能被实例化
  // static function是否可以被子类继承，不可以DB::
  // 工厂函数必须是要static    factory function
  static Status Open(const Options& options, const std::string& name,
                     DB** dbptr);
  // *dbptr = new DB_imp() return ok, notok;
  // 不能是dbptr = new DB_imp() 
  // 因为实际上Open调用的时候是
  /*
  DB* db = nullptr;
  DB::Open(...,db);
  db->
  如果不是以指针的方式去new，实际上db还是null，就是局部变量，那这个时候返回的得是DB*，然后如果是不是null，就是正常的
  返回，不然就是null，但是这样的做法不太美观：
  即  static DB* Open(const Options& options, const std::string& name,
                     DB* dbptr)
  */
  DB() = default;

  DB(const DB&) = delete; //copy constructor diabled
  DB& operator=(const DB&) = delete; //不允许copy给db  assignment operator disabled
  // DB db1 = db2;
  // db1 = db2;

  virtual ~DB();//解析函数，被定义成虚函数，会被子类来继承并且override，但是不是一定要override？其实不一定
  // 虚函数， 除非它的子类都是pure virtual function，要implement/override，其他的情况都不需要ovrride
  // polymorphism
  // override, return input types, function name, cv-specier,overriding
  // overriding with/without virtual specifier
  // virtual specifier
  // DB* db = & dbimple;
  // db.overridefunc(); -> dbimple;
  // no member type, interface class/type. DB;

  // Set the database entry for "key" to "value".  Returns OK on success,
  // and a non-OK status on error.
  // Note: consider setting options.sync = true.
  virtual Status Put(const WriteOptions& options, const Slice& key,
                     const Slice& value) = 0; //如果=0,就是pure virtual function，那么这个class是pure virtual class -> cannot be instantiated
                    //  DB db{"hello"}; not allowed

  // Remove the database entry (if any) for "key".  Returns OK on
  // success, and a non-OK status on error.  It is not an error if "key"
  // did not exist in the database.
  // Note: consider setting options.sync = true.
  virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;

  // Apply the specified updates to the database.
  // Returns OK on success, non-OK on failure.
  // Note: consider setting options.sync = true.
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;

  // If the database contains an entry for "key" store the
  // corresponding value in *value and return OK.
  //
  // If there is no entry for "key" leave *value unchanged and return
  // a status for which Status::IsNotFound() returns true.
  //
  // May return some other Status on an error.
  virtual Status Get(const ReadOptions& options, const Slice& key,
                     std::string* value) = 0;

  // Return a heap-allocated iterator over the contents of the database.
  // The result of NewIterator() is initially invalid (caller must
  // call one of the Seek methods on the iterator before using it).
  //
  // Caller should delete the iterator when it is no longer needed.
  // The returned iterator should be deleted before this db is deleted.
  virtual Iterator* NewIterator(const ReadOptions& options) = 0;

  // Return a handle to the current DB state.  Iterators created with
  // this handle will all observe a stable snapshot of the current DB
  // state.  The caller must call ReleaseSnapshot(result) when the
  // snapshot is no longer needed.
  virtual const Snapshot* GetSnapshot() = 0;
  // 这个不能进行修改，在heap上构建

  // Release a previously acquired snapshot.  The caller must not
  // use "snapshot" after this call.
  virtual void ReleaseSnapshot(const Snapshot* snapshot) = 0;

  // DB implementations can export properties about their state
  // via this method.  If "property" is a valid property understood by this
  // DB implementation, fills "*value" with its current value and returns
  // true.  Otherwise returns false.
  //
  //
  // Valid property names include:
  //
  //  "leveldb.num-files-at-level<N>" - return the number of files at level <N>,
  //     where <N> is an ASCII representation of a level number (e.g. "0").
  //  "leveldb.stats" - returns a multi-line string that describes statistics
  //     about the internal operation of the DB.
  //  "leveldb.sstables" - returns a multi-line string that describes all
  //     of the sstables that make up the db contents.
  //  "leveldb.approximate-memory-usage" - returns the approximate number of
  //     bytes of memory in use by the DB.
  virtual bool GetProperty(const Slice& property, std::string* value) = 0;
  /*
  {
    *value = "hello" //assignment operator for string
  }
  */

  // For each i in [0,n-1], store in "sizes[i]", the approximate
  // file system space used by keys in "[range[i].start .. range[i].limit)".
  //
  // Note that the returned sizes measure file system space usage, so
  // if the user data compresses by a factor of ten, the returned
  // sizes will be one-tenth the size of the corresponding user data size.
  //
  // The results may not include the sizes of recently written data.
  virtual void GetApproximateSizes(const Range* range, int n,
                                   uint64_t* sizes) = 0;
  // primitive type, int, *->not necessary to mark it as const
  // const type *, const type&

// different choices of function input types:
  // & reference   Student & s1 = s2; 
  // xx Student & s1; wrong
  // 1. type must exist during initialization, s2 must exist，而不可以是null, initialization cannot delayed
  // 2. can not be null
  // always prefer reference to pointer when possible
  // uses case: read, modify, 

  // * pointer     Student * s1 = s2; 
  // Student * s1; s1 = &s2;
  // 1. can be null at initialization
  // 2. can be null at initialization
  // linkedlist, treeset, linking structure.
  // if the value may not exist, nullptr, have to use pointer.

// global variable, or local variable in main() function.
// if the data type is created/allocated on heap, we can only use *
// Student* std = new Student("dsf");
// Student* const & std = new Student("dsf"); //这个指针只能指向heap的这个位置，就是不能改变 没有意义

// function inputs/outputs， when to use const specifier
// pass by value:almost never need to mark as const;
// void func(Student stu);本来就是复制，但凡copy by value，很多是要修改
// void func(Student* stu);//copy by value，也不需要
// input parameter, if pass by value, never need to mark as const
// void func(const Student & stu); ->xx st.age = 10; xx stu.change_age(10)
// 一个constant reference能不能呼叫一个non-constant reference，不能


// output parameter, output by copy
// Student& func(){
// Student* stu = new Student("1");
// return *stu;
// } 不这么做，直接return pointer，如果不修改的话直接加一个const Student*
// 不需要Student* const func()


  // Compact the underlying storage for the key range [*begin,*end].
  // In particular, deleted and overwritten versions are discarded,
  // and the data is rearranged to reduce the cost of operations
  // needed to access the data.  This operation should typically only
  // be invoked by users who understand the underlying implementation.
  //
  // begin==nullptr is treated as a key before all keys in the database.
  // end==nullptr is treated as a key after all keys in the database.
  // Therefore the following call will compact the entire database:
  //    db->CompactRange(nullptr, nullptr);
  virtual void CompactRange(const Slice* begin, const Slice* end) = 0;
};

// Destroy the contents of the specified database.
// Be very careful using this method.
//
// Note: For backwards compatibility, if DestroyDB is unable to list the
// database files, Status::OK() will still be returned masking this failure.
LEVELDB_EXPORT Status DestroyDB(const std::string& name,
                                const Options& options);

// If a DB cannot be opened, you may attempt to call this method to
// resurrect as much of the contents of the database as possible.
// Some data may be lost, so be careful when calling this function
// on a database that contains important information.
LEVELDB_EXPORT Status RepairDB(const std::string& dbname,
                               const Options& options);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_DB_H_
