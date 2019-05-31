
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

	class MessageHandler{
	public:
		MessageHandler(MessageChannel *channel);
		~MessageHandler(){}

		void OnHandleProposal(const protocol::WsMessage &message);
		void OnHandleNotarys(const protocol::WsMessage &message);
		void OnHandleAccountNonce(const protocol::WsMessage &message);
		void OnHandleDoTransaction(const protocol::WsMessage &message);

	private:
		MessageChannel *channel_;
	};

	class CrossChainMgr : public utils::Singleton<CrossChainMgr>, public IMessageHandler{
		friend class utils::Singleton<bubi::CrossChainMgr>;
	public:
		CrossChainMgr();
		~CrossChainMgr();

		bool Initialize();
		bool Exit();
		void HandleBlock(LedgerFrm::pointer closing_ledger);

	private:
		virtual void HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message) override;

	private:
		MessageChannel channel_;
		MessageHandler *handler_;
		BlockListener *block_listener_;
	};
}

#endif
