// Copyright 2017-2019 VMware, Inc.
// SPDX-License-Identifier: BSD-2-Clause
//
// The BSD-2 license (the License) set forth below applies to all parts of the
// Cascade project.  You may not use this file except in compliance with the
// License.
//
// BSD-2 License
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CASCADE_SRC_TARGET_INTERFACE_REMOTE_REMOTE_INTERFACE_H
#define CASCADE_SRC_TARGET_INTERFACE_REMOTE_REMOTE_INTERFACE_H

#include <cassert>
#include "common/sockstream.h"
#include "target/compiler/rpc.h"
#include "target/interface.h"

namespace cascade {

class RemoteInterface : public Interface {
  public:
    explicit RemoteInterface(sockstream* sock);
    ~RemoteInterface() override = default;

    void write(VId id, const Bits* b) override;
    void write(VId id, bool b) override;

    void debug(uint32_t action, const std::string& arg) override;
    void finish(uint32_t arg) override;
    void restart(const std::string& path) override;
    void retarget(const std::string& s) override;
    void save(const std::string& path) override;

    FId fopen(const std::string& path, uint8_t mode) override;
    int32_t in_avail(FId id) override;
    uint32_t pubseekoff(FId id, int32_t off, uint8_t way, uint8_t which) override;
    uint32_t pubseekpos(FId id, int32_t pos, uint8_t which) override;
    int32_t pubsync(FId id) override;
    int32_t sbumpc(FId id) override;
    int32_t sgetc(FId id) override;
    uint32_t sgetn(FId id, char* c, uint32_t n) override;
    int32_t sputc(FId id, char c) override;
    uint32_t sputn(FId id, const char* c, uint32_t n) override;
      
  private:
    sockstream* sock_;
}; 

inline RemoteInterface::RemoteInterface(sockstream* sock) : Interface() {
  sock_ = sock;
}

inline void RemoteInterface::write(VId id, const Bits* b) {
  Rpc(Rpc::Type::WRITE_BITS).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  b->serialize(*sock_);
}

inline void RemoteInterface::write(VId id, bool b) {
  Rpc(Rpc::Type::WRITE_BOOL).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->put(b ? 1 : 0);
}

inline void RemoteInterface::debug(uint32_t action, const std::string& arg) {
  Rpc(Rpc::Type::DEBUG).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&action), 4);
  sock_->write(arg.c_str(), arg.length());
  sock_->put('\0');
}

inline void RemoteInterface::finish(uint32_t arg) {
  Rpc(Rpc::Type::FINISH).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&arg), 4);
}

inline void RemoteInterface::restart(const std::string& path) {
  Rpc(Rpc::Type::RESTART).serialize(*sock_);
  sock_->write(path.c_str(), path.length());
  sock_->put('\0');
}

inline void RemoteInterface::retarget(const std::string& s) {
  Rpc(Rpc::Type::RETARGET).serialize(*sock_);
  sock_->write(s.c_str(), s.length());
  sock_->put('\0');
}

inline void RemoteInterface::save(const std::string& path) {
  Rpc(Rpc::Type::SAVE).serialize(*sock_);
  sock_->write(path.c_str(), path.length());
  sock_->put('\0');
}

inline FId RemoteInterface::fopen(const std::string& path, uint8_t mode) {
  Rpc(Rpc::Type::FOPEN).serialize(*sock_);
  sock_->write(path.c_str(), path.length());
  sock_->put('\0');
  sock_->write(reinterpret_cast<const char*>(&mode), sizeof(mode));
  sock_->flush();

  FId res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(FId));
  return res;
}

inline int32_t RemoteInterface::in_avail(FId id) {
  Rpc(Rpc::Type::IN_AVAIL).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->flush();

  int32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline uint32_t RemoteInterface::pubseekoff(FId id, int32_t off, uint8_t way, uint8_t which) {
  Rpc(Rpc::Type::PUBSEEKOFF).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->write(reinterpret_cast<const char*>(&off), sizeof(off));
  sock_->write(reinterpret_cast<const char*>(&way), sizeof(way));
  sock_->write(reinterpret_cast<const char*>(&which), sizeof(which));
  sock_->flush();

  uint32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline uint32_t RemoteInterface::pubseekpos(FId id, int32_t pos, uint8_t which) {
  Rpc(Rpc::Type::PUBSEEKPOS).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->write(reinterpret_cast<const char*>(&pos), sizeof(pos));
  sock_->write(reinterpret_cast<const char*>(&which), sizeof(which));
  sock_->flush();

  uint32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline int32_t RemoteInterface::pubsync(FId id) {
  Rpc(Rpc::Type::PUBSYNC).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->flush();

  int32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline int32_t RemoteInterface::sbumpc(FId id) {
  Rpc(Rpc::Type::SBUMPC).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->flush();

  int32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline int32_t RemoteInterface::sgetc(FId id) {
  Rpc(Rpc::Type::SGETC).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->flush();

  int32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline uint32_t RemoteInterface::sgetn(FId id, char* c, uint32_t n) {
  Rpc(Rpc::Type::SGETN).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->write(reinterpret_cast<const char*>(&n), sizeof(n));
  sock_->flush();

  uint32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  sock_->read(c, res);
  return res;
}

inline int32_t RemoteInterface::sputc(FId id, char c) {
  Rpc(Rpc::Type::SPUTC).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->put(c);
  sock_->flush();

  int32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

inline uint32_t RemoteInterface::sputn(FId id, const char* c, uint32_t n) {
  Rpc(Rpc::Type::SPUTN).serialize(*sock_);
  sock_->write(reinterpret_cast<const char*>(&id), sizeof(id));
  sock_->write(reinterpret_cast<const char*>(&n), sizeof(n));
  sock_->write(c, n);
  sock_->flush();

  uint32_t res;
  sock_->read(reinterpret_cast<char*>(&res), sizeof(res));
  return res;
}

} // namespace cascade

#endif
