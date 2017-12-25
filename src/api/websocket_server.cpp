/*
Copyright Bubi Technologies Co., Ltd. 2017 All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <utils/headers.h>
#include <common/general.h>
#include <main/configure.h>
#include <proto/cpp/monitor.pb.h>
#include <overlay/peer_manager.h>
#include <glue/glue_manager.h>
#include <ledger/ledger_manager.h>
#include <monitor/monitor.h>

#include "websocket_server.h"

namespace bubi {
	ContractLogMessage::ContractLogMessage() : list_limit_(1000), thread_ptr_(NULL){
	}

	ContractLogMessage::~ContractLogMessage() {

	}

	bool ContractLogMessage::Initialize() {
		ws_contract_log_list.clear();
		thread_ptr_ = new utils::Thread(this);
		if (!thread_ptr_->Start("wsmessage")) {
			return false;
		}
		return true;
	}

	void ContractLogMessage::Run(utils::Thread *thread) {
		WebSocketServer& websocket_server = WebSocketServer::Instance();
		while (thread_ptr_->enabled()) {
			bool is_empty = false;
			do {
				utils::MutexGuard guard(ws_send_message_list_mutex_);
				if (ws_contract_log_list.empty()) {
					is_empty = true;
					break;
				}

				protocol::ContractLog socket_message = ws_contract_log_list.front();
				websocket_server.BroadcastMsg(protocol::ChainMessageType::CHAIN_CONTRACT_LOG, socket_message.SerializeAsString());
				ws_contract_log_list.pop_front();
			} while (false);

			if (is_empty) {
				utils::Sleep(3 * utils::MILLI_UNITS_PER_SEC);
			}
			else {
				utils::Sleep(10);
			}
		}
	}

	bool ContractLogMessage::PullLog(const protocol::ContractLog& message) {
		utils::MutexGuard guard(ws_send_message_list_mutex_);
		if (ws_contract_log_list.size() >= list_limit_) {
			LOG_ERROR("the queue of web socket is full, sender(%s) log(%s)", message.sender().c_str(), message.data().c_str());
			return false;
		}
		ws_contract_log_list.push_back(message);
		return true;
	}

	bool ContractLogMessage::Exit() {
		if (thread_ptr_){
			delete thread_ptr_;
			thread_ptr_ = NULL;
		}
		utils::MutexGuard guard(ws_send_message_list_mutex_);
		ws_contract_log_list.clear();
		return false;
	}

	WebSocketServer::WebSocketServer() : Network(SslParameter()), log_size_limit_(64 * 1024){
		connect_interval_ = 120 * utils::MICRO_UNITS_PER_SEC;
		last_connect_time_ = 0;

		request_methods_[protocol::CHAIN_HELLO] = std::bind(&WebSocketServer::OnChainHello, this, std::placeholders::_1, std::placeholders::_2);
		request_methods_[protocol::CHAIN_PEER_MESSAGE] = std::bind(&WebSocketServer::OnChainPeerMessage, this, std::placeholders::_1, std::placeholders::_2);
		request_methods_[protocol::CHAIN_SUBMITTRANSACTION] = std::bind(&WebSocketServer::OnSubmitTransaction, this, std::placeholders::_1, std::placeholders::_2);

		thread_ptr_ = NULL;
	}

	WebSocketServer::~WebSocketServer() {
		if (thread_ptr_){
			delete thread_ptr_;
			thread_ptr_ = NULL;
		} 
	}

	bool WebSocketServer::Initialize(WsServerConfigure &ws_server_configure) {
		thread_ptr_ = new utils::Thread(this);
		if (!thread_ptr_->Start("websocket")) {
			return false;
		}
		StatusModule::RegisterModule(this);
		log_message_.Initialize();
		LOG_INFO("Websocket server initialized");
		return true;
	}

	bool WebSocketServer::Exit() {
		Stop();
		thread_ptr_->JoinWithStop();
		return true;
	}

	void WebSocketServer::Run(utils::Thread *thread) {
		Start(bubi::Configure::Instance().wsserver_configure_.listen_address_);
	}

	bool WebSocketServer::OnChainHello(protocol::WsMessage &message, int64_t conn_id) {
		protocol::ChainStatus cmsg;
		cmsg.set_bubi_version(General::BUBI_VERSION);
		cmsg.set_monitor_version(General::MONITOR_VERSION);
		cmsg.set_ledger_version(General::LEDGER_VERSION);
		cmsg.set_self_addr(PeerManager::Instance().GetPeerNodeAddress());
		cmsg.set_timestamp(utils::Timestamp::HighResolution());
		std::error_code ignore_ec;

		utils::MutexGuard guard_(conns_list_lock_);
		Connection *conn = GetConnection(conn_id);
		if (conn) {
			conn->SendResponse(message, cmsg.SerializeAsString(), ignore_ec);
			LOG_INFO("Recv chain hello from ip(%s), send response result(%d:%s)", conn->GetPeerAddress().ToIpPort().c_str(),
				ignore_ec.value(), ignore_ec.message().c_str());
		}
		return true;
	}

	bool WebSocketServer::OnChainPeerMessage(protocol::WsMessage &message, int64_t conn_id) {
		// send peer
		utils::MutexGuard guard_(conns_list_lock_);
		Connection *conn = GetConnection(conn_id);
		if (!conn) {
			return false;
		}

		LOG_INFO("Recv chain peer message from ip(%s)", conn->GetPeerAddress().ToIpPort().c_str());
		protocol::ChainPeerMessage cpm;
		if (!cpm.ParseFromString(message.data())) {
			LOG_ERROR("ChainPeerMessage FromString fail");
			return true;
		}

		//bubi::PeerManager::Instance().BroadcastPayLoad(cpm);
		return true;
	}

	void WebSocketServer::BroadcastMsg(int64_t type, const std::string &data) {
		utils::MutexGuard guard(conns_list_lock_);

		for (ConnectionMap::iterator iter = connections_.begin();
			iter != connections_.end();
			iter++) {
			std::error_code ec;
			iter->second->SendRequest(type, data, ec);
		}
	}


	void WebSocketServer::BroadcastChainTxMsg(const std::string &hash, const std::string &source_address, Result result, protocol::ChainTxStatus_TxStatus status) {
		protocol::ChainTxStatus cts;
		cts.set_tx_hash(utils::String::BinToHexString(hash));
		cts.set_source_address(source_address);
		cts.set_error_code((protocol::ERRORCODE)result.code());
		cts.set_error_desc(result.desc());
		cts.set_status(status);
		cts.set_timestamp(utils::Timestamp::Now().timestamp());
		std::string str = cts.SerializeAsString();
		bubi::WebSocketServer::Instance().BroadcastMsg(protocol::CHAIN_TX_STATUS, str);
	}

	bool WebSocketServer::OnSubmitTransaction(protocol::WsMessage &message, int64_t conn_id) {
		utils::MutexGuard guard_(conns_list_lock_);
		Connection *conn = GetConnection(conn_id);
		if (!conn) {
			return false;
		}

		Result result;
		protocol::TransactionEnv tran_env;
		do {
			if (!tran_env.ParseFromString(message.data())) {
				LOG_ERROR("Parse submit transaction string fail, ip(%s)", conn->GetPeerAddress().ToIpPort().c_str());
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc("Parse the transaction failed");
				break;
			}
			Json::Value real_json;
			real_json = Proto2Json(tran_env);
			printf("%s",real_json.toStyledString().c_str());
			std::string content = tran_env.transaction().SerializeAsString();

			// add node signature
			PrivateKey privateKey(bubi::Configure::Instance().p2p_configure_.node_private_key_);
			if (!privateKey.IsValid()) {
				result.set_code(protocol::ERRCODE_INVALID_PRIKEY);
				result.set_desc("Node signature failed");
				LOG_ERROR("Node private key is invalid");
				break;
			}
			std::string sign = privateKey.Sign(content);
			protocol::Signature *signpro = tran_env.add_signatures();
			signpro->set_sign_data(sign);
			signpro->set_public_key(privateKey.GetBase16PublicKey());

			TransactionFrm::pointer ptr = std::make_shared<TransactionFrm>(tran_env);
			GlueManager::Instance().OnTransaction(ptr, result);
			PeerManager::Instance().Broadcast(protocol::OVERLAY_MSGTYPE_TRANSACTION, tran_env.SerializeAsString());
		
		} while (false);

		//notice WebSocketServer Tx status
		std::string hash = HashWrapper::Crypto(tran_env.transaction().SerializeAsString());
		protocol::ChainTxStatus cts;
		cts.set_tx_hash(utils::String::BinToHexString(hash));
		cts.set_error_code((protocol::ERRORCODE)result.code());
		cts.set_source_address(tran_env.transaction().source_address());
		cts.set_status(result.code() == protocol::ERRCODE_SUCCESS ? protocol::ChainTxStatus_TxStatus_CONFIRMED : protocol::ChainTxStatus_TxStatus_FAILURE);
		cts.set_error_desc(result.desc());
		cts.set_timestamp(utils::Timestamp::Now().timestamp());
		std::string str = cts.SerializeAsString();
			
		BroadcastMsg(protocol::CHAIN_TX_STATUS, str);
		
		return true;
	}

	bool WebSocketServer::SendContractLog(const char* sender, const char* data, uint64_t data_size) {
		if (data_size > log_size_limit_) {
			LOG_ERROR("log's size (%lld) is too big, sender(%s) log(%s)", data_size, sender, data);
			return false;
		}
		protocol::ContractLog contract_log;
		contract_log.set_sender(sender);
		contract_log.set_data(data);
		contract_log.set_timestamp(utils::Timestamp::HighResolution());
		return log_message_.PullLog(contract_log);
	}

	void WebSocketServer::GetModuleStatus(Json::Value &data) {
		data["name"] = "websocket_server";
		Json::Value &peers = data["clients"];
		int32_t active_size = 0;
		utils::MutexGuard guard(conns_list_lock_);
		for (auto &item : connections_) {
			item.second->ToJson(peers[peers.size()]);
		}
	}
}