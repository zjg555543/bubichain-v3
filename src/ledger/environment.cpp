/*
Copyright Bubi Technologies Co., Ltd. 2017 All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stack>
#include <common/storage.h>
#include <utils/scopeGuard.h>
#include "ledger_manager.h"

namespace bubi{

	//int64_t Environment::time_ = 0;

	//Environment::Environment(Environment* parent){
	//	parent_ = parent;
	//	if (parent_){
	//		for (auto it = parent_->entries_.begin(); it != parent_->entries_.end(); it++){
	//			entries_[it->first] = std::make_shared<AccountFrm>(it->second);
	//		}
	//	}
	//}
	Environment::Environment(){}

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

	void Environment::Commit()
	{
		std::stack< ObjScopeGuardParm0<AccountFrm*, void (AccountFrm::*)()> > scopeGuards;

		for (auto kv : entries_)
		{
			kv.second->Commit();
			auto pAccountFrm = kv.second.get();
			auto guard = MakeGuard(pAccountFrm, &AccountFrm::UnCommit);
			scopeGuards.push(guard);
		}
		
		while (!scopeGuards.empty())
		{
			scopeGuards.top().Dismiss();
			scopeGuards.pop();
		}
	}

	void Environment::ClearChangeBuf()
	{
		for (auto entry : entries_){ entry.second->Reset(); }
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