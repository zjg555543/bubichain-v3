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

		LOG_INFO("Recv Proposal request, proposal type:%d, id:(" FMT_I64 " ).", cross_proposal.type(), cross_proposal.proposal_id());

		//find input proposal
		AccountFrm::pointer acc = NULL;
		if (!Environment::AccountFromDB(comm_contract_, acc)) {
			LOG_ERROR("GetAccount fail, account(%s) not exist", comm_contract_.c_str());
			return;
		}

		if (cross_proposal.type() == protocol::CROSS_PROPOSAL_INPUT){
			//TODO 对接合约参数
			protocol::KeyPair value_ptr;
			cross_proposal.asset_contract();

			if (!acc->GetMetaData("xxxxxxxxxx", value_ptr)){
				//尚未找到合约相关信息
				LOG_ERROR("GetAccount fail, account(%s) not exist", comm_contract_.c_str());
				return;
			}

			protocol::CrossProposalInfo info;
			//TODO 保存提案信息
			info.set_asset_contract(cross_proposal.asset_contract());

			*cross_proposal_response.mutable_proposal_info() = info;
		}
		else if (cross_proposal.type() == protocol::CROSS_PROPOSAL_INPUT){
			//TODO 查找output消息
			protocol::KeyPair value_ptr;
			cross_proposal.asset_contract();
			if (!acc->GetMetaData("xxxxxxxxxx", value_ptr)){
				//尚未找到合约相关信息
				return;
			}

			protocol::CrossProposalInfo info;
			//TODO 保存提案信息
			info.set_asset_contract(cross_proposal.asset_contract());

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

		channel_->SendResponse("", message, cross_proposal_response.SerializeAsString());
		return;
	}

	void MessageHandler::OnHandleCommInfo(const protocol::WsMessage &message){
		protocol::CrossCommInfo comm_info;
		comm_info.ParseFromString(message.data());
		protocol::CrossCommInfoResponse response;

		//Find the notary list for the contract
		AccountFrm::pointer acc = NULL;
		if (!Environment::AccountFromDB(comm_contract_, acc)) {
			LOG_ERROR("GetAccount fail, account(%s) not exist", comm_contract_.c_str());
			return;
		}

		protocol::KeyPair value_ptr;
		if (!acc->GetMetaData("xxxxxxxxxx", value_ptr)){
			//No contract information has been found
			LOG_ERROR("GetAccount fail, xxxxxxxxxx not exist");
			return;
		}

		//TODO 读取公证人列表
		for (size_t i = 0; i <= 1; i++){
			*response.add_notarys() = "123456";
		}

		//TODO 读取输入资产的业务地址
		for (size_t i = 0; i <= 1; i++){
			*response.add_asset_input_contracts() = "123456";
		}

		//TODO 读取输出资产的业务地址
		for (size_t i = 0; i <= 1; i++){
			*response.add_asset_output_contracts() = "123456";
		}

		channel_->SendResponse("", message, response.SerializeAsString());
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
		channel_->SendResponse("", message, response.SerializeAsString());
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
		channel_->SendResponse("", message, response.SerializeAsString());
	}

	CrossChainMgr::CrossChainMgr(){
	}

	CrossChainMgr::~CrossChainMgr(){
	}

	bool CrossChainMgr::Initialize(){
		CrossConfigure &config = Configure::Instance().cross_configure_;

		if (!config.enabled_){
			LOG_TRACE("Failed to init cross chain mgr, configuration file is not allowed");
			return true;
		}

		ChannelParameter param;
		param.inbound_ = false;
		param.notary_addr_ = config.notary_addr_;
		param.comm_unique_ = config.comm_unique_;

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
		if (protocol::CROSS_MSGTYPE_PROPOSAL == message.type() && message.request()){
			handler_->OnHandleProposal(message);
		}

		if (protocol::CROSS_MSGTYPE_COMM_INFO == message.type() && message.request()){
			handler_->OnHandleCommInfo(message);
		}

		if (protocol::CROSS_MSGTYPE_ACCOUNT_NONCE == message.type() && message.request()){
			handler_->OnHandleAccountNonce(message);
		}

		if (protocol::CROSS_MSGTYPE_DO_TRANSACTION == message.type() && message.request()){
			handler_->OnHandleDoTransaction(message);
		}

		return;
	}
	
}
