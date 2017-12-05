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

#ifndef BUBI_ACCOUNT_H_
#define BUBI_ACCOUNT_H_

#include <utils/base_int.h>
#include <utils/crypto.h>
#include <utils/logger.h>
#include <proto/cpp/chain.pb.h>
#include <utils/entry_cache.h>
#include <utils/atomBatch.h>
#include "proto/cpp/merkeltrie.pb.h"
#include <common/storage.h>
#include "kv_trie.h"
namespace bubi {


	struct StringPack
	{
		std::string str_;
		StringPack(std::string str) : str_(str){}

		const std::string& SerializeAsString() const { return str_; }
		void ParseFromString(std::string str){ str_ = str; }
	};

	struct AssetSort {
		bool operator() (const protocol::AssetProperty& a, const protocol::AssetProperty& b)const{
			return a.SerializeAsString() < b.SerializeAsString();
		}
	};

	struct StringPackSort {
		bool operator() (const StringPack &l, const StringPack &r) const {
			return l.SerializeAsString() < r.SerializeAsString();
		}
	};

	template<typename K, typename V, typename C = std::less<K>>
	class MapPackForAcc: public AtomBatch<std::string, K, V, C>::MapPack
	{
	public:
		void InitDB(KeyValueDb* db, const std::string prefix)
		{
			db_ = db;
			prefix_ = prefix;
		}

		void GetAll(std::vector<V>& vals)
		{
			KVTrie trie;
			auto batch = std::make_shared<WRITE_BATCH>();
			trie.Init(db_, batch, prefix_, 1);
			std::vector<std::string> values;

			trie.GetAll("", values);
			for (auto value : values)
			{
				V val;
				val.ParseFromString(value);
				vals.push_back(val);
			}
		}

		virtual bool GetFromDB(const K& key, V& val)
		{
			std::string buff;
			auto dbKey = key.SerializeAsString();

			KVTrie trie;
			auto batch = std::make_shared<WRITE_BATCH>();
			trie.Init(db_, batch, prefix_, 1);

			if (!trie.Get(dbKey, buff))
				return false;

			if (!val.ParseFromString(buff))
			{
				BUBI_EXIT("fatal error, obj ParseFromString fail, data may damaged.");
				return false;
			}

			return true;
		}

		void updateToDB(std::shared_ptr<WRITE_BATCH> batch)
		{
			KVTrie trie;
			trie.Init(db_, batch, prefix_, 1);

			if (data_)
			    for (auto entry : (*data_))
			    {
				    if (entry.second.first == AtomBatch<std::string, K, V, C>::DEL)
					    trie.Delete(entry.first.SerializeAsString());
				    else
					    trie.Set(entry.first.SerializeAsString(), entry.second.second.SerializeAsString());
			    }

			trie.UpdateHash();
			hash_ = trie.GetRootHash();
		}

		std::string GetRootHash(){
			return hash_;
		}

	private:
		std::string prefix_;
		std::string hash_;
		KeyValueDb * db_;
	};

	class AccountFrm {
	public:
		typedef std::shared_ptr<AccountFrm>	pointer;
		typedef MapPackForAcc<protocol::AssetProperty, protocol::Asset, AssetSort> MapPackAssets;
		typedef MapPackForAcc<StringPack, protocol::KeyPair, StringPackSort> MapPackMetadata;

		AccountFrm() = delete;
		AccountFrm(protocol::Account account);
		AccountFrm(std::shared_ptr< AccountFrm> account);

		~AccountFrm();

		void ToJson(Json::Value &result);

		void GetAllAssets(std::vector<protocol::Asset>& assets);

		void GetAllMetaData(std::vector<protocol::KeyPair>& metadata);

		std::string	Serializer();
		bool	UnSerializer(const std::string &str);

		std::string GetAccountAddress()const;

		bool GetAsset(const protocol::AssetProperty &_property, protocol::Asset& result);

		void SetAsset(const protocol::Asset& result);

		bool GetMetaData(const std::string& binkey, protocol::KeyPair& result);

		void SetMetaData(const protocol::KeyPair& result);

		bool DeleteMetaData(const protocol::KeyPair& dataptr);

		protocol::Account &GetProtoAccount() {
			return account_info_;
		}

		protocol::Account ProtocolAccount() const{
			return account_info_;
		}

		int64_t GetAccountNonce() const {
			return account_info_.nonce();
		}

		const int64_t GetProtoMasterWeight() const {
			return account_info_.priv().master_weight();
		}

		const int64_t GetProtoTxThreshold() const {
			return account_info_.priv().thresholds().tx_threshold();
		}

		const int64_t GetTypeThreshold(const protocol::Operation::Type type) const;

		void SetProtoMasterWeight(int64_t weight) {
			return account_info_.mutable_priv()->set_master_weight(weight);
		}

		void SetProtoTxThreshold(int64_t threshold) {
			return account_info_.mutable_priv()->mutable_thresholds()->set_tx_threshold(threshold);
		}

		bool UpdateSigner(const std::string &signer, int64_t weight);
		bool UpdateTypeThreshold(const protocol::Operation::Type type, int64_t threshold);
		void UpdateHash(std::shared_ptr<WRITE_BATCH> batch);
		void NonceIncrease();

		MapPackAssets& GetAccountAsset();
		MapPackMetadata& GetAccountMetadata();

    private:
		MapPackAssets assets_;
		MapPackMetadata metadata_;

		protocol::Account	account_info_;
	};

}

#endif
