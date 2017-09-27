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

#include <memory>

namespace bubi {
	class TransactionFrm;
	class LedgerFrm;
	class LedgerContext :public std::enable_shared_from_this<LedgerContext>
	{
	public:
		typedef std::shared_ptr<bubi::LedgerContext>	pointer;
		std::string hash_;
		int64_t apply_time_;
		LedgerFrm::pointer closing_ledger_;
		std::stack<std::shared_ptr<TransactionFrm>> transaction_stack_;
		LedgerContext();
		void Init(const std::string& hash);
	};

	class LedgerContextManager
	{
	public:
		
		typedef std::shared_ptr<bubi::LedgerContextManager>	pointer;

		LedgerContextManager();
		~LedgerContextManager();
		bool PreProcessLedger(const protocol::ConsensusValue& consensus_value, int& timeout_tx_index, LedgerFrm::EXECUTE_MODE execute_mode = LedgerFrm::EXECUTE_MODE::EM_TIMEOUT);
		std::shared_ptr<LedgerContext> GetContext(const protocol::ConsensusValue& consensus_value, const bool remove = false);
		std::shared_ptr<LedgerContext> GetContext(const std::string& context_index, const bool remove = false);
	private:
		utils::Mutex mutex_;
		std::unordered_map<std::string, std::shared_ptr<LedgerContext>> box_;
	};

}
#endif //end of ifndef