
#ifndef MESSAGE_CHANNEL_H_
#define MESSAGE_CHANNEL_H_

#include <proto/cpp/chain.pb.h>
#include <proto/cpp/overlay.pb.h>
#include <common/network.h>
#include <monitor/system_manager.h>

namespace bubi {
	class IMessageHandler{
	public:
		virtual void HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message) = 0;
	};

	static const std::string static_notary_unique_ = "static_notary_unique";

	class ChannelParameter{
	public:
		ChannelParameter(){
			comm_unique_ = "";
			inbound_ = false;
		}
		~ChannelParameter(){}

		std::string comm_unique_;
		utils::InetAddress listen_addr_;
		utils::InetAddress target_addr_;
		bool inbound_;
	};

	class MessageChannelPeer :public Connection{
	public:
		MessageChannelPeer(server *server_h, client *client_h, tls_server *tls_server_h, tls_client *tls_client_h, connection_hdl con, const std::string &uri, int64_t id);
		~MessageChannelPeer();

		bool IsActive() const;
		int64_t GetActiveTime() const;
		std::string GePeerUnique() const;

		void SetPeerInfo(const protocol::CrossHello &hello);
		void SetActiveTime(int64_t current_time);
		bool SendHello(const std::string &comm_unique, std::error_code &ec);

		virtual bool OnNetworkTimer(int64_t current_time);

	private:
		//Peer infomation
		int64_t active_time_;
		std::string comm_unique_;
	};

	class MessageChannel : public TimerNotify,
		public Network,
		public utils::Runnable {
		friend class utils::Singleton<bubi::MessageChannel>;
	public:
		MessageChannel();
		~MessageChannel();

		bool Initialize(const ChannelParameter &param);
		bool Exit();
		void SendRequest(const std::string &comm_unique, int64_t type, const std::string &data);
		void SendResponse(const std::string &comm_unique, const protocol::WsMessage &req_message, const std::string &data);
		void Register(IMessageHandler *handler, int64_t msg_type);
		void UnRegister(IMessageHandler *handler, int64_t msg_type);

	protected:
		virtual void Run(utils::Thread *thread) override;

	private:
		//handel message
		bool OnHandleHello(const protocol::WsMessage &message, int64_t conn_id);
		bool OnHandleHelloResponse(const protocol::WsMessage &message, int64_t conn_id);
		bool OnHandleApplyMessage(const protocol::WsMessage &message, int64_t conn_id);

		//network handlers
		virtual void OnDisconnect(Connection *conn);
		virtual bool OnConnectOpen(Connection *conn);
		virtual Connection *CreateConnectObject(server *server_h, client *client_,
			tls_server *tls_server_h, tls_client *tls_client_h,
			connection_hdl con, const std::string &uri, int64_t id);

		//override
		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) override {};

		//internal call
		void ProcessMessageChannelDisconnect();
		void Notify(const std::string &comm_unique, const protocol::WsMessage &message);
		std::string GePeerUnique(int64_t conn_id);
	
	private:
		utils::Thread *thread_ptr_;
		std::error_code last_ec_;
		int64_t last_uptate_time_;
		std::map<int64_t, IMessageHandler*> listener_map_;
		utils::Mutex listener_map_lock_;
		ChannelParameter param_;
	};
}
#endif

