
#ifndef TRANSACTION_FRM_H_
#define TRANSACTION_FRM_H_

#include <unordered_map>
#include <utils/common.h>
#include <common/general.h>
#include <ledger/account.h>
#include <overlay/peer.h>
#include <api/web_server.h>
#include <proto/cpp/overlay.pb.h>
#include "operation_frm.h"
#include "environment.h"

namespace bubi {

	class OperationFrm;
	class AccountEntry;
	class LedgerFrm;
	class TransactionFrm {
	public:
		typedef std::shared_ptr<bubi::TransactionFrm> pointer;

		std::set<std::string> involved_accounts_;
		std::vector<protocol::TransactionEnvStore> instructions_;
		std::shared_ptr<Environment> environment_;
	public:
		//only valid when the transaction belongs to a txset
		TransactionFrm();
		TransactionFrm(const protocol::TransactionEnv &env);
		
		virtual ~TransactionFrm();
		
		static bool AccountFromDB(const std::string &address, AccountFrm::pointer &account_ptr);

		std::string GetContentHash() const;
		std::string GetContentData() const;
		std::string GetFullHash() const;

		void ToJson(Json::Value &json);

		std::string GetSourceAddress() const;
		int64_t GetNonce() const;

		const protocol::TransactionEnv &GetTransactionEnv() const;
		std::string &GetTransactionString();

		bool CheckValid(int64_t last_seq); 
		bool CheckExpr(const std::string &code, const std::string &log_prefix);

		bool SignerHashPriv(utils::StringVector &address, std::shared_ptr<Environment> env, int32_t type) const;

		const protocol::Transaction &GetTx() const;

		Result GetResult() const;

		void Initialize();

		uint32_t LoadFromDb(const std::string &hash);

		bool CheckTimeout(int64_t expire_time);
		void NonceIncrease(LedgerFrm* ledger_frm, std::shared_ptr<Environment> env);
		bool Apply(LedgerFrm* ledger_frm, std::shared_ptr<Environment> env, bool bool_contract = false);
		bool ApplyExpr(const std::string &code, const std::string &log_prefix);

		protocol::TransactionEnv &GetProtoTxEnv() {
			return transaction_env_;
		}

		bool ValidForParameter();
		
		bool ValidForSourceSignature();

		bool ValidForApply(std::shared_ptr<Environment> environment);

		uint64_t apply_time_;
		int64_t ledger_seq_;
		Result result_;	
		int32_t processing_operation_;
		LedgerFrm* ledger_;

	private:		
		protocol::TransactionEnv transaction_env_;
		std::string hash_;
		std::string full_hash_;
		std::string data_;
		std::string full_data_;
		std::set<std::string> valid_signature_;
		
		int64_t incoming_time_;
		std::string *trans_value;  //accelate the program
	};
};

#endif
