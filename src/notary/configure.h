
#ifndef NOTARY_CONFIGURE_H_
#define NOTARY_CONFIGURE_H_

#include <common/configure_base.h>

namespace bubi {

	class NotaryConfigure {
	public:
		NotaryConfigure();
		~NotaryConfigure();

		utils::InetAddress listen_addr_;
		bool enabled_;
		bool Load(const Json::Value &value);
	};

	class PairChainConfigure {
	public:
		PairChainConfigure(){}
		~PairChainConfigure(){}

		std::string comm_unique_;
		std::string target_comm_unique_;

		bool Load(const Json::Value &value);
	};

	typedef std::map<std::string, PairChainConfigure> PairChainMap;

	class Configure : public ConfigureBase, public utils::Singleton<Configure> {
		friend class utils::Singleton<Configure>;
		Configure();
		~Configure();

	public:
		LoggerConfigure logger_configure_;
		NotaryConfigure notary_configure_;
		PairChainMap pair_chain_map_;

		virtual bool LoadFromJson(const Json::Value &values);
	};
}

#endif
