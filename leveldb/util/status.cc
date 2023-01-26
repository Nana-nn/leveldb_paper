// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/status.h"

#include <cstdio>

#include "port/port.h"

namespace leveldb {

const char* Status::CopyState(const char* state) {
  uint32_t size;//default-initialization, undefined value
  // reinterpret是一个很危险的操作，编译器不会报错，但是Runtime会
  std::memcpy(&size, state, sizeof(size));//memcpy直接拷贝raw data  sizeof(size)=4 byte avoid magic number
  // state的前4个byte copy to size
  // make_unique(内存自动释放)
  // memcpy的效率要高于strcpy(dest,src)  src c-style string,如果不是的话，只有其中一部分复制进来
  char* result = new char[size + 5]; //5个byte的overhead，前4个是length,第5个是code，之后才是msg
  std::memcpy(result, state, size + 5);
  return result;
}

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  assert(code != kOk);
  const uint32_t len1 = static_cast<uint32_t>(msg.size());//numeric type, int->double 可implicit，大的变小得到一定要cast
  // int i=0.5 ok
  // int i{0.5}; curly brackets doesn't work with truncation.
  const uint32_t len2 = static_cast<uint32_t>(msg2.size()); // copy/move construct??
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0); //numeric,boolean -> implicit boolean conversion
  char* result = new char[size + 5]; //unitialized local scope, contents are uinitailized.
  std::memcpy(result, &size, sizeof(size));
  result[4] = static_cast<char>(code); //char <-> Code(int), lossless
  // 如果有1万个type，那就不work了  char装不了这么大
  std::memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    std::memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string Status::ToString() const {
  if (state_ == nullptr) {
    return "OK";
  } else {
    char tmp[30]; // array to pointer decay, size info will be missing 例如传入函数  \0
    const char* type;
    switch (code()) {
      case kOk: //switch label, constexpr C++11;const;
        type = "OK";//c-style string
        break;
      case kNotFound:
        type = "NotFound: ";
        break;
      case kCorruption:
        type = "Corruption: ";
        break;
      case kNotSupported:
        type = "Not implemented: ";
        break;
      case kInvalidArgument:
        type = "Invalid argument: ";
        break;
      case kIOError:
        type = "IO error: ";
        break;
      default:
      // strcpy //memcpy 
        std::snprintf(tmp, sizeof(tmp),
                      "Unknown code(%d): ", static_cast<int>(code())); //named cast
        type = tmp;
        break;
    }
    std::string result(type);
    uint32_t length;
    std::memcpy(&length, state_, sizeof(length));//implicit type conversion
    // 4 bytes of char to one uint32_t
    result.append(state_ + 5, length);
    return result;//copy constructor,move constructor? move
  }
}

}  // namespace leveldb
