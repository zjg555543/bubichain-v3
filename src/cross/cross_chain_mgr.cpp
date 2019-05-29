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
			protocol::CrossProposal proposal;
			//TODO：分析meta data发送消息
			channel_->SendMessage(protocol::CROSS_MSGTYPE_PROPOSAL, proposal.SerializeAsString());
		}
	}

	void MessageHandler::HandleMessage(const std::string &chain_unique, int64_t msg_type, bool request, const std::string &data){
		if (protocol::CROSS_MSGTYPE_PROPOSAL == msg_type){
			if (request){
				OnHandleProposal(data);
			}
			else{
				OnHandleProposalResponse(data);
			}
		}

		return;
	}

	void MessageHandler::OnHandleProposal(const std::string &data){
		protocol::CrossProposal cross_proposal;
		cross_proposal.ParseFromString(data);
		//TODO
		LOG_INFO("Recv Proposal..");
		return;
	}

	void MessageHandler::OnHandleProposalResponse(const std::string &data){
		protocol::CrossHelloResponse cross_proposal_response;
		cross_proposal_response.ParseFromString(data);
		//TODO
		LOG_INFO("Recv Proposal Response..");
		return;
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
		param.chain_unique_ = config.chain_unique_;
		channel_.Initialize(param);
		channel_.Register(&handler_, protocol::CROSS_MSGTYPE_PROPOSAL);

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
	
}
