
#include <common/storage.h>
#include "ledger_manager.h"

namespace bubi{

	//int64_t Environment::time_ = 0;

	Environment::Environment(Environment* parent){
		utils::AtomicInc(&bubi::General::env_new_count);
		parent_ = parent;
		if (parent_){
			for (auto it = parent_->entries_.begin(); it != parent_->entries_.end(); it++){
				entries_[it->first] = std::make_shared<AccountFrm>(it->second);
			}
		}
	}
	
	Environment::~Environment() {
		utils::AtomicInc(&bubi::General::env_delete_count);
	}

	bool Environment::GetEntry(const std::string &key, AccountFrm::pointer &frm){
		if (entries_.find(key) == entries_.end()){
			if (AccountFromDB(key, frm)){
				entries_[key] = frm;
				return true;
			}
			else{
				return false;
			}
		}
		else{
			frm = entries_[key];
			return true;
		}
	}

	void Environment::Commit(){
		parent_->entries_ = entries_;
	}

	bool Environment::AddEntry(const std::string& key, AccountFrm::pointer frm){
		entries_[key] = frm;
		return true;
	}

	bool Environment::AccountFromDB(const std::string &address, AccountFrm::pointer &account_ptr){

		auto db = Storage::Instance().account_db();
		std::string index = utils::String::HexStringToBin(address);
		std::string buff;
		if (!LedgerManager::Instance().tree_->Get(index, buff)){
			return false;
		}

		protocol::Account account;
		if (!account.ParseFromString(buff)){
			BUBI_EXIT("fatal error, account(%s) ParseFromString failed", address.c_str());
		}
		account_ptr = std::make_shared<AccountFrm>(account);
		return true;
	}
}
