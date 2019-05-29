
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

	void ChainObj::SetChainInfo(const std::string &chain_unique, const std::string &target_chain_unique){
		chain_unique_ = chain_unique;
		target_chain_unique_ = target_chain_unique;
	}

	void ChainObj::SetPeerChain(ChainObj *peer_chain){
		peer_chain_ = peer_chain;
	}

	void ChainObj::OnHandleProposal(const std::string &data){
		protocol::CrossProposal cross_proposal;
		cross_proposal.ParseFromString(data);
		//TODO
		LOG_INFO("Recv Proposal..");
		return;
	}

	void ChainObj::OnHandleProposalResponse(const std::string &data){
		protocol::CrossHelloResponse cross_proposal_response;
		cross_proposal_response.ParseFromString(data);
		//TODO
		LOG_INFO("Recv Proposal Response..");
		return;
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

		PairChainMap::iterator itr = Configure::Instance().pair_chain_map_.begin();
		const PairChainConfigure &pair_chain_a = itr->second;
		a_chain_obj_.SetPeerChain(&b_chain_obj_);
		a_chain_obj_.SetChainInfo(pair_chain_a.chain_unique_, pair_chain_a.target_chain_unique_);

		itr++;
		const PairChainConfigure &pair_chain_b = itr->second;;
		b_chain_obj_.SetPeerChain(&a_chain_obj_);
		a_chain_obj_.SetChainInfo(pair_chain_b.chain_unique_, pair_chain_b.target_chain_unique_);

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
		//1、判断是否存在于 chain_obj中

		//2、把消息交付于对应的chain_obj 处理
	}
}

