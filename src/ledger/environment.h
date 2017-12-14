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

#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include <proto/cpp/chain.pb.h>
#include <utils/entry_cache.h>
#include <utils/atom_map.h>
#include "account.h"

namespace bubi {

	class Environment: public AtomMap<std::string, AccountFrm>{
	public:
		bool GetEntry(const std::string& key, AccountFrm::pointer &frm);
		bool AddEntry(const std::string& key, AccountFrm::pointer frm);
		void Commit();
		virtual bool GetFromDB(const std::string& key, AccountFrm::pointer& val);
		static bool AccountFromDB(const std::string &address, AccountFrm::pointer &account_ptr);
	};
}
#endif