/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "server.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

using ::featureform::embedding::proto::CreateSpaceRequest;
using ::featureform::embedding::proto::CreateSpaceResponse;
using ::featureform::embedding::proto::DownloadRequest;
using ::featureform::embedding::proto::DownloadResponse;
using ::featureform::embedding::proto::DownloadSpacesRequest;
using ::featureform::embedding::proto::DownloadSpacesResponse;
using ::featureform::embedding::proto::Embedding;
using ::featureform::embedding::proto::FreezeSpaceRequest;
using ::featureform::embedding::proto::FreezeSpaceResponse;
using ::featureform::embedding::proto::GetRequest;
using ::featureform::embedding::proto::GetResponse;
using ::featureform::embedding::proto::GetSpaceRequest;
using ::featureform::embedding::proto::GetVersionRequest;
using ::featureform::embedding::proto::ListEntriesRequest;
using ::featureform::embedding::proto::ListEntriesResponse;
using ::featureform::embedding::proto::ListVersionsRequest;
using ::featureform::embedding::proto::ListVersionsResponse;
using ::featureform::embedding::proto::MultiGetRequest;
using ::featureform::embedding::proto::MultiGetResponse;
using ::featureform::embedding::proto::MultiSetRequest;
using ::featureform::embedding::proto::MultiSetResponse;
using ::featureform::embedding::proto::NearestNeighborRequest;
using ::featureform::embedding::proto::NearestNeighborResponse;
using ::featureform::embedding::proto::SetRequest;
using ::featureform::embedding::proto::SetResponse;
using ::featureform::embedding::proto::SpaceEntry;
using ::featureform::embedding::proto::VersionEntry;
using ::grpc::Server;
using ::grpc::ServerBuilder;
using ::grpc::ServerContext;
using ::grpc::ServerReader;
using ::grpc::ServerReaderWriter;
using ::grpc::ServerWriter;
using ::grpc::Status;
using ::grpc::StatusCode;

namespace featureform {

namespace embedding {

constexpr auto DEFAULT_VERSION = "initial";
constexpr auto DEFAULT_DESCRIPTION = "default description";
constexpr auto DEFAULT_OWNER = "owner";
constexpr auto DEFAULT_TAGS = {"tag_1", "tag_2"};
constexpr auto DEFAULT_CREATED = "2011-03-10T11:23:56.123+0100";
constexpr auto DEFAULT_REVISION = "2011-03-10T11:23:56.123+0100";

std::vector<float> copy_embedding_to_vector(const Embedding& embedding) {
  auto vals = embedding.values();
  return std::vector<float>(vals.cbegin(), vals.cend());
}

template <typename T>
bool remove_uniq_value(std::vector<T>& vec, T val) {
  auto pos = std::find(vec.begin(), vec.end(), val);
  if (pos != vec.end()) {
    vec.erase(pos);
    return true;
  }
  return false;
}

grpc::Status EmbeddingHubService::CreateSpace(ServerContext* context,
                                              const CreateSpaceRequest* request,
                                              CreateSpaceResponse* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto space = store_->create_space(request->name(), DEFAULT_VERSION);
  space->create_version(DEFAULT_VERSION, request->dims(), DEFAULT_DESCRIPTION,
                        DEFAULT_OWNER, DEFAULT_TAGS, DEFAULT_CREATED,
                        DEFAULT_REVISION);
  return Status::OK;
}

grpc::Status EmbeddingHubService::FreezeSpace(ServerContext* context,
                                              const FreezeSpaceRequest* request,
                                              FreezeSpaceResponse* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto space_opt = store_->get_space(request->name());
  if (!space_opt) {
    return Status(StatusCode::NOT_FOUND, "Not found");
  }
  auto version_opt = GetVersion(request->name(), DEFAULT_VERSION);
  if (!version_opt.has_value()) {
    return Status(StatusCode::NOT_FOUND, "Not found");
  }
  version_opt.value()->make_immutable();
  return Status::OK;
}

grpc::Status EmbeddingHubService::Get(ServerContext* context,
                                      const GetRequest* request,
                                      GetResponse* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto version_opt = GetVersion(request->space(), DEFAULT_VERSION);
  if (!version_opt.has_value()) {
    return Status(StatusCode::NOT_FOUND, "Not found");
  }
  auto embedding = version_opt.value()->get(request->key());
  std::unique_ptr<Embedding> proto_embedding(new Embedding());
  *proto_embedding->mutable_values() = {embedding.begin(), embedding.end()};
  resp->set_allocated_embedding(proto_embedding.release());
  return Status::OK;
}

grpc::Status EmbeddingHubService::Set(ServerContext* context,
                                      const SetRequest* request,
                                      SetResponse* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto vec = copy_embedding_to_vector(request->embedding());
  auto version_opt = GetVersion(request->space(), DEFAULT_VERSION);
  if (!version_opt.has_value()) {
    return Status(StatusCode::NOT_FOUND, "Not found");
  }
  auto err = version_opt.value()->set(request->key(), vec);

  if (err != nullptr) {
    return Status(StatusCode::FAILED_PRECONDITION,
                  "Cannot write to immutable space");
  }
  return Status::OK;
}

grpc::Status EmbeddingHubService::MultiSet(
    ServerContext* context, ServerReader<MultiSetRequest>* reader,
    MultiSetResponse* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  MultiSetRequest request;
  while (reader->Read(&request)) {
    auto vec = copy_embedding_to_vector(request.embedding());
    auto version_opt = GetVersion(request.space(), DEFAULT_VERSION);
    if (!version_opt.has_value()) {
      return Status(StatusCode::NOT_FOUND, "Not found");
    }
    auto err = (*version_opt)->set(request.key(), vec);
    if (err != nullptr) {
      return Status(StatusCode::FAILED_PRECONDITION,
                    "Cannot write to immutable space");
    }
  }
  return Status::OK;
}

grpc::Status EmbeddingHubService::MultiGet(
    ServerContext* context,
    ServerReaderWriter<MultiGetResponse, MultiGetRequest>* stream) {
  std::unique_lock<std::mutex> lock(mtx_);
  MultiGetRequest request;
  while (stream->Read(&request)) {
    auto version_opt = GetVersion(request.space(), DEFAULT_VERSION);
    if (!version_opt.has_value()) {
      return Status(StatusCode::NOT_FOUND, "Not found");
    }
    auto embedding = (*version_opt)->get(request.key());
    std::unique_ptr<Embedding> proto_embedding(new Embedding());
    *proto_embedding->mutable_values() = {embedding.begin(), embedding.end()};
    MultiGetResponse resp;
    resp.set_allocated_embedding(proto_embedding.release());
    stream->Write(resp);
  }

  return Status::OK;
}

grpc::Status EmbeddingHubService::NearestNeighbor(
    ServerContext* context, const NearestNeighborRequest* request,
    NearestNeighborResponse* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto version_opt = GetVersion(request->space(), DEFAULT_VERSION);
  if (!version_opt.has_value()) {
    return Status(StatusCode::NOT_FOUND, "Not found");
  }
  auto version = *version_opt;
  auto has_key = request->key() != "";
  auto has_vec = request->embedding().values_size() != 0;
  if (has_key && has_vec) {
    return Status(StatusCode::INVALID_ARGUMENT,
                  "Key and embedding cannot both be set");
  }
  if (!has_key && !has_vec) {
    return Status(StatusCode::INVALID_ARGUMENT, "Key or embedding must be set");
  }

  std::vector<float> ref_vec;
  auto num_retrieve = request->num();
  if (has_key) {
    auto ref_key = request->key();
    ref_vec = version->get(ref_key);
    // We may retrieve the key that we're getting the nearest neighbors of.
    // We'll remove it later.
    num_retrieve += 1;
  } else {
    ref_vec = copy_embedding_to_vector(request->embedding());
  }
  auto nearest =
      version->get_ann_index()->approx_nearest(ref_vec, num_retrieve);
  // Remove the key we used to retrieve from the index if it exists.
  if (has_key && !remove_uniq_value(nearest, request->key())) {
    nearest.pop_back();
  }
  *resp->mutable_keys() = {nearest.begin(), nearest.end()};
  return Status::OK;
}

grpc::Status EmbeddingHubService::Download(
    ServerContext* context, const DownloadRequest* request,
    ServerWriter<DownloadResponse>* writer) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto version_opt = GetVersion(request->space(), DEFAULT_VERSION);
  if (!version_opt.has_value()) {
    return Status(StatusCode::NOT_FOUND, "Not found");
  }
  auto iter = version_opt.value()->iterator();
  while (iter.scan()) {
    auto key = iter.key();
    auto embedding = iter.value();
    std::unique_ptr<Embedding> proto_embedding(new Embedding());
    *proto_embedding->mutable_values() = {embedding.begin(), embedding.end()};
    DownloadResponse resp;
    resp.set_allocated_embedding(proto_embedding.release());
    resp.set_key(key);
    writer->Write(resp);
  }

  return Status::OK;
}

grpc::Status EmbeddingHubService::DownloadSpaces(
    ServerContext* context, const DownloadSpacesRequest* request,
    ServerWriter<DownloadSpacesResponse>* writer) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto iter = store_->iterator();
  while (iter.scan()) {
    DownloadSpacesResponse resp;
    resp.set_space(iter.key());
    writer->Write(resp);
  }
  return Status::OK;
}

grpc::Status EmbeddingHubService::ListVersions(
    ServerContext* context, const ListVersionsRequest* request,
    ServerWriter<ListVersionsResponse>* writer) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto iter = request->space()->iterator();
  while (iter.scan()) {
    ListVersionsResponse resp;
    resp.set_version(iter.key());
    writer->Write(resp);
  }
  return Status::OK;
}

std::optional<std::shared_ptr<Version>> EmbeddingHubService::GetVersion(
    const std::string& space_name, const std::string& version_name) {
  auto space_opt = store_->get_space(space_name);
  if (!space_opt.has_value()) {
    return std::nullopt;
  }
  return (*space_opt)->get_version(version_name);
}

grpc::Status EmbeddingHubService::GetSpaceEntry(
    grpc::ServerContext* context, const proto::GetSpaceRequest* request,
    proto::SpaceEntry* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto space_opt = store_->get_space(request->name());
  resp.set_path(space_opt->path());
  resp.set_name(space_opt->name());
  resp.set_default_version(space_opt->default_version());
  return Status::OK;
}

grpc::Status EmbeddingHubService::GetVersionEntry(
    grpc::ServerContext* context, const proto::GetVersionRequest* request,
    proto::VersionEntry* resp) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto version_opt =
      store_->get_space(request->space())->get_version(request->version());
  resp.set_path(version_opt->path());
  resp.set_space(version_opt->space());
  resp.set_name(version_opt->name());
  resp.set_description(version_opt->desc());
  resp.set_owner(version_opt->owner());
  for (int i = 0; i < version_opt->tags().size(); i++) {
    resp.add_tags(version_opt->tags()[i])
  }
  resp.set_created(version_opt->created());
  resp.set_revision(version_opt->revision());
  return Status::OK;
}

grpc::Status EmbeddingHubService::ListEntries(
    grpc::ServerContext* context, const proto::ListEntriesRequest* request,
    grpc::ServerWriter<proto::ListEntriesResponse>* writer) {
  std::unique_lock<std::mutex> lock(mtx_);
  auto space_iter = store_->iterator();
  auto space_opt = store_->get_space(request->space());
  while (iter.scan()) {
    ListEntriesResponse resp;
    resp.set_name(iter.key());
    auto space_opt = store_->get_space(iter.key());
    auto version_iter = space_opt->iterator();
    while (version_iter.scan()) {
      auto version_opt = space_opt->get_version(version_iter.key());
      VersionEntry ver_entry;
      ver_entry.set_path(version_opt->path());
      ver_entry.set_space(version_opt->space());
      ver_entry.set_name(version_opt->name());
      ver_entry.set_description(version_opt->desc());
      ver_entry.set_owner(version_opt->owner());
      for (int i = 0; i < version_opt->tags().size(); i++) {
        ver_entry.add_tags(version_opt->tags()[i])
      }
      ver_entry.set_created(version_opt->created());
      ver_entry.set_revision(version_opt->revision());
      resp.add_VersionEntry(ver_entry);
    }
    writer->Write(resp);
  }
  return Status::OK;
}

}  // namespace embedding
}  // namespace featureform

using featureform::embedding::EmbeddingHub;
using featureform::embedding::EmbeddingHubService;

void RunServer(const std::string& address) {
  auto store = EmbeddingHub::load_or_create("embedding_store.dat");
  auto service = EmbeddingHubService(std::move(store));

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}
