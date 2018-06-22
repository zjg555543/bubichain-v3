
#ifndef WEBSOCKET_SERVER_H_
#define WEBSOCKET_SERVER_H_

#include <proto/cpp/chain.pb.h>
#include <proto/cpp/overlay.pb.h>
#include <common/network.h>
#include <monitor/system_manager.h>
#include <main/configure.h>

namespace bubi {
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

		virtual void GetModuleStatus(Json::Value &data);
	protected:
		virtual void Run(utils::Thread *thread) override;

	private:
		utils::Thread *thread_ptr_;

		uint64_t last_connect_time_;
		uint64_t connect_interval_;
		
		const uint64_t log_size_limit_;
	};
}

#endif
