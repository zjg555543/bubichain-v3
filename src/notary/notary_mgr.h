
#ifndef NOTARY_MGR_H_
#define NOTARY_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>

namespace bubi {
	//save and control self chain
	class ChainObj{
	public:
		ChainObj();
		~ChainObj();

		typedef std::map<int64_t, protocol::CrossProposalInfo> CrossProposalInfoMap;
		typedef struct tagChainInfo
		{
			std::string pub_key;
			int64_t nonce;
			CrossProposalInfoMap out_map;
			CrossProposalInfoMap input_map;
			int64_t output_max_seq;
			int64_t input_max_seq;
			int64_t error_tx_times;
			utils::StringList tx_history;
			
		public:
			void Reset() {
				out_map.clear();
				input_map.clear();
				output_max_seq = -1;
				input_max_seq = -1;
				error_tx_times = -1;
				tx_history.clear();
				nonce = -1;
			}
		}ChainInfo;

		void SetPeerChain(ChainObj *peer_chain);
		void OnTimer(int64_t current_time);
		void SetChainInfo(const std::string &comm_unique, const std::string &target_comm_unique);
		std::string GetChainUnique() const { return comm_unique_; };
		void OnHandleMessage(const protocol::WsMessage &message);

	private:
		//message handle
		void OnHandleProposalNotice(const protocol::WsMessage &message);
		void OnHandleProposalResponse(const protocol::WsMessage &message);
		void OnHandleNotarysResponse(const protocol::WsMessage &message);
		void OnHandleAccountNonceResponse(const protocol::WsMessage &message);
		void OnHandleProposalDoTransResponse(const protocol::WsMessage &message);
		void HandleProposalNotice(const protocol::CrossProposalInfo &proposal_info);
		
		//内部函数处理
		void VoteOutPut();
		void VoteInPut();

		ChainObj *peer_chain_;
		std::string comm_unique_;
		std::string target_comm_unique_;
	};

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
		ChainObj a_chain_obj_;
		ChainObj b_chain_obj_;

		MessageChannel channel_;
	};
}

#endif
