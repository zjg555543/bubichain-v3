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

#include <common/storage.h>
#include "ledger_manager.h"

namespace bubi{

	Environment::Environment(mapKV* data, bool dummy) : AtomMap<std::string, AccountFrm>(data)
	{
		useAtomMap_ = Configure::Instance().ledger_configure_.use_atom_map_;
		parent_ = nullptr;
	}

	Environment::Environment(Environment* parent){

		useAtomMap_ = Configure::Instance().ledger_configure_.use_atom_map_;
		if (useAtomMap_)
		{
			parent_ = nullptr;
			return;
		}

		parent_ = parent;
		if (parent_){
			for (auto it = parent_->entries_.begin(); it != parent_->entries_.end(); it++){
				entries_[it->first] = std::make_shared<AccountFrm>(it->second);
			}
		}
	}

	bool Environment::GetEntry(const std::string &key, AccountFrm::pointer &frm){
		if (useAtomMap_)
			return Get(key, frm);

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

	bool Environment::Commit(){
		if (useAtomMap_)
			return AtomMap<std::string, AccountFrm>::Commit();

		parent_->entries_ = entries_;
		return true;
	}

	bool Environment::AddEntry(const std::string& key, AccountFrm::pointer frm){
		if (useAtomMap_ == true)
			return Set(key, frm);

		entries_[key] = frm;
		return true;
	}

	bool Environment::GetFromDB(const std::string &address, AccountFrm::pointer &account_ptr)
	{
		return AccountFromDB(address, account_ptr);
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