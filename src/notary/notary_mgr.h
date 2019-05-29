
#ifndef NOTARY_MGR_H_
#define NOTARY_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>

namespace bubi {
	class ChainObj{
	public:
		ChainObj();
		~ChainObj();

		void SetPeerChain(ChainObj *peer_chain);
		void OnTimer(int64_t current_time);
		//void SetChainInfo();
		void SetChainObjId();

	private:
		ChainObj *peer_chain_;
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

		virtual void HandleMessage(const std::string &chain_unique, int64_t msg_type, bool request, const std::string &data) override;

		//inside function
		void OnHandleProposal(const std::string &data);
		void OnHandleProposalResponse(const std::string &data);

	private:
		ChainObj a_chain_obj_;
		ChainObj b_chain_obj_;

		MessageChannel channel_;
	};
}

#endif
