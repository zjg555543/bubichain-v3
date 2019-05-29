
#ifndef CROSS_CHAIN_MGR_H_
#define CROSS_CHAIN_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>
#include <cross/cross_utils.h>
#include <ledger/ledger_frm.h>

namespace bubi {

	class BlockListener {
	public:
		BlockListener(MessageChannel &channel);
		~BlockListener();

		void HandleBlock(LedgerFrm::pointer closing_ledger);
	private:
		MessageChannel *channel_;
	};

	class MessageHandler : public IMessageHandler{
	public:
		MessageHandler(){}
		~MessageHandler(){}

		virtual void HandleMessage(const std::string &chain_unique, int64_t msg_type, bool request, const std::string &data) override;

		void OnHandleProposal(const std::string &data);
		void OnHandleProposalResponse(const std::string &data);
	};

	class CrossChainMgr : public utils::Singleton<CrossChainMgr>{
		friend class utils::Singleton<bubi::CrossChainMgr>;
	public:
		CrossChainMgr();
		~CrossChainMgr();

		bool Initialize();
		bool Exit();
		void HandleBlock(LedgerFrm::pointer closing_ledger);

	private:
		MessageChannel channel_;
		MessageHandler handler_;
		BlockListener *block_listener_;
	};
}

#endif
