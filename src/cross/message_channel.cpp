
#include <utils/headers.h>
#include <common/general.h>
#include <proto/cpp/monitor.pb.h>
#include <overlay/peer_manager.h>
#include <glue/glue_manager.h>
#include <ledger/ledger_manager.h>
#include <monitor/monitor.h>

#include "message_channel.h"
using namespace std;

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
		chain_unique_ = hello.chain_unique();
	}

	void MessageChannelPeer::SetActiveTime(int64_t current_time) {
		active_time_ = current_time;
	}

	bool MessageChannelPeer::SendHello(const std::string &chain_unique, std::error_code &ec) {
		protocol::CrossHello hello;
		hello.set_chain_unique(chain_unique);
		return SendRequest(protocol::CROSS_MSGTYPE_HELLO, hello.SerializeAsString(), ec);
	}

	std::string MessageChannelPeer::GetChainUnique() const{
		return chain_unique_;
	}

	bool MessageChannelPeer::OnNetworkTimer(int64_t current_time) {
		if (!IsActive() && current_time - connect_start_time_ > 10 * utils::MICRO_UNITS_PER_SEC) {
			LOG_ERROR("Failed to check peer active, (%s) timeout", GetPeerAddress().ToIpPort().c_str());
			return false;
		}

		return true;
	}

	MessageChannel::MessageChannel() : Network(SslParameter()) {
		connect_interval_ = 120 * utils::MICRO_UNITS_PER_SEC;
		last_connect_time_ = 0;
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

		const P2pConfigure &p2p_configure = Configure::Instance().p2p_configure_;
		network_id_ = p2p_configure.network_id_;

		StatusModule::RegisterModule(this);
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
		if (param_.inbound_){
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
			std::string error_desc_temp = utils::String::Format("Received a message channel hello message from ip(%s), and sent the response result(%d:%s)",
				conn->GetPeerAddress().ToIpPort().c_str(), ignore_ec.value(), ignore_ec.message().c_str());
			hello_response.set_error_desc(error_desc_temp.c_str());
			LOG_INFO("Received a message channel hello message from ip(%s), and sent the response result(%d:%s)", conn->GetPeerAddress().ToIpPort().c_str(),
				ignore_ec.value(), ignore_ec.message().c_str());

			LOG_INFO("Received a hello message, peer(%s) is active", conn->GetPeerAddress().ToIpPort().c_str());
			peer->SetActiveTime(utils::Timestamp::HighResolution());

			if (peer->InBound()) {
				//utils::InetAddress address("127.0.0.1");
				peer->SendHello(param_.chain_unique_, last_ec_);
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
		std::string chain_unique = GetChainUnique(conn_id);
		if (chain_unique.empty()){
			LOG_ERROR("Failed to handle proposal, chain unique is empty.");
			return false;
		}

		Notify(chain_unique, protocol::CROSS_MSGTYPE_PROPOSAL, true, message.data());
		return true;
	}

	bool MessageChannel::OnHandleProposalResponse(const protocol::WsMessage &message, int64_t conn_id){
		std::string chain_unique = GetChainUnique(conn_id);
		if (chain_unique.empty()){
			LOG_ERROR("Failed to handle proposal, chain unique is empty.");
			return false;
		}
		Notify(chain_unique, protocol::CROSS_MSGTYPE_PROPOSAL, false, message.data());
		return true;
	}

	void MessageChannel::SendMessage(int64_t type, const std::string &data) {
		utils::MutexGuard guard(conns_list_lock_);
		for (auto iter = connections_.begin(); iter != connections_.end(); iter++) {
			MessageChannelPeer *messageChannel = (MessageChannelPeer *)iter->second;
			std::error_code ec;
			messageChannel->SendRequest(type, data, ec);
		}
	}

	void MessageChannel::GetModuleStatus(Json::Value &data) {
		data["name"] = "message_channel";
		Json::Value &peers = data["clients"];
		int32_t active_size = 0;
		utils::MutexGuard guard(conns_list_lock_);
		for (auto &item : connections_) {
			item.second->ToJson(peers[peers.size()]);
		}
	}

	bool MessageChannel::OnConnectOpen(Connection *conn) {
		if (!conn->InBound()) {
			MessageChannelPeer *peer = (MessageChannelPeer *)conn;
			peer->SendHello(param_.chain_unique_, last_ec_);
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

		//const MessageChannelConfigure &message_channel_configure = Configure::Instance().message_channel_configure_;
		utils::MutexGuard guard(conns_list_lock_);
		ConnectionMap::const_iterator iter;
		utils::StringList listTempIptoPort;
		iter = connections_.begin();
		while (iter != connections_.end()) {
			listTempIptoPort.push_back(iter->second->GetPeerAddress().ToIpPort().c_str());
			iter++;
		}

		std::string address = "127.0.0.1";// message_channel_configure.target_message_channel_.ToIpPort().c_str();

		utils::StringList::const_iterator listTempIptoPortiter;
		listTempIptoPortiter = std::find(listTempIptoPort.begin(), listTempIptoPort.end(), address.c_str());

		if (listTempIptoPortiter == listTempIptoPort.end()){
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

	void MessageChannel::Notify(const std::string &chain_unique, int64_t type, bool request, const std::string &data){
		utils::MutexGuard guard(listener_map_lock_);
		std::map<int64_t, IMessageHandler*>::iterator itr = listener_map_.find(type);
		if (itr != listener_map_.end()){
			itr->second->HandleMessage(chain_unique, type, request, data);
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

