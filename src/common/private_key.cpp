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

#include <openssl/ecdsa.h>
#include <openssl/rsa.h>
#include <openssl/ripemd.h>
#include <utils/logger.h>
#include <utils/crypto.h>
#include <utils/sm3.h>
#include <utils/strings.h>
#include "general.h"
#include "private_key.h"
#include "cfca.h"

namespace bubi {

    std::string CalcHash(const std::string &value,const SignatureType &sign_type) {
        std::string hash;
        if (sign_type == SIGNTYPE_ED25519) {
            hash = utils::Sha256::Crypto(value);
        }
        else {
            hash = utils::Sm3::Crypto(value);
        }
        return hash;
    }

    bool GetKeyElement(const std::string &base58_key, PrivateKeyPrefix &prefix, SignatureType &sign_type, std::string &raw_data) {
        PrivateKeyPrefix prefix_tmp;
        SignatureType sign_type_tmp = SIGNTYPE_NONE;
        std::string buff = utils::Base58::Decode(base58_key);
        if (base58_key.substr(0,2) == "bu" && buff.size() ==27){// address
            prefix_tmp = ADDRESS_PREFIX;
        }
        else if (base58_key.substr(0,4) == "priv" && buff.size() == 41){//private key
            prefix_tmp = PRIVATEKEY_PREFIX;
        }
        else{//public key
            uint8_t a = (uint8_t)buff.at(0);
            PrivateKeyPrefix privprefix = (PrivateKeyPrefix)a;
            if (privprefix == PUBLICKEY_PREFIX)
                prefix_tmp = PUBLICKEY_PREFIX;
            else
                return false;
        }     
		
       

		bool ret = true;
		if (prefix_tmp == ADDRESS_PREFIX) {
            uint8_t a = (uint8_t)buff.at(2); 
            sign_type_tmp = (SignatureType)a;   
            size_t datalen = buff.size() - 7;
			switch (sign_type_tmp) {
			case SIGNTYPE_ED25519:{
				ret = (ED25519_ADDRESS_LENGTH == datalen);
				break;
			}
			case SIGNTYPE_CFCASM2:{
				ret = (SM2_ADDRESS_LENGTH == datalen);
				break;
			}
			default:
				ret = false;
			}
		}
		else if (prefix_tmp == PUBLICKEY_PREFIX) {
            uint8_t a = (uint8_t)buff.at(1);  
            sign_type_tmp = (SignatureType)a;
            size_t datalen = buff.size() - 6;
			switch (sign_type_tmp) {
			case SIGNTYPE_ED25519:{
				ret = (ED25519_PUBLICKEY_LENGTH == datalen);
				break;
			}
			case SIGNTYPE_CFCASM2:{
				ret = (SM2_PUBLICKEY_LENGTH == datalen);
				break;
			}
			default:
				ret = false;
			}
		}
		else if (prefix_tmp == PRIVATEKEY_PREFIX) {
            uint8_t a = (uint8_t)buff.at(3);  
            sign_type_tmp = (SignatureType)a;
            size_t datalen = buff.size() - 9;
			switch (sign_type_tmp) {
			case SIGNTYPE_ED25519:{
				ret = (ED25519_PRIVATEKEY_LENGTH == datalen);
				break;
			}
			case SIGNTYPE_CFCASM2:{
				ret = (SM2_PRIVATEKEY_LENGTH == datalen);
				break;
			}
			default:
				ret = false;
			}
		}
		else {
			ret = false;
		}

		if (ret){
            //checksum
            std::string checksum = buff.substr(buff.size() - 4);
            std::string hash1 = CalcHash(buff.substr(0, buff.size() - 4), sign_type_tmp);
            std::string hash2 = CalcHash(hash1, sign_type_tmp);
            if (checksum.compare(hash2.substr(0, 4)))
                return false;

			prefix = prefix_tmp;
			sign_type = sign_type_tmp;
            if (prefix_tmp == ADDRESS_PREFIX) {
                raw_data = buff.substr(3, buff.size() - 7);
            }
            else if (prefix_tmp == PUBLICKEY_PREFIX) {
                raw_data = buff.substr(2, buff.size() - 6);
            }
            else if (prefix_tmp == PRIVATEKEY_PREFIX) {
                raw_data = buff.substr(4, buff.size() - 9);
            }
		} 

		return ret;
	}

	std::string GetSignTypeDesc(SignatureType type) {
		switch (type) {
		case SIGNTYPE_CFCASM2: return "sm2";
		case SIGNTYPE_ED25519: return "ed25519";
		}

		return "";
	}

	SignatureType GetSignTypeByDesc(const std::string &desc) {
		
		if (desc == "sm2") {
			return SIGNTYPE_CFCASM2;
		}
		else if (desc == "ed25519") {
			return SIGNTYPE_ED25519;
		}
		return SIGNTYPE_NONE;
	}

    PublicKey::PublicKey() :valid_(false), type_(SIGNTYPE_ED25519) {}

	PublicKey::~PublicKey() {}

	PublicKey::PublicKey(const std::string &base58_pub_key) {
		do {
			PrivateKeyPrefix prefix;
            //valid_ = GetKeyElement(base58_pub_key, prefix, type_, raw_pub_key_);
			//valid_ = (prefix == PUBLICKEY_PREFIX);
            if (GetKeyElement(base58_pub_key, prefix, type_, raw_pub_key_)){
                valid_ = (prefix == PUBLICKEY_PREFIX);
            }
		} while (false);
	}

	void PublicKey::Init(std::string rawpkey) {
		raw_pub_key_ = rawpkey;
	}

	bool PublicKey::IsAddressValid(const std::string &address_base58) {
        std::string address = utils::Base58::Decode(address_base58);
		do {
            PrivateKeyPrefix prefix;
            SignatureType sign_type;
            std::string raw_pub_key;
            if (GetKeyElement(address_base58, prefix, sign_type, raw_pub_key)){
                return (prefix == ADDRESS_PREFIX);
            }
		} while (false);

		return false;
	}

	std::string PublicKey::GetBase58Address() const {
		
		std::string str_result = "";
        //append prefix (bubi)
        /*str_result.push_back((char)0XE6);
        str_result.push_back((char)0X9A);
        str_result.push_back((char)0X73);
        str_result.push_back((char)0XFF);*/
        //append prefix (bu)
        str_result.push_back((char)0X01);
        str_result.push_back((char)0X56);

		//append version 1byte
		str_result.push_back((char)type_);

		//append public key 20byte
		std::string hash = CalcHash(raw_pub_key_,type_);
		str_result.append(hash.substr(12));

		//append check sum 4byte
        std::string hash1, hash2;
        hash1 = CalcHash(str_result, type_);
        hash2 = CalcHash(hash1, type_);

        str_result.append(hash2.c_str(), 4);
        return utils::Base58::Encode(str_result);
	}

	std::string PublicKey::GetRawPublicKey() const {
		return raw_pub_key_;
	}

	std::string PublicKey::GetBase58PublicKey() const {
		
		std::string str_result = "";
        //append PrivateKeyPrefix
        str_result.push_back((char)PUBLICKEY_PREFIX);

		//append version
		str_result.push_back((char)type_);

		//append public key
		str_result.append(raw_pub_key_);

        std::string hash1, hash2;
        hash1 = CalcHash(str_result, type_);
        hash2 = CalcHash(hash1, type_);

        str_result.append(hash2.c_str(), 4);
        return utils::Base58::Encode(str_result);
	}
    //not modify
    bool PublicKey::Verify(const std::string &data, const std::string &signature, const std::string &public_key_base58) {
		PrivateKeyPrefix prefix;
		SignatureType sign_type;
		std::string raw_pubkey;
        bool valid = GetKeyElement(public_key_base58, prefix, sign_type, raw_pubkey);
		if (!valid || prefix != PUBLICKEY_PREFIX) {
			return false;
		} 

		if (sign_type == SIGNTYPE_ED25519 ) {
			return ed25519_sign_open((unsigned char *)data.c_str(), data.size(), (unsigned char *)raw_pubkey.c_str(), (unsigned char *)signature.c_str()) == 0;
		}
		else if (sign_type == SIGNTYPE_CFCASM2) {
			return utils::EccSm2::verify(utils::EccSm2::GetCFCAGroup(), raw_pubkey, "1234567812345678", data, signature) == 1;
		}
        else{
            LOG_ERROR("Unknown signature type(%d)", sign_type);
        }
		return false;
	}

	//地址是否合法
	PrivateKey::PrivateKey(SignatureType type) {
        std::string raw_pub_key = "";
		type_ = type;
		if (type_ == SIGNTYPE_ED25519) {
			utils::MutexGuard guard_(lock_);
			// ed25519;
			raw_priv_key_.resize(32);
			ed25519_randombytes_unsafe((void*)raw_priv_key_.c_str(), 32);

            raw_pub_key.resize(32);
            ed25519_publickey((const unsigned char*)raw_priv_key_.c_str(), (unsigned char*)raw_pub_key.c_str());
		}
		else if (type_ == SIGNTYPE_CFCASM2) {
			utils::EccSm2 key(utils::EccSm2::GetCFCAGroup());
			key.NewRandom();
			raw_priv_key_ = key.getSkeyBin();
            raw_pub_key = key.GetPublicKey();
		}
        else{
            LOG_ERROR("Unknown signature type(%d)", type_);
        }
        pub_key_.Init(raw_pub_key);
		pub_key_.type_ = type_;
		pub_key_.valid_ = true;
		valid_ = true;
	}

	PrivateKey::~PrivateKey() {}
    //not modify
    bool PrivateKey::From(const std::string &bas58_private_key) {
		valid_ = false;
		std::string tmp;

		do {
			PrivateKeyPrefix prefix;
			std::string raw_pubkey;
            valid_ = GetKeyElement(bas58_private_key, prefix, type_, raw_priv_key_);
			if (!valid_ || prefix != PRIVATEKEY_PREFIX) {
				return false;
			}

			if (type_ == SIGNTYPE_ED25519) {
				tmp.resize(32);
				ed25519_publickey((const unsigned char*)raw_priv_key_.c_str(), (unsigned char*)tmp.c_str());
			}
			else if (type_ == SIGNTYPE_CFCASM2) {
				utils::EccSm2 skey(utils::EccSm2::GetCFCAGroup());
				skey.From(raw_priv_key_);
				tmp = skey.GetPublicKey();
			}
            else{
                LOG_ERROR("Unknown signature type(%d)", type_);
            }
			//ToBase58();
			pub_key_.type_ = type_;
			pub_key_.Init(tmp);
			pub_key_.valid_ = true;
			valid_ = true;

		} while (false);
		return valid_;
	}

	PrivateKey::PrivateKey(const std::string &base58_private_key) {
		From(base58_private_key);
	}

	
    //not modify
	std::string PrivateKey::Sign(const std::string &input) const {
		unsigned char sig[10240];
		unsigned int sig_len = 0;

		if (type_ == SIGNTYPE_ED25519) {
			/*	ed25519_signature sig;*/
			ed25519_sign((unsigned char *)input.c_str(), input.size(), (const unsigned char*)raw_priv_key_.c_str(), (unsigned char*)pub_key_.GetRawPublicKey().c_str(), sig);
			sig_len = 64;
		}
		else if (type_ == SIGNTYPE_CFCASM2) {
			utils::EccSm2 key(utils::EccSm2::GetCFCAGroup());
			key.From(raw_priv_key_);
			std::string r, s;
			return key.Sign("1234567812345678", input);
		}
        else{
            LOG_ERROR("Unknown signature type(%d)", type_);
        }
		std::string output;
		output.append((const char *)sig, sig_len);
		return output;
	}

    std::string PrivateKey::GetBase58PrivateKey() const {
        std::string str_result;
        //append prefix(priv)
        str_result.push_back((char)0XDA);
        str_result.push_back((char)0X37);
        str_result.push_back((char)0X9F);

		//append version 1
		str_result.push_back((char)type_);

		//append private key 32
		str_result.append(raw_priv_key_);

        //压缩标志
        str_result.push_back(0X00);

        //bitcoin use 4 byte hash check.
        std::string hash1, hash2;
        hash1 = CalcHash(str_result, type_);
        hash2 = CalcHash(hash1, type_);

        str_result.append(hash2.c_str(),4);
        return utils::Base58::Encode(str_result);
	}

	std::string PrivateKey::GetBase58Address() const {
		return pub_key_.GetBase58Address();
	}

	std::string PrivateKey::GetBase58PublicKey() const {
		return pub_key_.GetBase58PublicKey();
	}

	std::string PrivateKey::GetRawPublicKey() const {
		return pub_key_.GetRawPublicKey();
	}

	utils::Mutex PrivateKey::lock_;
}
