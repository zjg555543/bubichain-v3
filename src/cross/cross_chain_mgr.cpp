#include <common/storage.h>
#include <common/private_key.h>
#include <common/pb2json.h>
#include <api/websocket_server.h>
#include "cross_chain_mgr.h"

namespace bubi {

	BlockListener::BlockListener(MessageChannel &channel){
		channel_ = &channel;
	}

	BlockListener::~BlockListener(){

	}

	void BlockListener::HandleBlock(LedgerFrm::pointer closing_ledger){
		//TODO
		if (0){
			//读取指定合约的消息事件，则通知给公证人
			protocol::CrossProposalInfo proposal;
			//TODO：分析meta data发送消息
			channel_->SendRequest("", protocol::CROSS_MSGTYPE_PROPOSAL_NOTICE, proposal.SerializeAsString());
		}
	}

	MessageHandler::MessageHandler(MessageChannel *channel){
		channel_ = channel;
	}

	void MessageHandler::OnHandleProposal(const protocol::WsMessage &message){
		protocol::CrossProposal cross_proposal;
		cross_proposal.ParseFromString(message.data());
		protocol::CrossProposalResponse cross_proposal_response;

		LOG_INFO("Recv Proposal request, proposal type:%d, id:(" FMT_I64 " ).", cross_proposal.type(), cross_proposal.proposal_id());
		if (cross_proposal.type() == protocol::CROSS_PROPOSAL_TRANS){
			//TODO 查找需要转移的消息队列

		}
		else if(cross_proposal.type() == protocol::CROSS_PROPOSAL_FEEDBACK){
			//TODO 查找需要反馈的消息队列

		}
		else{
			LOG_ERROR("Parse proposal error!");
			return;
		}

		channel_->SendResponse("", message, cross_proposal_response.SerializeAsString());
		return;
	}

	void MessageHandler::OnHandleNotarys(const protocol::WsMessage &message){
		protocol::CrossNotarys notarys;
		notarys.ParseFromString(message.data());
		protocol::CrossNotarysResponse response;
		//TODO 查找对应合约的公证人列表


		channel_->SendResponse("", message, response.SerializeAsString());
	}

	void MessageHandler::OnHandleAccountNonce(const protocol::WsMessage &message){
		protocol::CrossAccountNonce account;
		account.ParseFromString(message.data());
		protocol::CrossAccountNonceResponse response;
		//TODO 查找账号nonce值


		channel_->SendResponse("", message, response.SerializeAsString());
	}

	void MessageHandler::OnHandleDoTransaction(const protocol::WsMessage &message){
		protocol::CrossDoTransaction trans;
		trans.ParseFromString(message.data());
		protocol::CrossDoTransactionResponse response;
		//TODO 发起交易，并返回交易的结果

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

		handler_ = new MessageHandler(&channel_);
		channel_.Initialize(param);
		channel_.Register(this, protocol::CROSS_MSGTYPE_PROPOSAL);
		channel_.Register(this, protocol::CROSS_MSGTYPE_NOTARYS);
		channel_.Register(this, protocol::CROSS_MSGTYPE_ACCOUNT_NONCE);
		channel_.Register(this, protocol::CROSS_MSGTYPE_DO_TRANSACTION);

		block_listener_ = new BlockListener(channel_);
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

	void CrossChainMgr::HandleBlock(LedgerFrm::pointer closing_ledger){
		if (block_listener_ == nullptr || closing_ledger == nullptr){
			return;
		}
		block_listener_->HandleBlock(closing_ledger);
	}

	void CrossChainMgr::HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message){
		if (protocol::CROSS_MSGTYPE_PROPOSAL == message.type() && message.request()){
			handler_->OnHandleProposal(message);
		}

		if (protocol::CROSS_MSGTYPE_NOTARYS == message.type() && message.request()){
			handler_->OnHandleNotarys(message);
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
