// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A Status encapsulates the result of an operation.  It may indicate success,
// or it may indicate an error with an associated error message.
//
// Multiple threads can invoke const methods on a Status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Status must use
// external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_STATUS_H_
#define STORAGE_LEVELDB_INCLUDE_STATUS_H_

#include <algorithm>
#include <string>

#include "leveldb/export.h"
#include "leveldb/slice.h"

namespace leveldb {

class LEVELDB_EXPORT Status {
 public:
  // Create a success status.  不是接口函数
  Status() noexcept : state_(nullptr) {} //noexcept不会抛出异常
  ~Status() { delete[] state_; }//RA-II， heap-》constructor. release heap->destructor

  Status(const Status& rhs);//copy constructor
  Status& operator=(const Status& rhs);//copy assignment operator

// move constructor are usually designed not to throw，有意义的量传过来
  Status(Status&& rhs) noexcept : state_(rhs.state_) { rhs.state_ = nullptr; }//move constructor
  Status& operator=(Status&& rhs) noexcept;//move assignment operator

  // Return a success status. 跟目前的status没有相关性，就是static  null就是ok
  static Status OK() { return Status(); }

  // Return error status of an appropriate type.
  static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) { //slice()默认构建，就是空
    return Status(kNotFound, msg, msg2);
  }
  static Status Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kCorruption, msg, msg2);
  }
  static Status NotSupported(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotSupported, msg, msg2);
  }
  static Status InvalidArgument(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kInvalidArgument, msg, msg2);
  }
  static Status IOError(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kIOError, msg, msg2);
  }

  // Returns true iff the status indicates success.
  bool ok() const { return (state_ == nullptr); }

  // Returns true iff the status indicates a NotFound error.
  bool IsNotFound() const { return code() == kNotFound; }

  // Returns true iff the status indicates a Corruption error.
  bool IsCorruption() const { return code() == kCorruption; }

  // Returns true iff the status indicates an IOError.
  bool IsIOError() const { return code() == kIOError; }

  // Returns true iff the status indicates a NotSupportedError.
  bool IsNotSupportedError() const { return code() == kNotSupported; }

  // Returns true iff the status indicates an InvalidArgument.
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;

 private:
  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5
  };

  Code code() const {
    return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);//char也是一个integer value  char->integral type to enum
  }
  // static_cost:
  // 1. conversion betweeen numerical types. double -> int static_cast<int>(d)  size_t->uint32_t
  // 2. conversion of reference or pointer to types in the same family tree.
  // 3. types with conversion constructor, conversion operator.

// code cannot be ok
  Status(Code code, const Slice& msg, const Slice& msg2);
  // reusable code segements
  static const char* CopyState(const char* s);

  // OK status has a null state_.  Otherwise, state_ is a new[] array
  // of the following form:
  //    state_[0..3] == length of message, 4*8=32bit -> uint32_t
  //    state_[4]    == code
  //    state_[5..]  == message
  const char* state_;//最先关注
  /*
  state="helll"
  state = new char[5];
  state[0]='1' wrong
  不可以修改内容，但是可以修改指向

  1. c-style string. strlen(state_)? \n
  2. std::string state_;
  3. const char* state_,size_t length;

  */
};

/*
1. use case: move constructor
inline Status funct(const Status &s){
  Status status{s};//copy constr
  return status;//destructor
}
2. use case:calling std::move() to create rvalue reference from lvalue reference;
then pass into either move constructor Status(Status && s1); or move assignment operator
Status& operator=(Status&&s);
inline void swap(std::string &s1, std::string &s2){
  std::string temp{std::move(s1)};   //copy constructor
  s1 = std::move(s2);       //move assignment
  s2 = std::move(temp);     //copy assignment
}

1. constructor, copy constructor, copy assignment, move constructor, move assignment, destructor
2. if any constructor including copy is declared, then complier will not generate a default constructor.
3, if any copy constructor,copy assignment, move constructor, move assignment, destructor is declared,
then none of the rest of the four will be auto generated.

1. wrapper includes pointer that is allocated on the heap. int->32bit, pointer deleted.
   1.1 copy assignment -> 1. copy, allocate memory 2. release original heap memory; check self-assignment
   1.2 move assignment -> 1. copy pointer address 2. pass-in pointer set to null; release original heap memory. check self-move-assignment
   1.3 copy constructor -> 1. copy, allocate memory
   1.4 move constructor -> 1. copy pointer address, 2. pass-in pointer set to null; no check self-construction.

C-style string
char str[]{"hello"}; //size=6
char str[]="hello"; // copy initialization size=6
char str[]{'h','e','l','l','o','\0'}; //size=6
char str[20];//declare without initialization   if static/namespace array is value initialized to zero
// local scope is uinitialized
str="hello" copy assignment operator of array prohibited. copy constructor is ok
// how to pass an array as a function input argument, size information is lost after function call.

void func(char arr[]); -> void func(char*arr);

normal char array:
char str2[]{'h','e','l','\0','l','o','\0'}; //size=7  strlen(str2)=3
char str[]="he\0\l\0lo"; //normal char array.

c++ assign array to another array:
str2=str; array size is fixed. no

how to know the size of an array, number of elements;
sizeof(str2)/sizeof(str2[0])=7*1/1=7
*/
inline Status::Status(const Status& rhs) {
  state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
}
inline Status& Status::operator=(const Status& rhs) {
  // The following condition catches both aliasing (when this == &rhs),
  // and the common case where both rhs and *this are ok.
  /*
  self-assignment:
  if(this == &rhs){
    return *this;
  }
  */
  if (state_ != rhs.state_) {
    delete[] state_; //[] delete array
    state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
  }
  return *this;
}
inline Status& Status::operator=(Status&& rhs) noexcept {
  std::swap(state_, rhs.state_);
  return *this;
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_STATUS_H_
