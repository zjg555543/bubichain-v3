
#include <utils/headers.h>
#include <common/general.h>
#include <notary/configure.h>
#include <notary/notary_mgr.h>

namespace bubi {

	ChainObj::ChainObj(){
		peer_chain_ = nullptr;
	}

	ChainObj::~ChainObj(){

	}

	void ChainObj::OnTimer(int64_t current_time){

	}

	void ChainObj::SetPeerChain(ChainObj *peer_chain){
		peer_chain_ = peer_chain;
	}

	NotaryMgr::NotaryMgr(){
	}

	NotaryMgr::~NotaryMgr(){
	}

	bool NotaryMgr::Initialize(){

		NotaryConfigure &config = Configure::Instance().notary_configure_;

		if (!config.enabled_){
			LOG_TRACE("Failed to init notary mgr, configuration file is not allowed");
			return true;
		}
		
		TimerNotify::RegisterModule(this);
		LOG_INFO("Initialized notary mgr successfully");

		a_chain_obj_.SetPeerChain(&b_chain_obj_);
		b_chain_obj_.SetPeerChain(&a_chain_obj_);

		ChannelParameter param;
		param.inbound_ = true;
		param.notary_addr_ = config.listen_addr_;
		channel_.Initialize(param);
		channel_.Register(this, protocol::CROSS_MSGTYPE_PROPOSAL);

		return true;
	}

	bool NotaryMgr::Exit(){

		return true;
	}

	void NotaryMgr::OnTimer(int64_t current_time){
		a_chain_obj_.OnTimer(current_time);
		b_chain_obj_.OnTimer(current_time);
	}

	void NotaryMgr::HandleMessage(const std::string &chain_unique, int64_t msg_type, bool request, const std::string &data){

	}

	void NotaryMgr::OnHandleProposal(const std::string &data){
		protocol::CrossProposal cross_proposal;
		cross_proposal.ParseFromString(data);
		//TODO
		LOG_INFO("Recv Proposal..");
		return;
	}

	void NotaryMgr::OnHandleProposalResponse(const std::string &data){
		protocol::CrossHelloResponse cross_proposal_response;
		cross_proposal_response.ParseFromString(data);
		//TODO
		LOG_INFO("Recv Proposal Response..");
		return;
	}

}

