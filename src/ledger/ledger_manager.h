
#ifndef LEDGER_MANAGER_H_
#define LEDGER_MANAGER_H_

#include <utils/headers.h>
#include <utils/exprparser.h>
#include <utils/entry_cache.h>
#include <common/general.h>
#include <common/storage.h>
#include <common/private_key.h>
#include <main/configure.h>
#include <overlay/peer.h>
#include "ledger/ledger_frm.h"
#include "environment.h"
#include "kv_trie.h"
#include "proto/cpp/consensus.pb.h"

#ifdef WIN32
#include <leveldb/leveldb.h>
#include <leveldb/slice.h>
#else
#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#endif

namespace bubi {
	class LedgerFetch;
	class ContractManager;
	class LedgerManager : public utils::Singleton<bubi::LedgerManager>,
		public bubi::TimerNotify,
		public bubi::StatusModule {
		friend class utils::Singleton<bubi::LedgerManager>;
		friend class LedgerFetch;
	public:

		bool Initialize();
		bool Exit();

		int OnConsent(const protocol::ConsensusValue &value, const std::string& proof);

		protocol::LedgerHeader GetLastClosedLedger();

		int GetAccountNum();

		void OnRequestLedgers(const protocol::GetLedgers &message, int64_t peer_id);

		void OnReceiveLedgers(const protocol::Ledgers &message, int64_t peer_id);

		bool GetValidators(int64_t seq, protocol::ValidatorSet& validators_set);

		bool ConsensusValueFromDB(int64_t seq, protocol::ConsensusValue& request);

		bool DoTransaction(protocol::TransactionEnv& env);

		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) override;
		virtual void GetModuleStatus(Json::Value &data);

		static void CreateHardforkLedger();

	public:
		utils::Mutex gmutex_;
		Json::Value statistics_;
		LedgerFrm::pointer closing_ledger_;
		//std::shared_ptr<TransactionFrm> execute_transaction_;
		std::stack<std::shared_ptr<TransactionFrm>> transaction_stack_;
		KVTrie* tree_;
	private:
		LedgerManager();
		~LedgerManager();

		void RequestConsensusValues(int64_t pid, protocol::GetLedgers& gl, int64_t time);

		int64_t GetMaxLedger();

		bool CloseLedger(const protocol::ConsensusValue& request, const std::string& proof);

		bool CreateGenesisAccount();

		static void ValidatorsSet(std::shared_ptr<WRITE_BATCH> batch, const protocol::ValidatorSet& validators);
		static bool ValidatorsGet(const std::string& hash, protocol::ValidatorSet& vlidators_set);
		
		LedgerFrm::pointer last_closed_ledger_;
		protocol::ValidatorSet validators_;
		std::string proof_;

		utils::ReadWriteLock lcl_header_mutex_;
		protocol::LedgerHeader lcl_header_;

		struct SyncStat{
			int64_t send_time_;
			protocol::GetLedgers gl_;
			int64_t probation_; //
			SyncStat(){
				send_time_ = 0;
				probation_ = 0;
			}
			Json::Value ToJson(){
				Json::Value v;
				v["send_time"] = send_time_;
				v["probation"] = probation_;
				v["gl"] = Proto2Json(gl_);
				return v;
			}
		};
		
		struct Sync{
			int64_t update_time_;
			/*std::map<int64_t, int> buffer_;*/
			std::map<int64_t, SyncStat> peers_;
			Sync(){
				update_time_ = 0;
			}
			Json::Value ToJson(){
				Json::Value v;
				v["update_time"] = update_time_;
				Json::Value& peers = v["peers"];
				for (auto it = peers_.begin(); it != peers_.end(); it++){
					Json::Value tmp = it->second.ToJson();
					tmp["pid"] = it->first;
					peers[peers.size()] = tmp;
				}
				return v;
			}
		};

		Sync sync_;
	};

	class ExprCondition : public utils::ExprParser {
		std::shared_ptr<Environment> environment_;
		const protocol::ConsensusValue &cons_value_;
	public:
		ExprCondition(const std::string & program, std::shared_ptr<Environment> env, const protocol::ConsensusValue &cons_value);
		~ExprCondition();

		static void RegisterFunctions();
		static const utils::ExprValue DoLedger(const utils::ExprValue &arg, utils::ExprParser *parser);
		static const utils::ExprValue DoAccount(const utils::ExprValue &arg, utils::ExprParser *parser);
		static const utils::ExprValue DoJsonPath(const utils::ExprValue &arg1, const utils::ExprValue &arg2, utils::ExprParser *parser);
		Result Eval(utils::ExprValue &value);
		Result Parse(utils::ExprValue &value);
		std::shared_ptr<Environment> GetEnviroment();
	};

}
#endif

