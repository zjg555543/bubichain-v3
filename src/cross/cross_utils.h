
#ifndef CROSS_UTILS_H_
#define CROSS_UTILS_H_
#include<ledger/ledger_manager.h>

namespace bubi {
	class CrossUtils : public utils::NonCopyable{
	public:
		CrossUtils() {}
		~CrossUtils() {}
		static int32_t QueryContract(const std::string &address, const std::string &input, Json::Value &query_rets);
	};

	struct TransTask{
		std::vector<std::string> input_paras_;
		int32_t amount_;
		std::string dest_address_;
		std::string user_defined_;

		TransTask(){
			input_paras_.clear();
			amount_ = 0;
			dest_address_.clear();
			user_defined_.clear();
		}

		TransTask(const std::vector<std::string> &input_paras, int32_t amount, const std::string &dest_address, const std::string &user_defined){
			input_paras_ = input_paras;
			amount_ = amount;
			dest_address_ = dest_address;
			user_defined_ = user_defined;
		}

	};
	typedef std::vector<TransTask> TransTaskVector;

	struct TransTaskResult{
		bool result_;
		std::string desc_;
		std::string hash_;

		TransTaskResult(){
			result_ = false;
			desc_.clear();
			hash_.clear();
		}
		TransTaskResult(bool result, const std::string &desc, const std::string &hash){
			result_ = result;
			desc_ = desc;
			hash_ = hash;
		}
	};

	class ITransactionSenderNotify{
	public:
		virtual void HandleTransactionSenderResult(const TransTask &task_task, const TransTaskResult &task_result) = 0;
	};

	class TransactionSender : public utils::Singleton<TransactionSender>,
		public utils::Runnable{
		friend class utils::Singleton<TransactionSender>;

	public:
		TransactionSender();
		~TransactionSender();

		bool Initialize(const std::string &private_key);
		bool Exit();

		void AsyncSendTransaction(ITransactionSenderNotify *notify, const TransTask &tans_task);

	private:
		virtual void Run(utils::Thread *thread) override;
		void SendingAll();
		TransTaskResult SendingSingle(const std::vector<std::string> &paras, const std::string &dest);

		TransactionFrm::pointer BuildTransaction(const std::string &private_key, const std::string &dest, const std::vector<std::string> &paras, int64_t nonce);
		int32_t SendTransaction(TransactionFrm::pointer tran_ptr);

		bool enabled_;
		utils::Thread *thread_ptr_;

		int64_t cur_nonce_;
		int64_t last_update_time_;
		std::string source_address_;
		std::string private_key_;

		utils::Mutex task_vector_lock_;
		typedef std::map<ITransactionSenderNotify*, TransTaskVector> TransTaskMap;
		TransTaskMap trans_task_map_;
	};
}

#endif