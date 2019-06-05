
#include <utils/headers.h>
#include <utils/common.h>
#include <common/general.h>
#include <notary/configure.h>
#include <notary/notary_mgr.h>
#include <common/private_key.h>

namespace bubi {

	ChainObj::ChainObj(MessageChannel *channel, const std::string &comm_unique, const std::string &notary_address, const std::string &private_key, const std::string &comm_contract){
		ResetChainInfo();
		peer_chain_ = nullptr;
		channel_ = channel;
		comm_unique_ = comm_unique;
		notary_address_ = notary_address;
		private_key_ = private_key;
		comm_contract_ = comm_contract;
	}

	ChainObj::~ChainObj(){

	}

	void ChainObj::OnSlowTimer(int64_t current_time){
		//请求通信合约的信息
		RequestCommInfo();

		//Get the latest output list and sort outmap
		RequestAndUpdate(protocol::CROSS_PROPOSAL_OUTPUT);

		//Get the latest intput list and sort the inputmap
		RequestAndUpdate(protocol::CROSS_PROPOSAL_INPUT);

		//Check the number of tx errors
		CheckTxError();
	}

	void ChainObj::OnFastTimer(int64_t current_time){
		//vote output
		Vote(protocol::CROSS_PROPOSAL_OUTPUT);

		//vote input
		Vote(protocol::CROSS_PROPOSAL_INPUT);

		SubmitTransaction();
	}

	void ChainObj::OnHandleMessage(const protocol::WsMessage &message){
		if (message.request()){
			return;
		}

		switch (message.type())
		{
		case protocol::CROSS_MSGTYPE_PROPOSAL:
			OnHandleProposalResponse(message);
			break;
		case protocol::CROSS_MSGTYPE_COMM_INFO:
			OnHandleCrossCommInfoResponse(message);
			break;
		case protocol::CROSS_MSGTYPE_ACCOUNT_NONCE:
			OnHandleAccountNonceResponse(message);
			break;
		case protocol::CROSS_MSGTYPE_DO_TRANSACTION:
			OnHandleProposalDoTransResponse(message);
			break;
		default:
			break;
		}
	}

	void ChainObj::SetPeerChain(std::shared_ptr<ChainObj> peer_chain){
		peer_chain_ = peer_chain;
	}

	bool ChainObj::GetProposalInfo(protocol::CROSS_PROPOSAL_TYPE type, int64_t index, protocol::CrossProposalInfo &info){
		utils::MutexGuard guard(lock_);
		ProposalRecord *record = GetProposalRecord(type);
		auto itr_info = record->proposal_info_map.find(index);
		if (itr_info == record->proposal_info_map.end()){
			return false;
		}
		info = itr_info->second;
		return true;
	}

	void ChainObj::OnHandleProposalNotice(const protocol::WsMessage &message){
		protocol::CrossProposalInfo cross_proposal;
		cross_proposal.ParseFromString(message.data());
		LOG_INFO("Recv proposal notice..");
		HandleProposalNotice(cross_proposal);
		return;
	}

	void ChainObj::OnHandleProposalResponse(const protocol::WsMessage &message){
		protocol::CrossProposalResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv proposal pesponse..");
		HandleProposalNotice(msg.proposal_info());
	}

	void ChainObj::OnHandleCrossCommInfoResponse(const protocol::WsMessage &message){
		protocol::CrossCommInfoResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv cross comm info response..");

		assert(comm_unique_ == msg.comm_unique());

		//保存公证人列表
		if (msg.notarys_size() >= 100){
			LOG_ERROR("Notary nums is no more than 100");
			assert(false);
			return;
		}
		memset(&notary_list_, 0, sizeof(notary_list_));
		for (int i = 0; i < msg.notarys_size(); i++) {
			notary_list_[i] = msg.notarys(i);
		}

		//保存最大确认序号和最大序号
		input_record_.affirm_max_seq = MAX(input_record_.affirm_max_seq, msg.input_finish_seq());
		input_record_.recv_max_seq = MAX(input_record_.recv_max_seq, msg.input_max_seq());

		output_record_.affirm_max_seq = MAX(output_record_.affirm_max_seq, msg.output_finish_seq());
		output_record_.recv_max_seq = MAX(output_record_.recv_max_seq, msg.output_max_seq());
	}

	void ChainObj::OnHandleAccountNonceResponse(const protocol::WsMessage &message){
		protocol::CrossAccountNonceResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv Account Nonce Response..");
		nonce_ = msg.nonce();
	}

	void ChainObj::OnHandleProposalDoTransResponse(const protocol::WsMessage &message){
		protocol::CrossDoTransactionResponse msg;
		msg.ParseFromString(message.data());
		LOG_INFO("Recv Do Trans Response..");

		auto iter = std::find(tx_history_.begin(), tx_history_.end(), msg.hash());
		if (iter == tx_history_.end()){
			return;
		}

		if (msg.error_code() == protocol::ERRCODE_SUCCESS){
			error_tx_times_ = 0;
			tx_history_.clear();
			return;
		}
		error_tx_times_++;
		LOG_ERROR("Failed to Do Transaction, (" FMT_I64 "),tx hash is %s,err_code is (" FMT_I64 "),err_desc is %s",
			msg.hash().c_str(), msg.error_code(), msg.error_desc().c_str());

	}

	void ChainObj::HandleProposalNotice(const protocol::CrossProposalInfo &proposal_info){
		//save proposal
		utils::MutexGuard guard(lock_);
		ProposalRecord *proposal = GetProposalRecord(proposal_info.type());
		assert(proposal_info.proposal_id() >= 0);

		proposal->proposal_info_map[proposal_info.proposal_id()] = proposal_info;
		proposal->recv_max_seq = MAX(proposal_info.proposal_id(), proposal->recv_max_seq);
		if ((proposal_info.status() == EXECUTE_STATE_SUCCESS) || (proposal_info.status() == EXECUTE_STATE_FAIL)){
			proposal->affirm_max_seq = MAX(proposal_info.proposal_id(), proposal->affirm_max_seq);
		}
	}

	void ChainObj::RequestCommInfo(){
		protocol::CrossCommInfo comm_info;
		channel_->SendRequest(comm_unique_, protocol::CROSS_MSGTYPE_COMM_INFO, comm_info.SerializeAsString());
	}

	void ChainObj::RequestAndUpdate(protocol::CROSS_PROPOSAL_TYPE type){
		utils::MutexGuard guard(lock_);
		//请求最新的提案
		protocol::CrossProposal cross_proposal;
		cross_proposal.set_type(type);
		cross_proposal.set_proposal_id(-1);
		channel_->SendRequest(comm_unique_, protocol::CROSS_MSGTYPE_PROPOSAL, cross_proposal.SerializeAsString());

		//删除已完成的提案
		ProposalRecord *record = GetProposalRecord(type);
		ProposalInfoMap &proposal_info = record->proposal_info_map;
		for (auto itr_info = proposal_info.begin(); itr_info != proposal_info.end();){
			const protocol::CrossProposalInfo &info = itr_info->second;
			if ((info.status() == EXECUTE_STATE_SUCCESS) || (info.status() == EXECUTE_STATE_FAIL)){
				proposal_info.erase(itr_info++);
				continue;
			}

			itr_info++;
		}
		
		//请求从最后确认的一个提案后的缺失的提案开始，每次最大更新10个
		int64_t max_nums = MIN(10, (record->recv_max_seq - record->affirm_max_seq));
		if (max_nums <= 0){
			max_nums = 1;
		}
		for (int64_t i = 1; i <= max_nums; i++){
			int64_t index = record->affirm_max_seq + i;
			protocol::CrossProposal proposal;
			proposal.set_type(type);
			proposal.set_proposal_id(index);
			channel_->SendRequest(comm_unique_, protocol::CROSS_MSGTYPE_PROPOSAL, proposal.SerializeAsString());
		}
	}

	void ChainObj::CheckTxError(){
		//TODO 处理异常的交易
	}

	void ChainObj::Vote(protocol::CROSS_PROPOSAL_TYPE type){
		utils::MutexGuard guard(lock_);
		ProposalRecord *record = GetProposalRecord(type);

		protocol::CROSS_PROPOSAL_TYPE get_peer_type;
		if (type == protocol::CROSS_PROPOSAL_OUTPUT){
			get_peer_type = protocol::CROSS_PROPOSAL_INPUT;
		}
		else if (type == protocol::CROSS_PROPOSAL_INPUT){
			get_peer_type = protocol::CROSS_PROPOSAL_OUTPUT;
		}
		else{
			LOG_ERROR("Unknown proposal type.");
			return;
		}

		//查找最后一个完成提案，并检查下一个提案是否已经存在
		record->affirm_max_seq = MAX(0, record->affirm_max_seq);
		protocol::CrossProposalInfo vote_proposal;
		vote_proposal.set_proposal_id(-1);

		do
		{
			const ProposalInfoMap &proposal_info_map = record->proposal_info_map;
			int64_t next_proposal_index = record->affirm_max_seq + 1;
			auto itr = proposal_info_map.find(next_proposal_index);
			if (itr == proposal_info_map.end()){
				//如果不存在，则主动检查对端的列表
				if (!peer_chain_->GetProposalInfo(get_peer_type, next_proposal_index, vote_proposal)){
					LOG_INFO("No proposl");
					return;
				}
				break;
			}

			//如果存在则判断自己是否投过票并进行投票处理
			const protocol::CrossProposalInfo &proposal = itr->second;
			bool confirmed = false;
			for (int i = 0; i < proposal.confirmed_notarys_size(); i++){
				if (proposal.confirmed_notarys(i) == notary_address_){
					confirmed = true;
					break;
				}
			}

			if (confirmed){
				break;
			}

			//对于自己没有投票的提案，不要盲目相信已经提案的列表，而是需要自己从对端获取提案的原始数据
			if (!peer_chain_->GetProposalInfo(get_peer_type, next_proposal_index, vote_proposal)){
				LOG_ERROR("No proposl INFO");
				break;
			}
		} while (false);

		if (vote_proposal.proposal_id() == -1){
			return;
		}

		//当获取对端为output时候，需要保证其状态非ok状态
		if (get_peer_type == protocol::CROSS_PROPOSAL_OUTPUT){
			if (vote_proposal.status() == EXECUTE_STATE_SUCCESS){
				LOG_INFO("If peers' output is sucess, ignore it.");
				return;
			}
		}
		//当获取对端为input时候，需要保证其状态为成功状态或者失败状态
		else if (get_peer_type == protocol::CROSS_PROPOSAL_INPUT){
			if (vote_proposal.status() == EXECUTE_STATE_INITIAL || vote_proposal.status() == EXECUTE_STATE_PROCESSING){
				LOG_INFO("If peers' input is init or processing, ignore it, status:%d", vote_proposal.status());
				return;
			}
		}

		//改变提案状态类型，保存提案队列。
		vote_proposal.set_type(type);
		vote_proposal.clear_status();
		vote_proposal.clear_confirmed_notarys();
		proposal_info_vector_.push_back(vote_proposal);
	}

	void ChainObj::SubmitTransaction(){
		utils::MutexGuard guard(lock_);
		protocol::CrossDoTransaction cross_do_trans;
		protocol::TransactionEnv tran_env;
		protocol::Transaction *tran = tran_env.mutable_transaction();
		tran->set_source_address(notary_address_);
		tran->set_nonce(nonce_);

		//打包操作
		for (unsigned i = 0; i < proposal_info_vector_.size(); i++){
			const protocol::CrossProposalInfo &info = proposal_info_vector_[i];
			protocol::Operation *ope = tran->add_operations();
			ope->set_type(protocol::Operation_Type_PAYMENT);
			protocol::OperationPayment *payment = ope->mutable_payment();
			payment->set_dest_address(comm_contract_);
			if (info.type() == protocol::CROSS_PROPOSAL_INPUT){
				Json::Value input_value;
				Json::Value data;
				data.fromString(info.proposal_body());
				input_value["action"] = "put_msg";
				input_value["data"] = data;
				payment->set_input(input_value.toFastString());
			}
			else{
				Json::Value input_value;
				Json::Value data;
				data.fromString(info.proposal_body());
				input_value["function"] = "processState";
				input_value["seq"] = info.proposal_id();
				input_value["state"] = info.status();
				payment->set_input(input_value.toFastString());
			}
		}

		std::string content = tran->SerializeAsString();
		PrivateKey privateKey(private_key_);
		if (!privateKey.IsValid()) {
			LOG_ERROR("Submit transaction error.");
			return;
		}
		std::string sign = privateKey.Sign(content);
		protocol::Signature *signpro = tran_env.add_signatures();
		signpro->set_sign_data(sign);
		signpro->set_public_key(privateKey.GetBase16PublicKey());

		//发送交易
		protocol::CrossDoTransaction do_trans;
		do_trans.set_hash("xxxxxx");
		*do_trans.mutable_tran_env() = tran_env;
		channel_->SendRequest(comm_unique_, protocol::CROSS_MSGTYPE_DO_TRANSACTION, do_trans.SerializeAsString());
		proposal_info_vector_.clear();
	}

	ChainObj::ProposalRecord* ChainObj::GetProposalRecord(protocol::CROSS_PROPOSAL_TYPE type){
		ProposalRecord *record = nullptr;
		if (type == protocol::CROSS_PROPOSAL_OUTPUT){
			record = &output_record_;
		}
		else if (type == protocol::CROSS_PROPOSAL_INPUT){
			record = &input_record_;
		}
		else{
			LOG_ERROR("Canot find proposal, type:%d", type);
			assert(false);
		}
		return record;
	}

	bool NotaryMgr::Initialize(){
		last_update_time_ = utils::Timestamp::HighResolution();
		update_times_ = 0;
		NotaryConfigure &config = Configure::Instance().notary_configure_;		
		TimerNotify::RegisterModule(this);
		LOG_INFO("Initialized notary mgr successfully");

		PairChainMap::iterator itr = Configure::Instance().pair_chain_map_.begin();
		const PairChainConfigure &config_a = itr->second;
		itr++;
		const PairChainConfigure &config_b = itr->second;
		std::shared_ptr<ChainObj> a = std::make_shared<ChainObj>(&channel_, config_a.comm_unique_, config.notary_address_, config.private_key_, config_a.comm_contract_);
		std::shared_ptr<ChainObj> b = std::make_shared<ChainObj>(&channel_, config_b.comm_unique_, config.notary_address_, config.private_key_, config_a.comm_contract_);

		chain_obj_map_[config_a.comm_unique_] = a;
		chain_obj_map_[config_b.comm_unique_] = b;

		a->SetPeerChain(b);
		b->SetPeerChain(a);

		ChannelParameter param;
		param.inbound_ = true;
		param.listen_addr_ = config.listen_addr_;
		param.comm_unique_ = static_notary_unique_;
		channel_.Initialize(param);
		channel_.Register(this, protocol::CROSS_MSGTYPE_PROPOSAL);
		channel_.Register(this, protocol::CROSS_MSGTYPE_COMM_INFO);
		channel_.Register(this, protocol::CROSS_MSGTYPE_ACCOUNT_NONCE);
		channel_.Register(this, protocol::CROSS_MSGTYPE_DO_TRANSACTION);

		return true;
	}

	bool NotaryMgr::Exit(){
		return true;
	}

	void NotaryMgr::OnTimer(int64_t current_time){
		if ((current_time - last_update_time_) < 3 * utils::MICRO_UNITS_PER_SEC){
			return;
		}
		last_update_time_ = current_time;
		update_times_++;

		//3秒更新状态
		for (auto itr = chain_obj_map_.begin(); itr != chain_obj_map_.end(); itr++){
			itr->second->OnFastTimer(current_time);
		}

		//12秒提交提案
		if (update_times_ % 4 == 0){
			for (auto itr = chain_obj_map_.begin(); itr != chain_obj_map_.end(); itr++){
				itr->second->OnSlowTimer(current_time);
			}
		}
	}

	void NotaryMgr::HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message){
		//Check existing in chain obj
		auto itr = chain_obj_map_.find(comm_unique);
		if (itr == chain_obj_map_.end()){
			LOG_ERROR("Can not find chain.");
			return;
		}
		LOG_INFO("Recv comm unique %s message", comm_unique.c_str());
		//Deliver the message to chain obj
		itr->second->OnHandleMessage(message);
	}
}

