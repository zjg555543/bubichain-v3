
#ifndef NOTARY_MGR_H_
#define NOTARY_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>

namespace bubi {
	//save and control self chain
	class ChainObj{
	public:
		ChainObj(MessageChannel *channel, const std::string &comm_unique, const std::string &notary_address, const std::string &private_key);
		~ChainObj();

		typedef std::map<int64_t, protocol::CrossProposalInfo> ProposalInfoMap;
		typedef std::vector<protocol::CrossProposalInfo> ProposalInfoVector;

		typedef struct tagProposal{
			ProposalInfoMap proposal_info_map;
			int64_t recv_max_seq;
			int64_t affirm_max_seq;
			utils::StringVector asset_vector;

			void Reset(){
				proposal_info_map.clear();
				recv_max_seq = -1;
				affirm_max_seq = -1;
			}
		}Proposal;

		typedef std::map<std::string, Proposal> ProposalMap;

		typedef struct tagChainInfo
		{
			int64_t nonce;
			ProposalMap output_map;
			ProposalMap input_map;
			int64_t error_tx_times;
			utils::StringList tx_history;
			std::string notary_list[100];
			
		public:
			void Reset() {
				output_map.clear();
				input_map.clear();
				error_tx_times = -1;
				tx_history.clear();
				memset(&notary_list, 0, sizeof(notary_list));
				nonce = -1;
			}
		}ChainInfo;

		void SetPeerChain(std::shared_ptr<ChainObj> peer_chain);
		void OnTimer(int64_t current_time);
		void OnHandleMessage(const protocol::WsMessage &message);
		ChainInfo GetChainInfo();

	private:
		//message handle
		void OnHandleProposalNotice(const protocol::WsMessage &message);
		void OnHandleProposalResponse(const protocol::WsMessage &message);
		void OnHandleCrossCommInfoResponse(const protocol::WsMessage &message);
		void OnHandleAccountNonceResponse(const protocol::WsMessage &message);
		void OnHandleProposalDoTransResponse(const protocol::WsMessage &message);
		void HandleProposalNotice(const protocol::CrossProposalInfo &proposal_info);
		
		//内部函数处理
		void RequestAndSort(protocol::CROSS_PROPOSAL_TYPE type);
		void CheckTxError();
		void Vote(protocol::CROSS_PROPOSAL_TYPE type);
		void SubmitTransaction();

	private:
		ChainInfo chain_info_;
		utils::Mutex lock_;
		std::shared_ptr<ChainObj> peer_chain_;
		MessageChannel *channel_;
		std::string comm_unique_;
		std::string notary_address_;
		std::string private_key_;
		ProposalInfoVector proposal_info_vector_;
	};

	typedef std::map<std::string, std::shared_ptr<ChainObj>> ChainObjMap;

	class NotaryMgr : public utils::Singleton<NotaryMgr>, public TimerNotify, public IMessageHandler{
		friend class utils::Singleton<bubi::NotaryMgr>;
	public:
		NotaryMgr();
		~NotaryMgr();

		bool Initialize();
		bool Exit();

	private:
		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) override {};

		virtual void HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message) override;

	private:
		ChainObjMap chain_obj_map_;
		MessageChannel channel_;
	};
}

#endif
