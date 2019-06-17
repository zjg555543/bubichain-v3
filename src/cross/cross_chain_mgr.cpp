#include <common/storage.h>
#include <common/private_key.h>
#include <common/pb2json.h>
#include <api/websocket_server.h>
#include <glue/glue_manager.h>
#include <overlay/peer_manager.h>
#include "cross_chain_mgr.h"

namespace bubi {
	MessageHandler::MessageHandler(MessageChannel *channel, const std::string &comm_contract){
		channel_ = channel;
		comm_contract_ = comm_contract;
	}

	void MessageHandler::OnHandleProposal(const protocol::WsMessage &message){
		protocol::CrossProposal cross_proposal;
		cross_proposal.ParseFromString(message.data());
		protocol::CrossProposalResponse cross_proposal_response;

		LOG_INFO("Recvd proposal for request, type:%d, id:(" FMT_I64 " ).", cross_proposal.type(), cross_proposal.proposal_id());

		//查找通讯合约
		AccountFrm::pointer acc = NULL;
		if (!Environment::AccountFromDB(comm_contract_, acc)) {
			LOG_INFO("GetAccount fail, account(%s) not exist", comm_contract_.c_str());
			return;
		}

		if (cross_proposal.type() == protocol::CROSS_PROPOSAL_INPUT){
			int64_t id = cross_proposal.proposal_id();
			if (id <= 0){
				protocol::KeyPair value_ptr;
				if (!acc->GetMetaData("receive_relay", value_ptr)){
					//尚未找到合约相关信息
					LOG_ERROR("Get metadata fail, key receive_relay not exist");
					return;
				}

				Json::Value data;
				data.fromString(value_ptr.value());
				id = data["init_seq"].asInt64();
			}

			protocol::KeyPair recv_key_pair;
			std::string key = utils::String::Format("receive_proposal_" FMT_I64 "", id);
			if (!acc->GetMetaData(key, recv_key_pair)){
				//尚未找到合约相关信息
				LOG_INFO("Get metadata fail, key(%s) not exist", key.c_str());
				return;
			}

			//设置提案信息
			Json::Value data;
			data.fromString(recv_key_pair.value());
			const Json::Value &proposals_obj = data["proposals"];
			protocol::CrossProposalInfo info;
			info.set_type(cross_proposal.type());
			info.set_proposal_id(id);
			info.set_proposal_body(proposals_obj.toFastString());
			info.set_status(data["state"].asInt());
			for (size_t i = 0; i < proposals_obj.size(); i++) {
				*info.add_confirmed_notarys() = proposals_obj[i][Json::UInt(0)].asString();
			}

			LOG_INFO("Recvd proposal for request, status:%d, id:(" FMT_I64 " ).", data["status"].asInt(), id);
			*cross_proposal_response.mutable_proposal_info() = info;
		}
		else if (cross_proposal.type() == protocol::CROSS_PROPOSAL_OUTPUT){
			int64_t id = cross_proposal.proposal_id();
			if (id <= 0){
				protocol::KeyPair value_ptr;
				if (!acc->GetMetaData("send_relay", value_ptr)){
					//尚未找到合约相关信息
					LOG_ERROR("Get metadata fail, key send_relay not exist");
					return;
				}

				Json::Value data;
				data.fromString(value_ptr.value());
				id = data["init_seq"].asInt64();
			}

			protocol::KeyPair value_ptr;
			std::string key = utils::String::Format("send_proposal_" FMT_I64 "", id);
			if (!acc->GetMetaData(key, value_ptr)){
				//尚未找到合约相关信息
				LOG_INFO("Get metadata fail, key(%s) not exist", key.c_str());
				return;
			}

			//设置提案信息
			Json::Value data;
			data.fromString(value_ptr.value());
			protocol::CrossProposalInfo info;
			info.set_type(cross_proposal.type());
			info.set_proposal_id(id);
			info.set_proposal_body(data["proposal"].toFastString());
			info.set_status(data["state"].asInt());
			const Json::Value &json_votes_array = data["vote"];
			for (size_t i = 0; i < json_votes_array.size(); i++) {
				const Json::Value &vote_obj = json_votes_array[i];
				*info.add_confirmed_notarys() = vote_obj[(Json::UInt)0].asString();
			}

			*cross_proposal_response.mutable_proposal_info() = info;
		}
		else{
			LOG_ERROR("Parse proposal error!");
			return;
		}

		if (cross_proposal_response.proposal_info().proposal_id() <= 0){
			LOG_ERROR("No proposal response message.");
			return;
		}

		channel_->SendResponse(static_notary_unique_, message, cross_proposal_response.SerializeAsString());
		return;
	}

	void MessageHandler::OnHandleCommInfo(const protocol::WsMessage &message){
		protocol::CrossCommInfo comm_info;
		comm_info.ParseFromString(message.data());
		protocol::CrossCommInfoResponse response;

		LOG_INFO("Recvd comm info for request.");

		AccountFrm::pointer acc = NULL;
		if (!Environment::AccountFromDB(comm_contract_, acc)) {
			LOG_ERROR("GetAccount fail, account(%s) not exist", comm_contract_.c_str());
			return;
		}

		//查询合约信息的input信息
		protocol::KeyPair input_obj;
		if (acc->GetMetaData("receive_relay", input_obj)){
			Json::Value data;
			data.fromString(input_obj.value());
			response.set_comm_unique(data["chain_id"].asString());
			response.set_input_finish_seq(data["complete_seq"].asInt64());
			response.set_input_max_seq(data["init_seq"].asInt64());

			const Json::Value &notary_list = data["notary_list"];
			for (size_t i = 0; i < notary_list.size(); i++){
				*response.add_notarys() = notary_list[i].asString();
			}
		}
		
		//查询合约信息的output信息
		protocol::KeyPair cross_chain;
		if (acc->GetMetaData("send_relay", cross_chain)){
			Json::Value data;
			data.fromString(cross_chain.value());
			response.set_comm_unique(data["f_chain_id"].asString());
			response.set_output_finish_seq(data["complete_seq"].asInt64());
			response.set_output_max_seq(data["init_seq"].asInt64());

			const Json::Value &notary_list = data["notary_list"];
			for (size_t i = 0; i < notary_list.size(); i++){
				*response.add_notarys() = notary_list[i].asString();
			}
		}

		channel_->SendResponse(static_notary_unique_, message, response.SerializeAsString());
	}

	void MessageHandler::OnHandleAccountNonce(const protocol::WsMessage &message){
		protocol::CrossAccountNonce account;
		account.ParseFromString(message.data());
		protocol::CrossAccountNonceResponse response;

		//Find the account nonce value
		AccountFrm::pointer acc = NULL;
		if (!Environment::AccountFromDB(account.account(), acc)) {
			LOG_ERROR("GetAccount fail, account(%s) not exist", comm_contract_.c_str());
			return;
		}

		response.set_nonce(acc->GetAccountNonce());
		channel_->SendResponse(static_notary_unique_, message, response.SerializeAsString());
	}

	void MessageHandler::OnHandleDoTransaction(const protocol::WsMessage &message){
		protocol::CrossDoTransaction trans;
		trans.ParseFromString(message.data());
		protocol::CrossDoTransactionResponse response;

		Result result;
		result.set_code(protocol::ERRCODE_SUCCESS);
		result.set_desc("");

		protocol::TransactionEnv tran_env = trans.tran_env();
		TransactionFrm::pointer ptr = std::make_shared<TransactionFrm>(tran_env);
		do 
		{
			// add node signature
			PrivateKey privateKey(bubi::Configure::Instance().p2p_configure_.node_private_key_);
			if (!privateKey.IsValid()) {
				result.set_code(protocol::ERRCODE_INVALID_PRIKEY);
				result.set_desc("signature failed");
				break;
			}
			std::string sign = privateKey.Sign(tran_env.transaction().SerializeAsString());
			protocol::Signature *signpro = tran_env.add_signatures();
			signpro->set_sign_data(sign);
			signpro->set_public_key(privateKey.GetBase16PublicKey());

			GlueManager::Instance().OnTransaction(ptr, result);
			PeerManager::Instance().Broadcast(protocol::OVERLAY_MSGTYPE_TRANSACTION, tran_env.SerializeAsString());		
		} while (false);
		
		response.set_error_code(result.code());
		response.set_error_desc(result.desc());
		response.set_hash(trans.hash());
		channel_->SendResponse(static_notary_unique_, message, response.SerializeAsString());
	}

	bool CrossChainMgr::Initialize(){
		CrossConfigure &config = Configure::Instance().cross_configure_;
		if (!config.enabled_){
			LOG_TRACE("Failed to init cross chain mgr, configuration file is not allowed");
			return true;
		}

		ChannelParameter param;
		param.inbound_ = false;
		param.comm_unique_ = config.comm_unique_;
		param.target_addr_ = config.target_addr_;

		handler_ = new MessageHandler(&channel_, config.comm_contract_);
		channel_.Initialize(param);
		channel_.Register(this, protocol::CROSS_MSGTYPE_PROPOSAL);
		channel_.Register(this, protocol::CROSS_MSGTYPE_COMM_INFO);
		channel_.Register(this, protocol::CROSS_MSGTYPE_ACCOUNT_NONCE);
		channel_.Register(this, protocol::CROSS_MSGTYPE_DO_TRANSACTION);
		return true;
	}

	bool CrossChainMgr::Exit(){
		if (!Configure::Instance().cross_configure_.enabled_){
			LOG_TRACE("Failed to exit cross chain mgr, configuration file is not allowed");
			return true;
		}

		channel_.Exit();
		return true;
	}

	void CrossChainMgr::HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message){
		if (!message.request()){
			return;
		}

		switch (message.type())
		{
		case protocol::CROSS_MSGTYPE_PROPOSAL:
			handler_->OnHandleProposal(message);
			break;
		case protocol::CROSS_MSGTYPE_COMM_INFO:
			handler_->OnHandleCommInfo(message);
			break;
		case protocol::CROSS_MSGTYPE_ACCOUNT_NONCE:
			handler_->OnHandleAccountNonce(message);
			break;
		case protocol::CROSS_MSGTYPE_DO_TRANSACTION:
			handler_->OnHandleDoTransaction(message);
			break;
		default:
			break;
		}

		return;
	}
	
}
