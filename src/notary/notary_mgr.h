
#ifndef NOTARY_MGR_H_
#define NOTARY_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>

namespace bubi {
	//save and control self chain
	typedef enum ExecuteState{
		EXECUTE_STATE_INITIAL = 1,
		EXECUTE_STATE_PROCESSING = 2,
		EXECUTE_STATE_FAIL = 3,
		EXECUTE_STATE_SUCCESS = 4
	};

	class ChainObj{
	public:
		ChainObj(MessageChannel *channel, const std::string &comm_unique, const std::string &notary_address, const std::string &private_key, const std::string &comm_contract);
		~ChainObj();

		typedef std::map<int64_t, protocol::CrossProposalInfo> ProposalInfoMap;
		typedef std::vector<protocol::CrossProposalInfo> ProposalInfoVector;

		typedef struct tagProposalRecord{
			ProposalInfoMap proposal_info_map;
			int64_t max_seq;
			int64_t affirm_max_seq;

			void Reset(){
				proposal_info_map.clear();
				max_seq = -1;
				affirm_max_seq = -1;
			}
		}ProposalRecord;

		void ResetChainInfo() {
			output_record_.Reset();
			input_record_.Reset();
			error_tx_times_ = -1;
			tx_history_.clear();
			memset(&notary_list_, 0, sizeof(notary_list_));
		}

		void SetPeerChain(std::shared_ptr<ChainObj> peer_chain);
		void OnSlowTimer(int64_t current_time);
		void OnFastTimer(int64_t current_time);
		void OnHandleMessage(const protocol::WsMessage &message);
		bool GetProposalInfo(protocol::CROSS_PROPOSAL_TYPE type, int64_t index, protocol::CrossProposalInfo &info);

	private:
		//message handle
		void OnHandleProposalNotice(const protocol::WsMessage &message);
		void OnHandleProposalResponse(const protocol::WsMessage &message);
		void OnHandleCrossCommInfoResponse(const protocol::WsMessage &message);
		void OnHandleAccountNonceResponse(const protocol::WsMessage &message);
		void OnHandleProposalDoTransResponse(const protocol::WsMessage &message);
		void HandleProposalNotice(const protocol::CrossProposalInfo &proposal_info);
		
		//内部函数处理
		void RequestNotaryAccountNonce();
		void RequestCommInfo();
		void RequestAndUpdate(protocol::CROSS_PROPOSAL_TYPE type);
		void CheckTxError();
		void Vote(protocol::CROSS_PROPOSAL_TYPE type);
		void SubmitTransaction();

		ProposalRecord* GetProposalRecord(protocol::CROSS_PROPOSAL_TYPE type); //使用的时候，需要在外部加锁

	private:
		utils::Mutex lock_;
		std::shared_ptr<ChainObj> peer_chain_;
		MessageChannel *channel_;
		std::string comm_unique_;
		std::string comm_contract_;
		std::string notary_address_;
		std::string private_key_;
		int64_t notary_nonce_;
		ProposalInfoVector proposal_info_vector_;

		//Chain Info
		ProposalRecord output_record_;
		ProposalRecord input_record_;
		int64_t error_tx_times_;
		utils::StringList tx_history_;
		std::string notary_list_[100];
	};

	typedef std::map<std::string, std::shared_ptr<ChainObj>> ChainObjMap;

	class NotaryMgr : public utils::Singleton<NotaryMgr>, public TimerNotify, public IMessageHandler{
		friend class utils::Singleton<bubi::NotaryMgr>;
	public:
		NotaryMgr(){}
		~NotaryMgr(){}

		bool Initialize();
		bool Exit();

	private:
		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) override {};

		virtual void HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message) override;

	private:
		ChainObjMap chain_obj_map_;
		MessageChannel channel_;
		int64_t last_update_time_;
		int64_t update_times_;
	};
}

#endif
