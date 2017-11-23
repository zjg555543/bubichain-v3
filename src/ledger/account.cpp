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
#include <common/pb2json.h>
#include <ledger/ledger_manager.h>
#include "account.h"
#include "kv_trie.h"

namespace bubi {

	//AccountFrm::AccountFrm() {
	//	utils::AtomicInc(&bubi::General::account_new_count);
	//	assets_ = nullptr;
	//	storage_ = nullptr;
	//}

	AccountFrm::AccountFrm(protocol::Account account_info) 
		: account_info_(account_info) {
		utils::AtomicInc(&bubi::General::account_new_count);

		std::string asset_prefix = ComposePrefix(General::ASSET_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		std::string metadata_prefix = ComposePrefix(General::METADATA_PREFIX, utils::String::HexStringToBin(account_info_.address()));

		assets_.init(Storage::Instance().account_db(), asset_prefix);
		metadata_.init(Storage::Instance().account_db(), metadata_prefix);
	}

	AccountFrm::AccountFrm(std::shared_ptr<AccountFrm> account){
		account_info_.CopyFrom(account->ProtocolAccount());
		assets_ = account->assets_;
		metadata_ = account->metadata_;
	}

	AccountFrm::~AccountFrm() {
		utils::AtomicInc(&bubi::General::account_delete_count);
	}

	std::string AccountFrm::Serializer() {
		return account_info_.SerializeAsString();
	}

	bool AccountFrm::UnSerializer(const std::string &str) {
		if (!account_info_.ParseFromString(str)) {
			LOG_ERROR("Accountunserializer is erro!");
			return false;
		}
		return true;
	}

	std::string AccountFrm::GetAccountAddress()const {
		return account_info_.address();
	}

	bool AccountFrm::UpdateSigner(const std::string &signer, int64_t weight) {
		weight = weight & UINT8_MAX;
		if (weight > 0) {
			bool found = false;
			for (int32_t i = 0; i < account_info_.mutable_priv()->signers_size(); i++) {
				if (account_info_.mutable_priv()->signers(i).address() == signer) {
					found = true;
					account_info_.mutable_priv()->mutable_signers(i)->set_weight(weight);
				}
			}

			if (!found) {
				if (account_info_.priv().signers_size() >= protocol::Signer_Limit_SIGNER) {
					return false;
				}

				protocol::Signer* signer1 = account_info_.mutable_priv()->add_signers();
				signer1->set_address(signer);
				signer1->set_weight(weight);
			}
		}
		else {
			bool found = false;
			std::vector<std::pair<std::string, int64_t> > nold;
			for (int32_t i = 0; i < account_info_.mutable_priv()->signers_size(); i++) {
				if (account_info_.mutable_priv()->signers(i).address() != signer) {
					nold.push_back(std::make_pair(account_info_.mutable_priv()->signers(i).address(), account_info_.mutable_priv()->signers(i).weight()));
				}
				else {
					found = true;
				}
			}

			if (found) {
				account_info_.mutable_priv()->clear_signers();
				for (size_t i = 0; i < nold.size(); i++) {
					protocol::Signer* signer = account_info_.mutable_priv()->add_signers();
					signer->set_address(nold[i].first);
					signer->set_weight(nold[i].second);
				}
			}
		}

		return true;
	}

	const int64_t AccountFrm::GetTypeThreshold(const protocol::Operation::Type type) const {
		const protocol::AccountThreshold &thresholds = account_info_.priv().thresholds();
		for (int32_t i = 0; i < thresholds.type_thresholds_size(); i++) {
			if (thresholds.type_thresholds(i).type() == type) {
				return thresholds.type_thresholds(i).threshold();
			}
		}

		return 0;
	}

	bool AccountFrm::UpdateTypeThreshold(const protocol::Operation::Type type, int64_t threshold) {
		threshold = threshold & UINT64_MAX;
		if (threshold > 0) {
			protocol::AccountThreshold *thresholds = account_info_.mutable_priv()->mutable_thresholds();
			bool found = false;
			for (int32_t i = 0; i < thresholds->type_thresholds_size(); i++) {
				if (thresholds->type_thresholds(i).type() == type) {
					found = true;
					thresholds->mutable_type_thresholds(i)->set_threshold(threshold);
				}
			}

			if (!found) {
				if (thresholds->type_thresholds_size() >= protocol::Signer_Limit_SIGNER) {
					return false;
				}

				protocol::OperationTypeThreshold* signer1 = thresholds->add_type_thresholds();
				signer1->set_type(type);
				signer1->set_threshold(threshold);
			}
		}
		else {
			bool found = false;
			protocol::AccountThreshold *thresholds = account_info_.mutable_priv()->mutable_thresholds();
			std::vector<std::pair<protocol::Operation::Type, int32_t> > nold;
			for (int32_t i = 0; i < thresholds->type_thresholds_size(); i++) {
				if (thresholds->type_thresholds(i).type() != type) {
					nold.push_back(std::make_pair(thresholds->type_thresholds(i).type(), thresholds->type_thresholds(i).threshold()));
				}
				else {
					found = true;
				}
			}

			if (found) {
				thresholds->clear_type_thresholds();
				for (size_t i = 0; i < nold.size(); i++) {
					protocol::OperationTypeThreshold* signer = thresholds->add_type_thresholds();
					signer->set_type(nold[i].first);
					signer->set_threshold(nold[i].second);
				}
			}
		}
		return true;
	}


	void AccountFrm::ToJson(Json::Value &result) {
		result = bubi::Proto2Json(account_info_);
	}

	void AccountFrm::GetAllAssets(std::vector<protocol::Asset>& assets){
		assets_.GetAll(assets);
	}

	void AccountFrm::GetAllMetaData(std::vector<protocol::KeyPair>& metadata){
		metadata_.GetAll(metadata);
	}

	bool AccountFrm::GetAsset(const protocol::AssetProperty &asset_property, protocol::Asset& asset)
	{
		return assets_.Get(asset_property, asset);
	}

	void AccountFrm::SetAsset(const protocol::Asset& data_ptr){
		assets_.Set(data_ptr.property(), data_ptr);
	}

	//
	bool AccountFrm::GetMetaData(const std::string& binkey, protocol::KeyPair& keypair_ptr)
	{
		StringPack key(binkey);
		return metadata_.Get(key, keypair_ptr);
	}

	void AccountFrm::SetMetaData(const protocol::KeyPair& dataptr)
	{
		StringPack key(dataptr.key());
		metadata_.Set(key, dataptr);
	}

	bool AccountFrm::DeleteMetaData(const protocol::KeyPair& dataptr)
	{
		StringPack key(dataptr.key());
		return metadata_.Del(dataptr.key());
	}

	void AccountFrm::UpdateHash(std::shared_ptr<WRITE_BATCH> batch){

		assets_.updateToDB(batch);
		metadata_.updateToDB(batch);

		account_info_.set_assets_hash(assets_.GetRootHash());
		account_info_.set_metadatas_hash(metadata_.GetRootHash());
	}

	void AccountFrm::NonceIncrease(){
		int64_t new_nonce = account_info_.nonce() + 1;
		account_info_.set_nonce(new_nonce);
	}

	bool AccountFrm::Commit()
	{
		return assets_.Commit() && metadata_.Commit();
	}

	void AccountFrm::UnCommit()
	{
		assets_.UnCommit();
		metadata_.UnCommit();
	}

	void AccountFrm::Reset()
	{
		assets_.Reset();
		metadata_.Reset();
	}
}

