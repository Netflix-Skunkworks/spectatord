#pragma once

#include "logger.h"
#include <cstdio>

namespace spectatord {
class StdIoFile {
 public:
  explicit StdIoFile(const char* name) noexcept : fp_(std::fopen(name, "r")) {
    if (fp_ == nullptr) {
      Logger()->warn("Unable to open {}", name);
    } else {
      setbuffer(fp_, buf, sizeof buf);
    }
  }
  StdIoFile(const StdIoFile&) = delete;
  StdIoFile(StdIoFile&& other) noexcept : fp_(other.fp_) {
    other.fp_ = nullptr;
  }

  ~StdIoFile() {
    if (fp_ != nullptr) {
      std::fclose(fp_);
      fp_ = nullptr;
    }
  }

  operator FILE*() const { return fp_; }

 private:
  FILE* fp_;
  char buf[65536];
};
}  // namespace spectatord