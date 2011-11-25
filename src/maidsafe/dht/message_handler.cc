  /* Copyright (c) 2010 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "maidsafe/dht/message_handler.h"
#include "boost/lexical_cast.hpp"
#include "maidsafe/dht/log.h"
#include "maidsafe/common/securifier.h"
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/rpcs.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif

namespace maidsafe {

namespace dht {

void MessageHandler::OnMessageReceived(const std::string &request,
                                       const transport::Info &info,
                                       std::string *response,
                                       transport::Timeout *timeout) {
  if (request.empty())
    return;
  SecurityType security_type = request.at(0);
  if (security_type) {
    on_message_received_(request, info, response, timeout);
//     transport::MessageHandler::OnMessageReceived(request, info, response,
//                                                  timeout);
    return;
  } else if (!securifier_) {
    return;
  }
  std::string serialised_message(request.substr(1));
  if (security_type & kAsymmetricEncrypt) {
    std::string aes_seed = request.substr(1, 512);
    if (aes_seed.size() != 512)
      return;

    std::string encrypt_aes_seed = securifier_->AsymmetricDecrypt(aes_seed);
    if (encrypt_aes_seed.empty()) {
      DLOG(WARNING) << "Failed to decrypt: encrypt_aes_seed is empty.";
      return;
    }

    std::string aes_key = encrypt_aes_seed.substr(0, 32);
    std::string kIV = encrypt_aes_seed.substr(32, 16);
    serialised_message = crypto::SymmDecrypt(request.substr(513), aes_key, kIV);
  }
  int msg_type;
  std::string payload;
  std::string message_signature;
  if (UnwrapWrapperMessage(serialised_message, &msg_type, &payload,
                       &message_signature)) {
    ProcessSerialisedMessage(msg_type, payload,
                             security_type, message_signature,
                             info, response, timeout);
  }
}

std::string MessageHandler::WrapMessage(
    const protobuf::PingRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kPingRequest, msg.SerializeAsString(),
                                      kAsymmetricEncrypt, recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::PingResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kPingResponse, msg.SerializeAsString(),
                                      kAsymmetricEncrypt, recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::FindValueRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kFindValueRequest,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt, recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::FindValueResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kFindValueResponse,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::FindNodesRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kFindNodesRequest,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::FindNodesResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kFindNodesResponse,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::StoreRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kStoreRequest,
                                      msg.SerializeAsString(),
                                      kSign | kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::StoreResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kStoreResponse, msg.SerializeAsString(),
                                      kAsymmetricEncrypt, recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::StoreRefreshRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kStoreRefreshRequest,
                                      msg.SerializeAsString(),
                                      kSign | kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::StoreRefreshResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kStoreRefreshResponse,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::DeleteRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kDeleteRequest,
                                      msg.SerializeAsString(),
                                      kSign | kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::DeleteResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kDeleteResponse, msg.SerializeAsString(),
                                      kAsymmetricEncrypt, recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::DeleteRefreshRequest &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kDeleteRefreshRequest,
                                      msg.SerializeAsString(),
                                      kSign | kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::DeleteRefreshResponse &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kDeleteRefreshResponse,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt,
                                      recipient_public_key);
}

std::string MessageHandler::WrapMessage(
    const protobuf::DownlistNotification &msg,
    const std::string &recipient_public_key) {
  if (!msg.IsInitialized())
    return "";
  return MakeSerialisedWrapperMessage(kDownlistNotification,
                                      msg.SerializeAsString(),
                                      kAsymmetricEncrypt,
                                      recipient_public_key);
}

void MessageHandler::ProcessSerialisedMessage(
    const int &message_type,
    const std::string &payload,
    const SecurityType &security_type,
    const std::string &message_signature,
    const transport::Info &info,
    std::string *message_response,
    transport::Timeout* timeout) {
  message_response->clear();
  *timeout = transport::kImmediateTimeout;
  switch (message_type) {
    case kPingRequest: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::PingRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        protobuf::PingResponse response;
        (*on_ping_request_)(info, request, &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kPingResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::PingResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_ping_response_)(info, response);
      break;
    }
    case kFindValueRequest: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::FindValueRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        protobuf::FindValueResponse response;
        (*on_find_value_request_)(info, request, &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kFindValueResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::FindValueResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_find_value_response_)(info, response);
      break;
    }
    case kFindNodesRequest: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::FindNodesRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        protobuf::FindNodesResponse response;
        (*on_find_nodes_request_)(info, request, &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kFindNodesResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::FindNodesResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_find_nodes_response_)(info, response);
      break;
    }
    case kStoreRequest: {
      if ((security_type != (kSign | kAsymmetricEncrypt)) ||
          (message_signature.empty() && !securifier_->kSigningKeyId().empty()))
        return;
      protobuf::StoreRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        if (!request.sender().has_node_id())
          return;
        std::string message =
            boost::lexical_cast<std::string>(message_type) + payload;
        std::string public_key_id, public_key, other_info;
        if (request.sender().has_public_key_id())
          public_key_id = request.sender().public_key_id();
        if (request.sender().has_public_key())
          public_key = request.sender().public_key();
        if (request.sender().has_other_info())
          other_info = request.sender().other_info();
        securifier_->GetPublicKeyAndValidation(public_key_id, &public_key,
                                               &other_info);
        if (!securifier_->Validate(message, message_signature, public_key_id,
                                   public_key, other_info,
                                   request.sender().node_id()))
          return;
        protobuf::StoreResponse response;
        (*on_store_request_)(info, request, payload, message_signature,
                             &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kStoreResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::StoreResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_store_response_)(info, response);
      break;
    }
    case kStoreRefreshRequest: {
      if ((security_type != (kSign | kAsymmetricEncrypt)) ||
          (message_signature.empty() && !securifier_->kSigningKeyId().empty()))
        return;
      protobuf::StoreRefreshRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        std::string message =
            boost::lexical_cast<std::string>(message_type) + payload;
        std::string public_key_id, public_key, other_info;
        if (request.sender().has_public_key_id())
          public_key_id = request.sender().public_key_id();
        if (request.sender().has_public_key())
          public_key = request.sender().public_key();
        if (request.sender().has_other_info())
          other_info = request.sender().other_info();
        securifier_->GetPublicKeyAndValidation(public_key_id, &public_key,
                                               &other_info);
        if (!securifier_->Validate(message, message_signature, public_key_id,
                                   public_key, other_info,
                                   request.sender().node_id()))
          return;
        protobuf::StoreRefreshResponse response;
        (*on_store_refresh_request_)(info, request, &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kStoreRefreshResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::StoreRefreshResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_store_refresh_response_)(info, response);
      break;
    }
    case kDeleteRequest: {
      if ((security_type != (kSign | kAsymmetricEncrypt)) ||
          (message_signature.empty() && !securifier_->kSigningKeyId().empty()))
        return;
      protobuf::DeleteRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        if (!request.sender().has_node_id())
          return;
        std::string message =
            boost::lexical_cast<std::string>(message_type) + payload;
        std::string public_key_id, public_key, other_info;
        if (request.sender().has_public_key_id())
          public_key_id = request.sender().public_key_id();
        if (request.sender().has_public_key())
          public_key = request.sender().public_key();
        if (request.sender().has_other_info())
          other_info = request.sender().other_info();
        securifier_->GetPublicKeyAndValidation(public_key_id, &public_key,
                                               &other_info);
        if (!securifier_->Validate(message, message_signature, public_key_id,
                                   public_key, other_info,
                                   request.sender().node_id()))
          return;
        protobuf::DeleteResponse response;
        (*on_delete_request_)(info, request, payload, message_signature,
                              &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kDeleteResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::DeleteResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_delete_response_)(info, response);
      break;
    }
    case kDeleteRefreshRequest: {
      if ((security_type != (kSign | kAsymmetricEncrypt)) ||
          (message_signature.empty() && !securifier_->kSigningKeyId().empty()))
        return;
      protobuf::DeleteRefreshRequest request;
      if (request.ParseFromString(payload) && request.IsInitialized()) {
        std::string message =
            boost::lexical_cast<std::string>(message_type) + payload;
        std::string public_key_id, public_key, other_info;
        if (request.sender().has_public_key_id())
          public_key_id = request.sender().public_key_id();
        if (request.sender().has_public_key())
          public_key = request.sender().public_key();
        if (request.sender().has_other_info())
          other_info = request.sender().other_info();
        securifier_->GetPublicKeyAndValidation(public_key_id, &public_key,
                                               &other_info);
        if (!securifier_->Validate(message, message_signature, public_key_id,
                                   public_key, other_info,
                                   request.sender().node_id()))
          return;
        protobuf::DeleteRefreshResponse response;
        (*on_delete_refresh_request_)(info, request, &response, timeout);
        *message_response = WrapMessage(response,
                                        request.sender().public_key());
      }
      break;
    }
    case kDeleteRefreshResponse: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::DeleteRefreshResponse response;
      if (response.ParseFromString(payload) && response.IsInitialized())
        (*on_delete_refresh_response_)(info, response);
      break;
    }
    case kDownlistNotification: {
      if (security_type != kAsymmetricEncrypt)
        return;
      protobuf::DownlistNotification request;
      if (request.ParseFromString(payload) && request.IsInitialized())
        (*on_downlist_notification_)(info, request, timeout);
      break;
    }
    default:
      process_serialised_message_(message_type, payload, security_type,
                                  message_signature, info, message_response,
                                  timeout);
//       transport::MessageHandler::ProcessSerialisedMessage(message_type,
//                                                           payload,
//                                                           security_type,
//                                                           message_signature,
//                                                           info,
//                                                           message_response,
//                                                           timeout);
  }
}

std::string MessageHandler::MakeSerialisedWrapperMessage(
    const int &message_type,
    const std::string &payload,
    SecurityType security_type,
    const std::string &recipient_public_key) {
  // If we asked for security but provided no securifier, fail.
  if (security_type && !securifier_) {
    DLOG(ERROR) << "MakeSerialisedWrapperMessage - type " << message_type
                << " - No securifier provided.";
    return "";
  }

  // Handle signing
  std::string signature;
  if (security_type & kSign) {
    signature = securifier_->Sign(
        boost::lexical_cast<std::string>(message_type) + payload);
  } else if (security_type & kSignWithParameters) {
    signature = securifier_->SignWithParameters(
        boost::lexical_cast<std::string>(message_type) + payload);
  }
  std::string serialised_message(WrapWrapperMessage(message_type,
                                                    payload,
                                                    signature));
  // Handle encryption
  std::string final_message(1, security_type);
  if (security_type & kAsymmetricEncrypt) {
    if (recipient_public_key.empty()) {
      DLOG(ERROR) << "MakeSerialisedWrapperMessage - type " << message_type
                  << " - No public key for receiver provided.";
      return "";
    }
    std::string seed = RandomString(48);
    std::string key = seed.substr(0, 32);
    std::string kIV = seed.substr(32, 16);
    std::string encrypt_message =
        crypto::SymmEncrypt(serialised_message, key, kIV);
    std::string encrypt_aes_seed =
        securifier_->AsymmetricEncrypt(seed, recipient_public_key);
    final_message += encrypt_aes_seed + encrypt_message;
  } else {
    final_message += serialised_message;
  }
  return final_message;
}

}  // namespace dht

}  // namespace maidsafe
