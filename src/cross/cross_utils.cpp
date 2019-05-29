
#include <glue/glue_manager.h>
#include <overlay/peer_manager.h>
#include <utils/base_int.h>
#include "cross_utils.h"

#define MAX_SEND_TRANSACTION_TIMES 50

namespace bubi {

	int32_t CrossUtils::QueryContract(const std::string &address, const std::string &input, Json::Value &query_rets){
		return 0;
	}

	TransactionSender::TransactionSender(){
		enabled_ = false;
		thread_ptr_ = NULL;
		cur_nonce_ = 0;
		last_update_time_ = utils::Timestamp::HighResolution();
	}
	TransactionSender::~TransactionSender(){
		if (thread_ptr_){
			delete thread_ptr_;
			thread_ptr_ = NULL;
		}
	}

	bool TransactionSender::Initialize(const std::string &private_key){
		enabled_ = true;
		thread_ptr_ = new utils::Thread(this);
		if (!thread_ptr_->Start("TransactionSender")) {
			return false;
		}

		PrivateKey pkey(private_key);
		if (!pkey.IsValid()){
			LOG_ERROR("Private key is not valid");
			return false;
		}

		private_key_ = private_key;
		source_address_ = pkey.GetBase16PublicKey();
		return true;
	}

	bool TransactionSender::Exit(){
		enabled_ = false;
		if (thread_ptr_) {
			thread_ptr_->JoinWithStop();
		}
		return true;
	}

	void TransactionSender::AsyncSendTransaction(ITransactionSenderNotify *notify, const TransTask &trans_task){
		assert(trans_task.amount_ >= 0);
		assert(!trans_task.dest_address_.empty());
		assert(!trans_task.input_paras_.empty());
		assert(notify != nullptr);
		
		utils::MutexGuard guard(task_vector_lock_);
		TransTaskVector &task_vector = trans_task_map_[notify];
		task_vector.push_back(trans_task);
	}
	void TransactionSender::Run(utils::Thread *thread){
		while (enabled_){
			utils::Sleep(10);
			int64_t current_time = utils::Timestamp::HighResolution();
			if ((current_time - last_update_time_) <= 5 * utils::MICRO_UNITS_PER_SEC){
				continue;
			}

			SendingAll();
			last_update_time_ = current_time;
		}
	}

	void TransactionSender::SendingAll(){
		TransTaskMap temp_map;
		do
		{
			utils::MutexGuard guard(task_vector_lock_);
			temp_map.swap(trans_task_map_);
			trans_task_map_.clear();
			assert(trans_task_map_.size() == 0);
		} while (false);
		if (temp_map.empty()){
			return;
		}

		AccountFrm::pointer account_ptr;
		if (!Environment::AccountFromDB(source_address_, account_ptr)) {
			LOG_ERROR("Address:%s not exsit", source_address_.c_str());
			return;
		}
		cur_nonce_ = account_ptr->GetAccountNonce() + 1;
		
		for (TransTaskMap::const_iterator itr = temp_map.begin(); itr != temp_map.end(); itr++){
			ITransactionSenderNotify* notify = itr->first;
			const TransTaskVector &task_vector = itr->second;
			for (uint32_t i = 0; i < task_vector.size(); i++){
				const TransTask &trans_task = task_vector[i];
				TransTaskResult task_result = SendingSingle(trans_task.input_paras_, trans_task.dest_address_);
				notify->HandleTransactionSenderResult(trans_task, task_result);
			}
		}
	}

	TransTaskResult TransactionSender::SendingSingle(const std::vector<std::string> &paras, const std::string &dest){
		int32_t err_code = 0;

		for (int i = 0; i <= MAX_SEND_TRANSACTION_TIMES; i++){
			TransactionFrm::pointer trans = BuildTransaction(private_key_, dest, paras, cur_nonce_);
			if (nullptr == trans){
				LOG_ERROR("Trans pointer is null");
				continue;
			}
			std::string hash = utils::String::BinToHexString(trans->GetContentHash().c_str());
			err_code = SendTransaction(trans);
			switch (err_code)
			{
				case protocol::ERRCODE_SUCCESS:
				case protocol::ERRCODE_ALREADY_EXIST:{
					cur_nonce_++;
					TransTaskResult task_result(true, "", hash);
					return task_result;
				}
				case protocol::ERRCODE_BAD_SEQUENCE:{
					cur_nonce_++;
					continue;
				}
				default:{
					LOG_ERROR("Send transaction erro code:%d", err_code);
					continue;
				}
			}

			utils::Sleep(10);
		}

		TransTaskResult task_result(false, "Try MAX_SEND_TRANSACTION_TIMES times", "");
		return task_result;
	}

	TransactionFrm::pointer TransactionSender::BuildTransaction(const std::string &private_key, const std::string &dest, const std::vector<std::string> &paras, int64_t nonce){
		PrivateKey pkey(private_key);
		if (!pkey.IsValid()){
			LOG_ERROR("Private key is not valid");
			return nullptr;
		}

		std::string source_address = pkey.GetBase16PublicKey();

		protocol::TransactionEnv tran_env;
		protocol::Transaction *tran = tran_env.mutable_transaction();

		tran->set_source_address(source_address);
		tran->set_nonce(nonce);
		for (unsigned i = 0; i < paras.size(); i++){
			protocol::Operation *ope = tran->add_operations();
			ope->set_type(protocol::Operation_Type_PAY_COIN);
			protocol::OperationPayCoin *pay_coin = ope->mutable_pay_coin();
			pay_coin->set_amount(0);
			pay_coin->set_dest_address(dest);
			pay_coin->set_input(paras[i]);
		}

		std::string content = tran->SerializeAsString();
		std::string sign = pkey.Sign(content);
		protocol::Signature *signpro = tran_env.add_signatures();
		signpro->set_sign_data(sign);
		signpro->set_public_key(pkey.GetBase16PublicKey());

		std::string tx_hash = utils::String::BinToHexString(HashWrapper::Crypto(content)).c_str();
		LOG_INFO("Pay coin tx hash %s", tx_hash.c_str());

		TransactionFrm::pointer ptr = std::make_shared<TransactionFrm>(tran_env);
		return ptr;
	}

	int32_t TransactionSender::SendTransaction(TransactionFrm::pointer tran_ptr) {
		Result result;
		GlueManager::Instance().OnTransaction(tran_ptr, result);
		if (result.code() != 0) {
			LOG_ERROR("Pay coin result code:%d, des:%s", result.code(), result.desc().c_str());
			return result.code();
		}

		PeerManager::Instance().Broadcast(protocol::OVERLAY_MSGTYPE_TRANSACTION, tran_ptr->GetProtoTxEnv().SerializeAsString());
		return protocol::ERRCODE_SUCCESS;
	}
}