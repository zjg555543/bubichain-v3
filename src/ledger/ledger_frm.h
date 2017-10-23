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

#ifndef LEDGER_FRM_H_
#define LEDGER_FRM_H_

#include <utils/utils.h>
#include <proto/cpp/monitor.pb.h>
#include "transaction_frm.h"
#include "glue/transaction_set.h"
#include "account.h"

namespace bubi {
	class AccountEntry;
	class LedgerContext;
	class LedgerFrm {
	public:
		typedef std::shared_ptr <LedgerFrm>	pointer;

		LedgerFrm();
		~LedgerFrm();

		protocol::LedgerHeader GetProtoHeader() const {
		  	return ledger_.header();
		}

		protocol::Ledger &ProtoLedger();


		bool Apply(const protocol::ConsensusValue& request, 
			LedgerContext *ledger_context, 
			int64_t tx_time_out, 
			int32_t &tx_time_out_index);
		bool Cancel();

		// void GetSqlTx(std::string &sqltx, std::string &sql_account_tx);

		bool AddToDb(WRITE_BATCH& batch);

		bool LoadFromDb(int64_t seq);

		size_t GetTxCount() {
			return apply_tx_frms_.size();
		}

		size_t GetTxOpeCount() {
			size_t ope_count = 0;
			for (size_t i = 0; i < apply_tx_frms_.size(); i++) {
				const protocol::Transaction &tx = apply_tx_frms_[i]->GetTransactionEnv().transaction();
				ope_count += (tx.operations_size() > 0 ? tx.operations_size() : 1);
			}
			return ope_count;
		}

		bool CheckValidation ();

		Json::Value ToJson();

		bool Commit(KVTrie* trie, int64_t& new_count, int64_t& change_count);
	private:
		int64_t id_;
		protocol::Ledger ledger_;
	public:
		std::shared_ptr<protocol::ConsensusValue> value_;
		std::vector<TransactionFrm::pointer> apply_tx_frms_;
		std::string sql_;
		std::shared_ptr<Environment> environment_;
		LedgerContext *lpledger_context_;
		int64_t apply_time_;
		bool enabled_;
	};
}
#endif //end of ifndef
