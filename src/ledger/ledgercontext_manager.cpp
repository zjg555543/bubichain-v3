/*
Copyright Bubi Technologies Co., Ltd. 2017 All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "ledgercontext_manager.h"
#include "ledger_manager.h"
#include "contract_manager.h"

namespace bubi {

	LedgerContext::LedgerContext(const std::string &chash, const protocol::ConsensusValue &consvalue, int64_t timeout) :
		lpmanager_(NULL),
		hash_(chash),
		consensus_value_(consvalue),
		start_time_(-1),
		exe_result_(false),
		sync_(true),
		tx_timeout_(timeout),
		timeout_tx_index_(-1){
		closing_ledger_ = std::make_shared<LedgerFrm>();
	}

	LedgerContext::LedgerContext(LedgerContextManager *lpmanager, const std::string &chash, const protocol::ConsensusValue &consvalue, int64_t timeout, PreProcessCallback callback) :
		lpmanager_(lpmanager),
		hash_(chash),
		consensus_value_(consvalue),
		start_time_(-1),
		exe_result_(false),
		sync_(false),
		callback_(callback),
		tx_timeout_(timeout),
		timeout_tx_index_(-1) {
			closing_ledger_ = std::make_shared<LedgerFrm>();
		}
	LedgerContext::~LedgerContext() {}

	void LedgerContext::Run() {
		LOG_INFO("Thread preprocessing the consensus value, ledger seq(" FMT_I64 ")", consensus_value_.ledger_seq());
		start_time_ = utils::Timestamp::HighResolution();
		Do();
	}

	void LedgerContext::Do() {
		protocol::Ledger& ledger = closing_ledger_->ProtoLedger();
		auto header = ledger.mutable_header();
		header->set_seq(consensus_value_.ledger_seq());
		header->set_close_time(consensus_value_.close_time());
		header->set_previous_hash(consensus_value_.previous_ledger_hash());
		header->set_consensus_value_hash(hash_);
		//LOG_INFO("set_consensus_value_hash:%s,%s", utils::String::BinToHexString(con_str).c_str(), utils::String::BinToHexString(chash).c_str());
		header->set_version(LedgerManager::Instance().GetLastClosedLedger().version());
		LedgerManager::Instance().tree_->time_ = 0;
		exe_result_ = closing_ledger_->Apply(consensus_value_, this, tx_timeout_, timeout_tx_index_);

		if (!sync_){
			callback_(exe_result_);
		}
		//move running to complete
		if (lpmanager_){
			lpmanager_->MoveRunningToComplete(this);
		}
	}

	void LedgerContext::Cancel() {
		std::stack<int64_t> copy_stack;
		do {
			utils::MutexGuard guard(lock_);
			copy_stack = contract_ids_;
		} while (false);

		while (!copy_stack.empty()) {
			int64_t cid = copy_stack.top();
			ContractManager::Instance().Cancel(cid);
			copy_stack.pop();
		}

		JoinWithStop();
	}

	bool LedgerContext::CheckExpire(int64_t total_timeout) {
		return utils::Timestamp::HighResolution() - start_time_ >= total_timeout;
	}

	void LedgerContext::PushContractId(int64_t id) {
		utils::MutexGuard guard(lock_);
		contract_ids_.push(id);
	}

	void LedgerContext::PopContractId() {
		utils::MutexGuard guard(lock_);
		contract_ids_.pop();
	}

	int64_t LedgerContext::GetTopContractId() {
		utils::MutexGuard guard(lock_);
		if (!contract_ids_.empty()) {
			return contract_ids_.top();
		}

		return -1;
	}

	std::string LedgerContext::GetHash() {
		return hash_;
	}

	int32_t LedgerContext::GetTxTimeoutIndex() {
		return timeout_tx_index_;
	}

	LedgerContextManager::LedgerContextManager() {
		check_interval_ = 10 * utils::MICRO_UNITS_PER_MILLI;
	}
	LedgerContextManager::~LedgerContextManager() {
	}

	void LedgerContextManager::Initialize() {
		TimerNotify::RegisterModule(this);
	}

	int32_t LedgerContextManager::CheckComplete(const std::string &chash) {
		do {
			utils::MutexGuard guard(ctxs_lock_);
			LedgerContextMap::iterator iter = completed_ctxs_.find(chash);
			if (iter != completed_ctxs_.end()) {
				return iter->second->exe_result_ ? 1 : 0;
			}
		} while (false);

		return -1;
	}

	LedgerFrm::pointer LedgerContextManager::SyncProcess(const protocol::ConsensusValue& consensus_value) {
		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);
		do {
			utils::MutexGuard guard(ctxs_lock_);
			LedgerContextMap::iterator iter = completed_ctxs_.find(chash);
			if (iter != completed_ctxs_.end()) {
				return iter->second->closing_ledger_;
			}
		} while (false);

		LOG_INFO("Syn processing the consensus value, ledger seq(" FMT_I64 ")", consensus_value.ledger_seq());
		LedgerContext ledger_context(chash, consensus_value, -1);
		ledger_context.Do();
		return ledger_context.closing_ledger_;
	}

	int32_t LedgerContextManager::AsyncPreProcess(const protocol::ConsensusValue& consensus_value,
		int64_t timeout, 
		PreProcessCallback callback,
		int32_t &timeout_tx_index) {

		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);

		int32_t check_complete = CheckComplete(chash);
		if (check_complete > 0) {
			return check_complete;
		}

		LedgerContext *ledger_context = new LedgerContext(this, chash, consensus_value, utils::MICRO_UNITS_PER_SEC, callback);
		do {
			utils::MutexGuard guard(ctxs_lock_);
			running_ctxs_.insert(std::make_pair(chash, ledger_context));
		} while (false);

		if (!ledger_context->Start("process-value")) {
			LOG_ERROR_ERRNO("Start process value thread failed, consvalue hash(%s)", utils::String::BinToHexString(chash).c_str(),
				STD_ERR_CODE, STD_ERR_DESC);
			
			utils::MutexGuard guard(ctxs_lock_);
			for (LedgerContextMultiMap::iterator iter = running_ctxs_.begin(); 
				iter != running_ctxs_.end();
				iter++) {
				if (iter->second == ledger_context) {
					running_ctxs_.erase(iter);
					delete ledger_context;
				} 
			}

			timeout_tx_index = -1;
			return 0;
		}

		return -1;
	}

	bool LedgerContextManager::SyncPreProcess(const protocol::ConsensusValue &consensus_value,
		int64_t total_timeout, 
		int32_t &timeout_tx_index) {

		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);

		int32_t check_complete = CheckComplete(chash);
		if (check_complete > 0 ){
			return check_complete == 1;
		} 

		LedgerContext *ledger_context = new LedgerContext(this, chash, consensus_value, utils::MICRO_UNITS_PER_SEC, [](bool) {});

		if (!ledger_context->Start("process-value")) {
			LOG_ERROR_ERRNO("Start process value thread failed, consvalue hash(%s)", utils::String::BinToHexString(chash).c_str(), 
				STD_ERR_CODE, STD_ERR_DESC);

			timeout_tx_index = -1;
			delete ledger_context;
			return false;
		}

		int64_t time_start = utils::Timestamp::HighResolution();
		bool is_timeout = false;
		while (ledger_context->IsRunning()) {
			utils::Sleep(10);
			if (utils::Timestamp::HighResolution() - time_start > total_timeout) {
				is_timeout = true;
				break;
			}
		}

		if (is_timeout){ //cancel it
			ledger_context->Cancel();
			timeout_tx_index = ledger_context->GetTxTimeoutIndex();
			LOG_ERROR("Pre execute consvalue time(" FMT_I64 "ms) is out, timeout tx index(%d)", total_timeout / utils::MICRO_UNITS_PER_MILLI, timeout_tx_index);
			return false;
		}

		return true;
	}

	void LedgerContextManager::RemoveCompleted(int64_t ledger_seq) {
		utils::MutexGuard guard(ctxs_lock_);
		for (LedgerContextMap::iterator iter = completed_ctxs_.begin();
			iter != completed_ctxs_.end();
			) {
			if (iter->second->consensus_value_.ledger_seq() <= ledger_seq) {
				delete iter->second;
				completed_ctxs_.erase(iter++);
			}
			else {
				iter++;
			}
		}
	}

	void LedgerContextManager::GetModuleStatus(Json::Value &data) {
		utils::MutexGuard guard(ctxs_lock_);
		data["completed_size"] = completed_ctxs_.size();
		data["running_size"] = running_ctxs_.size();
	}

	void LedgerContextManager::OnTimer(int64_t current_time) {

		std::vector<LedgerContext *> expired_context;
		do {
			utils::MutexGuard guard(ctxs_lock_);
			for (LedgerContextMultiMap::iterator iter = running_ctxs_.begin(); 
				iter != running_ctxs_.end();
				iter++) {
				if (iter->second->CheckExpire( 5 * utils::MICRO_UNITS_PER_SEC)){
					expired_context.push_back(iter->second);
				} 
			}
			 
		} while (false);

		for (size_t i = 0; i < expired_context.size(); i++) {
			expired_context[i]->Cancel();
		}
	}

	void LedgerContextManager::OnSlowTimer(int64_t current_time) {}

	void LedgerContextManager::MoveRunningToComplete(LedgerContext *ledger_context) {
		utils::MutexGuard guard(ctxs_lock_);
		for (LedgerContextMultiMap::iterator iter = running_ctxs_.begin();
			iter != running_ctxs_.end();
			) {
			if (iter->second == ledger_context) {
				running_ctxs_.erase(iter++);
			}
			else {
				iter++;
			}
		}

		//LOG_ERROR("Push hash(%s)", utils::String::Bin4ToHexString(ledger_context->GetHash()).c_str());
		completed_ctxs_.insert(std::make_pair(ledger_context->GetHash(), ledger_context));
	}
}