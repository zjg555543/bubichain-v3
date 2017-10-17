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


#include "ledger/ledger_manager.h"
#include <glue/glue_manager.h>

namespace bubi {
	LedgerContext::LedgerContext(){
		closing_ledger_ = std::make_shared<LedgerFrm>();
	}
	void LedgerContext::Init(const std::string& hash){
		closing_ledger_->SetContext(shared_from_this());
		hash_ = hash;
	}
	bool LedgerContext::Apply(const protocol::ConsensusValue& consensus_value, int& timeout_tx_index, LedgerFrm::EXECUTE_MODE execute_mode){
		protocol::Ledger& ledger = closing_ledger_->ProtoLedger();
		auto header = ledger.mutable_header();
		header->set_seq(consensus_value.ledger_seq());
		header->set_close_time(consensus_value.close_time());
		header->set_previous_hash(consensus_value.previous_ledger_hash());
		header->set_consensus_value_hash(hash_);
		//LOG_INFO("set_consensus_value_hash:%s,%s", utils::String::BinToHexString(con_str).c_str(), utils::String::BinToHexString(chash).c_str());
		header->set_version(LedgerManager::Instance().GetLastClosedLedger().version());
		int64_t time0 = utils::Timestamp().HighResolution();
		LedgerManager::Instance().tree_->time_ = 0;
		bool result = closing_ledger_->Apply(consensus_value, execute_mode);
		int64_t time1 = utils::Timestamp().HighResolution();
		apply_time_ = time1 - time0;
		timeout_tx_index = closing_ledger_->timeout_tx_index_;
		return result;
	}


	LedgerContextManager::LedgerContextManager()
	{}
	LedgerContextManager::~LedgerContextManager()
	{}

	bool LedgerContextManager::PreProcessLedger(const protocol::ConsensusValue& consensus_value, int& timeout_tx_index, LedgerFrm::EXECUTE_MODE execute_mode){
		utils::MutexGuard guard(mutex_);
		std::shared_ptr<LedgerContext> context;
		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);

		auto it = box_.find(chash);
		if (it == box_.end()){
			context = std::make_shared<LedgerContext>();
			context->Init(chash);
			//box_[box_key] = context;//insert into box after apply successful
			LOG_INFO("Create context in box,ledger_seq(" FMT_I64 ") consensus_value(%s)", consensus_value.ledger_seq(), utils::String::BinToHexString(chash).c_str());
		
			if (!context->Apply(consensus_value, timeout_tx_index, execute_mode)){
				LOG_ERROR("Ledger_seq(" FMT_I64 ") consensus_value(%s) time out in tx(%d),context be deleted", consensus_value.ledger_seq(), utils::String::BinToHexString(chash).c_str(), context->closing_ledger_->timeout_tx_index_);
				return false;
			}
			box_[chash] = context;
		}		
		return true;
	}

	std::shared_ptr<LedgerContext> LedgerContextManager::GetContext(const protocol::ConsensusValue& consensus_value, const bool remove){
		std::shared_ptr<LedgerContext> context = nullptr;
		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);
		utils::MutexGuard guard(mutex_);
		auto it = box_.find(chash);
		if (it != box_.end()){
			context = it->second;
			if (remove)
				box_.erase(it);
		}
		return context;
	}

	std::shared_ptr<LedgerContext> LedgerContextManager::GetContext(const std::string& context_index, const bool remove){
		std::shared_ptr<LedgerContext> context = nullptr;
		utils::MutexGuard guard(mutex_);
		auto it = box_.find(context_index);
		if (it != box_.end()){
			context = it->second;
			if (remove)
				box_.erase(it);
		}
		return context;
	}
}