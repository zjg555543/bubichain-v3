
#ifndef CROSS_CHAIN_MGR_H_
#define CROSS_CHAIN_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <cross/message_channel.h>
#include <cross/cross_utils.h>
#include <ledger/ledger_frm.h>

namespace bubi {
	//暂不使用主动通知事件
	//void BlockListener::HandleBlock(LedgerFrm::pointer closing_ledger){
	//	//TODO
	//	if (0){
	//		//读取指定合约的消息事件，则通知给公证人
	//		protocol::CrossProposalInfo proposal;
	//		//TODO：分析meta data发送消息
	//		channel_->SendRequest("", protocol::CROSS_MSGTYPE_PROPOSAL_NOTICE, proposal.SerializeAsString());
	//	}
	//}

	class MessageHandler{
	public:
		MessageHandler(MessageChannel *channel, const std::string &comm_contract);
		~MessageHandler(){}

		void OnHandleProposal(const protocol::WsMessage &message);
		void OnHandleCommInfo(const protocol::WsMessage &message);
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
