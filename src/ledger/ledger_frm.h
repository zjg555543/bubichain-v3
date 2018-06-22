
#ifndef LEDGER_FRM_H_
#define LEDGER_FRM_H_

#include <utils/utils.h>
#include <proto/cpp/monitor.pb.h>
#include "transaction_frm.h"
#include "glue/transaction_set.h"
#include "account.h"

namespace bubi {
	class AccountEntry;
	class LedgerFrm {
	public:
		typedef std::shared_ptr <LedgerFrm>	pointer;

		LedgerFrm();
		~LedgerFrm();

		protocol::LedgerHeader GetProtoHeader() const {
			return ledger_.header();
		}

		protocol::Ledger &ProtoLedger();


		bool Apply(const protocol::ConsensusValue& request);

		// void GetSqlTx(std::string &sqltx, std::string &sql_account_tx);

        bool AddToDb(WRITE_BATCH& batch);

		bool LoadFromDb(int64_t seq);

		//static bool LoadFromDb(int64_t seq, protocol::Ledger &ledger);
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

		std::string &GetConsensusValueString();
	private:
		protocol::Ledger ledger_;
	public:
		std::shared_ptr<protocol::ConsensusValue> value_;
		std::vector<TransactionFrm::pointer> apply_tx_frms_;
		std::vector<TransactionFrm::pointer> dropped_tx_frms_;
		std::string sql_;
		std::shared_ptr<Environment> environment_;

		std::string consensus_value_string_;
	};
}
#endif //end of ifndef
