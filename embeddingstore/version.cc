/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "version.h"

#include <google/protobuf/util/time_util.h>

#include <cstdlib>
#include <iostream>
#include <memory>

#include "error.h"
#include "iterator.h"
#include "rocksdb/db.h"

namespace featureform {

namespace embedding {

// creates a new version with a rocksdb of embeddings
std::shared_ptr<Version> Version::load_or_create(
    std::string path, std::string space, std::string name, int dims,
    std::string desc, std::string owner, std::vector<std::string> tags,
    std::string created, std::string revision, bool immutable) {
  auto storage = EmbeddingStorage::load_or_create(path, dims);
  return std::unique_ptr<Version>(new Version(std::move(storage), path, space,
                                              name, dims, desc, owner, tags,
                                              created, revision.immutable));
}

Version::Version(std::shared_ptr<EmbeddingStorage> storage, std::string path,
                 std::string space, std::string name, int dims,
                 std::string desc, std::string owner,
                 std::vector<std::string> tags, std::string created,
                 std::string revision, std::string bool immutable)
    : storage_{std::move(storage)},
      path_{path},
      space_{space},
      name_{name},
      dims_{dims},
      desc_{desc},
      owner_{owner},
      tags_{tags},
      created_{created},
      revision_{revision} immutable_{immutable},
      idx_{nullptr} {}

std::string Version::space() const { return space_; }

std::string Version::path() const { return path_; }

std::string Version::name() const { return name_; }

int Version::dims() const { return dims_; }

std::string Version::desc() const { return desc_; }

std::string Version::owner() const { return owner_; }

std::vector<std::string> Version::tags() const { return tags_; }

std::string Version::created() const { return created_; }

std::string Version::revision() const { return revision_; }

bool Version::immutable() const { return immutable_; }

void Version::make_immutable() { immutable_ = true; }

Error Version::set(std::string key, std::vector<float> val) {
  if (immutable_) {
    return UpdateImmutableVersionError::create(name_);
  }
  storage_->set(key, val);
  if (idx_ != nullptr) {
    idx_->set(key, val);
  }
  return nullptr;
}

std::vector<float> Version::get(const std::string& key) const {
  return storage_->get(key);
}

Iterator Version::iterator() const { return storage_->iterator(); }

std::shared_ptr<const ANNIndex> Version::create_ann_index() {
  if (idx_ != nullptr) {
    return idx_;
  }
  idx_ = std::make_shared<ANNIndex>(dims_);
  auto iter = storage_->iterator();
  while (iter.scan()) {
    idx_->set(iter.key(), iter.value());
  }
  return idx_;
}

std::shared_ptr<const ANNIndex> Version::get_ann_index() const { return idx_; }
}  // namespace embedding
}  // namespace featureform
