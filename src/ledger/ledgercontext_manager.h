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
#ifndef LEDGER_CONTEXT_MANAGER_H_
#define LEDGER_CONTEXT_MANAGER_H_

#include <utils/headers.h>
#include <common/general.h>
#include <proto/cpp/chain.pb.h>
#include "ledger_frm.h"

namespace bubi {

	class LedgerContextManager;
	class LedgerContext;
	typedef std::function< void(bool check_result)> PreProcessCallback;
	class LedgerContext : public utils::Thread {
		std::stack<int64_t> contract_ids_; //may be called by check thread or execute thread.so need lock
		//parameter
		std::string hash_;
		LedgerContextManager *lpmanager_;
		int64_t start_time_;

	public:
		LedgerContext(
			LedgerContextManager *lpmanager,
			const std::string &chash, 
			const protocol::ConsensusValue &consvalue, 
			int64_t tx_timeout,
			PreProcessCallback callback);
		LedgerContext(
			const std::string &chash, 
			const protocol::ConsensusValue &consvalue, 
			int64_t tx_timeout);
		~LedgerContext();

		protocol::ConsensusValue consensus_value_;
		bool sync_;
		PreProcessCallback callback_;
		int64_t tx_timeout_;

		LedgerFrm::pointer closing_ledger_;
		std::stack<std::shared_ptr<TransactionFrm>> transaction_stack_;
		
		//result
		bool exe_result_;
		int32_t timeout_tx_index_;

		utils::Mutex lock_;

		virtual void Run();
		void Do();
		void Cancel();
		bool CheckExpire(int64_t total_timeout);
		void PushContractId(int64_t id);
		void PopContractId();
		int64_t GetTopContractId();
		std::string GetHash();
		int32_t GetTxTimeoutIndex();
	};

	typedef std::multimap<std::string, LedgerContext *> LedgerContextMultiMap;
	typedef std::map<std::string, LedgerContext *> LedgerContextMap;
	class LedgerContextManager :
		public bubi::TimerNotify {
		utils::Mutex ctxs_lock_;
		LedgerContextMultiMap running_ctxs_;
		LedgerContextMap completed_ctxs_;
	public:
		LedgerContextManager();
		~LedgerContextManager();

		void Initialize();
		virtual void OnTimer(int64_t current_time);
		virtual void OnSlowTimer(int64_t current_time);
		void MoveRunningToComplete(LedgerContext *ledger_context);
		void RemoveCompleted(int64_t ledger_seq);
		void GetModuleStatus(Json::Value &data);

		//<0 : notfound 1: found and success 0: found and failed
		int32_t CheckComplete(const std::string &chash);
		bool SyncPreProcess(const protocol::ConsensusValue& consensus_value, int64_t timeout, int32_t &timeout_tx_index);

		//<0 : processing 1: found and success 0: found and failed
		int32_t AsyncPreProcess(const protocol::ConsensusValue& consensus_value, int64_t timeout, PreProcessCallback callback, int32_t &timeout_tx_index);
		LedgerFrm::pointer SyncProcess(const protocol::ConsensusValue& consensus_value); //for ledger closing
	};

}
#endif //end of ifndef