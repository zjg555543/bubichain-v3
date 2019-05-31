
#include <utils/headers.h>
#include <common/general.h>
#include <proto/cpp/monitor.pb.h>
#include <overlay/peer_manager.h>
#include <glue/glue_manager.h>
#include <ledger/ledger_manager.h>
#include <monitor/monitor.h>
#include <cross/message_channel.h>

namespace bubi {

	MessageChannelPeer::MessageChannelPeer(server *server_h, client *client_h, tls_server *tls_server_h, tls_client *tls_client_h, connection_hdl con, const std::string &uri, int64_t id) :
		Connection(server_h, client_h, tls_server_h, tls_client_h, con, uri, id) {
		active_time_ = 0;
	}

	MessageChannelPeer::~MessageChannelPeer(){
	}

	int64_t MessageChannelPeer::GetActiveTime() const {
		return active_time_;
	}

	bool MessageChannelPeer::IsActive() const {
		return active_time_ > 0;
	}

	void MessageChannelPeer::SetPeerInfo(const protocol::CrossHello &hello) {
		comm_unique_ = hello.comm_unique();
	}

	void MessageChannelPeer::SetActiveTime(int64_t current_time) {
		active_time_ = current_time;
	}

	bool MessageChannelPeer::SendHello(const std::string &comm_unique, std::error_code &ec) {
		protocol::CrossHello hello;
		hello.set_comm_unique(comm_unique);
		return SendRequest(protocol::CROSS_MSGTYPE_HELLO, hello.SerializeAsString(), ec);
	}

	std::string MessageChannelPeer::GetChainUnique() const{
		return comm_unique_;
	}

	bool MessageChannelPeer::OnNetworkTimer(int64_t current_time) {
		if (!IsActive() && current_time - connect_start_time_ > 10 * utils::MICRO_UNITS_PER_SEC) {
			LOG_ERROR("Failed to check peer active, (%s) timeout", GetPeerAddress().ToIpPort().c_str());
			return false;
		}

		return true;
	}

	MessageChannel::MessageChannel() : Network(SslParameter()) {
		last_uptate_time_ = utils::Timestamp::HighResolution();

		request_methods_[protocol::CROSS_MSGTYPE_HELLO] = std::bind(&MessageChannel::OnHandleHello, this, std::placeholders::_1, std::placeholders::_2);
		request_methods_[protocol::CROSS_MSGTYPE_PROPOSAL] = std::bind(&MessageChannel::OnHandleProposal, this, std::placeholders::_1, std::placeholders::_2);

		response_methods_[protocol::CROSS_MSGTYPE_HELLO] = std::bind(&MessageChannel::OnHandleHelloResponse, this, std::placeholders::_1, std::placeholders::_2);
		response_methods_[protocol::CROSS_MSGTYPE_PROPOSAL] = std::bind(&MessageChannel::OnHandleProposalResponse, this, std::placeholders::_1, std::placeholders::_2);
		
		thread_ptr_ = NULL;
	}

	MessageChannel::~MessageChannel() {
		if (thread_ptr_){
			delete thread_ptr_;
		}
	}

	bool MessageChannel::Initialize(const ChannelParameter &param) {
		param_ = param;

		thread_ptr_ = new utils::Thread(this);
		if (!thread_ptr_->Start("messageChannel")) {
			return false;
		}

		TimerNotify::RegisterModule(this);
		LOG_INFO("Initialized message channel server successfully");
		return true;
	}

	bool MessageChannel::Exit() {
		Stop();
		thread_ptr_->JoinWithStop();
		return true;
	}

	void MessageChannel::Run(utils::Thread *thread) {
		if (!param_.inbound_){
			Start(param_.notary_addr_);
		}
		else{
			utils::InetAddress ip;
			Start(ip);
		}
	}

	bool MessageChannel::OnHandleHello(const protocol::WsMessage &message, int64_t conn_id){
		protocol::CrossHello hello;
		hello.ParseFromString(message.data());

		protocol::CrossHelloResponse hello_response;
		std::error_code ignore_ec;

		utils::MutexGuard guard_(conns_list_lock_);
		Connection *conn = GetConnection(conn_id);
		do{
			if (!conn) {
				LOG_ERROR("MessageChannelPeer conn pointer is empty");
				return false;
			}

			MessageChannelPeer *peer = (MessageChannelPeer *)conn;
			peer->SetPeerInfo(hello);
			hello_response.set_error_code(protocol::ERRCODE_SUCCESS);
			LOG_INFO("Received a hello message, peer(%s) is active", conn->GetPeerAddress().ToIpPort().c_str());
			peer->SetActiveTime(utils::Timestamp::HighResolution());

			if (peer->InBound()) {
				peer->SendHello(param_.comm_unique_, last_ec_);
			}

		} while (false);

		conn->SendResponse(message, hello_response.SerializeAsString(), ignore_ec);
		return hello_response.error_code() == 0;
	}

	bool MessageChannel::OnHandleHelloResponse(const protocol::WsMessage &message, int64_t conn_id){
		utils::MutexGuard guard(conns_list_lock_);
		MessageChannelPeer *peer = (MessageChannelPeer *)GetConnection(conn_id);
		if (!peer) {
			return true;
		}

		protocol::CrossHelloResponse hello_response;
		hello_response.ParseFromString(message.data());
		if (hello_response.error_code() != 0) {
			LOG_ERROR("Failed to response the MessageChannelPeer hello message.MessageChannelPeer reponse error code(%d), desc(%s)", 
				hello_response.error_code(), hello_response.error_desc().c_str());
			return false;
		}

		return true;
	}

	bool MessageChannel::OnHandleProposal(const protocol::WsMessage &message, int64_t conn_id){
		std::string comm_unique = GetChainUnique(conn_id);
		if (comm_unique.empty()){
			LOG_ERROR("Failed to handle proposal, chain unique is empty.");
			return false;
		}

		Notify(comm_unique, message);
		return true;
	}

	bool MessageChannel::OnHandleProposalResponse(const protocol::WsMessage &message, int64_t conn_id){
		std::string comm_unique = GetChainUnique(conn_id);
		if (comm_unique.empty()){
			LOG_ERROR("Failed to handle proposal, chain unique is empty.");
			return false;
		}
		Notify(comm_unique, message);
		return true;
	}

	void MessageChannel::SendRequest(const std::string &comm_unique, int64_t type, const std::string &data) {
		utils::MutexGuard guard(conns_list_lock_);
		for (auto iter = connections_.begin(); iter != connections_.end(); iter++) {
			MessageChannelPeer *messageChannel = (MessageChannelPeer *)iter->second;
			if (!comm_unique.empty() && messageChannel->GetChainUnique() != comm_unique){
				continue;
			}
			std::error_code ec;
			messageChannel->SendRequest(type, data, ec);
		}
	}

	void MessageChannel::SendResponse(const std::string &comm_unique, const protocol::WsMessage &req_message, const std::string &data){
		utils::MutexGuard guard(conns_list_lock_);
		for (auto iter = connections_.begin(); iter != connections_.end(); iter++) {
			MessageChannelPeer *messageChannel = (MessageChannelPeer *)iter->second;
			if (!comm_unique.empty() && messageChannel->GetChainUnique() != comm_unique){
				continue;
			}
			std::error_code ec;
			messageChannel->SendResponse(req_message, data, ec);
			break;
		}
	}

	bool MessageChannel::OnConnectOpen(Connection *conn) {
		if (!conn->InBound()) {
			MessageChannelPeer *peer = (MessageChannelPeer *)conn;
			peer->SendHello(param_.comm_unique_, last_ec_);
		}
		return true;
	}


	void MessageChannel::OnDisconnect(Connection *conn) {
		MessageChannelPeer *peer = (MessageChannelPeer *)conn;
		LOG_INFO("The MessageChannelPeer has been disconnected, node address is (%s)", peer->GetPeerAddress().ToIpPort().c_str());
	}

	Connection *MessageChannel::CreateConnectObject(server *server_h, client *client_,
		tls_server *tls_server_h, tls_client *tls_client_h,
		connection_hdl con, const std::string &uri, int64_t id) {
		return new MessageChannelPeer(server_h, client_, tls_server_h, tls_client_h, con, uri, id);
	}

	void MessageChannel::ProcessMessageChannelDisconnect(){
		if (param_.inbound_){
			return;
		}

		utils::MutexGuard guard(conns_list_lock_);
		ConnectionMap::const_iterator iter;
		utils::StringList ip_port_list;
		iter = connections_.begin();
		while (iter != connections_.end()) {
			ip_port_list.push_back(iter->second->GetPeerAddress().ToIpPort().c_str());
			iter++;
		}
		std::string address = param_.notary_addr_.ToIpPort().c_str();
		utils::StringList::const_iterator itr;
		itr = std::find(ip_port_list.begin(), ip_port_list.end(), address.c_str());
		if (itr == ip_port_list.end()){
			std::string uri = utils::String::Format("%s://%s", ssl_parameter_.enable_ ? "wss" : "ws", address.c_str());
			Connect(uri);
		}
	}

	void MessageChannel::OnTimer(int64_t current_time){
		if (current_time > 10 * utils::MICRO_UNITS_PER_SEC + last_uptate_time_){
			ProcessMessageChannelDisconnect();
			last_uptate_time_ = current_time;
		}
	}

	void MessageChannel::Register(IMessageHandler *handler, int64_t msg_type){
		utils::MutexGuard guard(listener_map_lock_);
		listener_map_[msg_type] = handler;
	}

	void MessageChannel::UnRegister(IMessageHandler *handler, int64_t msg_type){
		utils::MutexGuard guard(listener_map_lock_);

		std::map<int64_t, IMessageHandler*>::iterator itr = listener_map_.find(msg_type);
		if (itr != listener_map_.end()){
			listener_map_.erase(itr);
		}
	}

	void MessageChannel::Notify(const std::string &comm_unique, const protocol::WsMessage &message){
		utils::MutexGuard guard(listener_map_lock_);
		std::map<int64_t, IMessageHandler*>::iterator itr = listener_map_.find(message.type());
		if (itr != listener_map_.end()){
			itr->second->HandleMessage(comm_unique, message);
		}
	}

	std::string MessageChannel::GetChainUnique(int64_t conn_id){
		Connection *conn = GetConnection(conn_id);
		if (!conn) {
			LOG_ERROR("MessageChannelPeer conn pointer is empty");
			return "";
		}

		MessageChannelPeer *peer = (MessageChannelPeer *)conn;
		return peer->GetChainUnique();
	}
}

