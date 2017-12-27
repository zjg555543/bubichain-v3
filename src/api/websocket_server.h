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

#ifndef WEBSOCKET_SERVER_H_
#define WEBSOCKET_SERVER_H_

#include <proto/cpp/chain.pb.h>
#include <proto/cpp/overlay.pb.h>
#include <common/network.h>
#include <monitor/system_manager.h>
#include <main/configure.h>

namespace bubi {
	class ContractLogMessage : public utils::Runnable {
	public:
		ContractLogMessage();
		~ContractLogMessage();

		bool Initialize();
		bool Exit();

		bool PullLog(const protocol::ChainContractLog &message);
	protected:
		virtual void Run(utils::Thread *thread) override;

	public:
		utils::Thread *thread_ptr_;

		const uint32_t list_limit_;
		utils::Mutex ws_send_message_list_mutex_;
		std::list<protocol::ChainContractLog> ws_contract_log_list;
	};

	class WebSocketServer :public utils::Singleton<WebSocketServer>,
		public StatusModule,
		public Network,
		public utils::Runnable {
		friend class utils::Singleton<bubi::WebSocketServer>;
	public:
		WebSocketServer();
		~WebSocketServer();

		bool Initialize(WsServerConfigure &ws_server_configure);
		bool Exit();

		// Handlers
		bool OnChainHello(protocol::WsMessage &message, int64_t conn_id);
		bool OnChainPeerMessage(protocol::WsMessage &message, int64_t conn_id);
		bool OnSubmitTransaction(protocol::WsMessage &message, int64_t conn_id);

		void BroadcastMsg(int64_t type, const std::string &data);
		void BroadcastChainTxMsg(const std::string &hash, const std::string &source_address, Result result, protocol::ChainTxStatus_TxStatus status);

		bool SendContractLog(const char* sender, const char* data, uint64_t data_size);

		virtual void GetModuleStatus(Json::Value &data);
	protected:
		virtual void Run(utils::Thread *thread) override;

	private:
		utils::Thread *thread_ptr_;

		uint64_t last_connect_time_;
		uint64_t connect_interval_;
		
		const uint64_t log_size_limit_;
		ContractLogMessage log_message_;
	};
}

#endif