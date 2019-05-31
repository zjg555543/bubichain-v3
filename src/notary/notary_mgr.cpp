
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
		//获取最新的output列表

		//获取最新的intput列表

		//排序outmap

		//排序inputmap

		//检查交易错误次数是否超过最大值

		//投票output

		//投票input
	}

	void ChainObj::SetChainInfo(const std::string &comm_unique, const std::string &target_comm_unique){
		comm_unique_ = comm_unique;
		target_comm_unique_ = target_comm_unique;
	}

	void ChainObj::OnHandleMessage(const protocol::WsMessage &message){
		if (message.type() == protocol::CROSS_MSGTYPE_PROPOSAL && !message.request()){
			OnHandleProposalResponse(message);
		}

		if (message.type() == protocol::CROSS_MSGTYPE_PROPOSAL_NOTICE){
			OnHandleProposalNotice(message);
		}

		if (message.type() == protocol::CROSS_MSGTYPE_NOTARYS && !message.request()){
			OnHandleNotarysResponse(message);
		}

		if (message.type() == protocol::CROSS_MSGTYPE_ACCOUNT_NONCE && !message.request()){
			OnHandleAccountNonceResponse(message);
		}

		if (message.type() == protocol::CROSS_MSGTYPE_DO_TRANSACTION && !message.request()){
			OnHandleProposalDoTransResponse(message);
		}
	}

	void ChainObj::SetPeerChain(ChainObj *peer_chain){
		peer_chain_ = peer_chain;
	}

	void ChainObj::OnHandleProposalNotice(const protocol::WsMessage &message){
		protocol::CrossProposalInfo cross_proposal;
		cross_proposal.ParseFromString(message.data());
		LOG_INFO("Recv Proposal Notice..");
		HandleProposalNotice(cross_proposal);
		return;
	}

	void ChainObj::OnHandleProposalResponse(const protocol::WsMessage &message){
		protocol::CrossProposalResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv Proposal Response..");
		HandleProposalNotice(msg.proposal_info());
	}

	void ChainObj::OnHandleNotarysResponse(const protocol::WsMessage &message){
		protocol::CrossNotarys msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv Notarys Response..");
		//TODO 处理公证人列表

	}

	void ChainObj::OnHandleAccountNonceResponse(const protocol::WsMessage &message){
		protocol::CrossAccountNonceResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv Account Nonce Response..");
		//TODO 处理账号nonce值

	}

	void ChainObj::OnHandleProposalDoTransResponse(const protocol::WsMessage &message){
		protocol::CrossDoTransactionResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv Do Trans Response..");
		//TODO 处理交易结果值

	}

	void ChainObj::HandleProposalNotice(const protocol::CrossProposalInfo &proposal_info){
		//TODO 处理提案用例
		LOG_INFO("Handel Proposal Notice..");
	}

	void ChainObj::VoteOutPut(){
		//1.获取对端input列表的情况

		//2.获取自己的output列表

		//3.检查自己的output列表最后一个完成状态的下一个值是否存在，如果存在则判断自己是否投过票并进行投票处理，如果不存在检查对端的intput列表，是否需要进行新的投票表决
	}

	void ChainObj::VoteInPut(){
		//1.获取对端的output列表

		//2.获取自己的input列表

		//3.检查自己的input列表状态里最后一个完成状态的下一个值是否存在，如果存在判断自己是否投过票并进行投票处理，如果不存在检查对端的output列表，是否需要进行新的投票表决
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
		a_chain_obj_.SetChainInfo(pair_chain_a.comm_unique_, pair_chain_a.target_comm_unique_);
		a_chain_obj_.SetPeerChain(&b_chain_obj_);

		itr++;
		const PairChainConfigure &pair_chain_b = itr->second;
		b_chain_obj_.SetChainInfo(pair_chain_b.comm_unique_, pair_chain_b.target_comm_unique_);
		b_chain_obj_.SetPeerChain(&a_chain_obj_);

		ChannelParameter param;
		param.inbound_ = true;
		param.notary_addr_ = config.listen_addr_;
		channel_.Initialize(param);
		channel_.Register(this, protocol::CROSS_MSGTYPE_PROPOSAL);
		channel_.Register(this, protocol::CROSS_MSGTYPE_PROPOSAL_NOTICE);
		channel_.Register(this, protocol::CROSS_MSGTYPE_NOTARYS);
		channel_.Register(this, protocol::CROSS_MSGTYPE_ACCOUNT_NONCE);
		channel_.Register(this, protocol::CROSS_MSGTYPE_DO_TRANSACTION);

		return true;
	}

	bool NotaryMgr::Exit(){

		return true;
	}

	void NotaryMgr::OnTimer(int64_t current_time){
		a_chain_obj_.OnTimer(current_time);
		b_chain_obj_.OnTimer(current_time);
	}

	void NotaryMgr::HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message){
		//1、判断是否存在于 chain_obj中
		ChainObj * chain = nullptr;
		if (comm_unique == a_chain_obj_.GetChainUnique()){
			chain = &a_chain_obj_;
		}
		else if (comm_unique == b_chain_obj_.GetChainUnique()){
			chain = &b_chain_obj_;
		}

		if (chain == nullptr){
			LOG_ERROR("Can not find chain.");
			return;
		}

		//2、把消息交付于对应的chain_obj 处理
		chain->OnHandleMessage(message);
	}
}

