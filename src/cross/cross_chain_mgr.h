
#ifndef CROSS_CHAIN_MGR_H_
#define CROSS_CHAIN_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>
#include <cross/cross_utils.h>
#include <ledger/ledger_frm.h>

namespace bubi {
	//?Y2?¨º1¨®??¡Â?¡¥¨ª¡§?a¨º??t
	//void BlockListener::HandleBlock(LedgerFrm::pointer closing_ledger){
	//	//TODO
	//	if (0){
	//		//?¨¢¨¨????¡§o???¦Ì????¡é¨º??t¡ê??¨°¨ª¡§?a??1??¡è¨¨?
	//		protocol::CrossProposalInfo proposal;
	//		//TODO¡êo¡¤???meta data¡¤¡é?¨ª???¡é
	//		channel_->SendRequest("", protocol::CROSS_MSGTYPE_PROPOSAL_NOTICE, proposal.SerializeAsString());
	//	}
	//}

	class MessageHandler{
	public:
		MessageHandler(MessageChannel *channel, const std::string &comm_contract);
		~MessageHandler(){}

		void OnHandleProposal(const protocol::WsMessage &message);
		void OnHandleNotarys(const protocol::WsMessage &message);
		void OnHandleAccountNonce(const protocol::WsMessage &message);
		void OnHandleDoTransaction(const protocol::WsMessage &message);

	private:
		MessageChannel *channel_;
		std::string comm_contract_;
	};

	class CrossChainMgr : public utils::Singleton<CrossChainMgr>, public IMessageHandler{
		friend class utils::Singleton<bubi::CrossChainMgr>;
	public:
		CrossChainMgr();
		~CrossChainMgr();

		bool Initialize();
		bool Exit();

	private:
		virtual void HandleMessage(const std::string &comm_unique, const protocol::WsMessage &message) override;

	private:
		MessageChannel channel_;
		MessageHandler *handler_;
	};
}

#endif
