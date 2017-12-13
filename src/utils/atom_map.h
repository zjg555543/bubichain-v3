#ifndef TEMPLATE_ATOMIC_MAP_H
#define TEMPLATE_ATOMIC_MAP_H

#include <map>
#include <string>
#include <memory>
#include <exception>
#include "logger.h"

namespace bubi
{
    template<class KEY, class VALUE, class COMPARE = std::less<KEY>>
    class AtomMap
    {
    public:
		typedef std::shared_ptr<VALUE> pointer;

        enum actType
        {
            ADD = 0,
            MOD = 1,
            DEL = 2,
			REV = 3,
			MAX,
        };

		struct ActValue
		{
			pointer value_;
			actType type_;
			ActValue(actType type = MAX) :type_(type){}
			ActValue(const pointer& val, actType type = MAX) :value_(val), type_(type){}
		};

        typedef std::map<KEY, ActValue, COMPARE> mapKV;

	protected:
        mapKV actionBuf_;
        mapKV revertBuf_;
        mapKV dataCopy_;
        mapKV data_;

	private:
		void SetValue(const KEY& key, const pointer& val)
		{
			if (data_.find(key) == data_.end())
				actionBuf_[key] = ActValue(val, ADD);
			else
				actionBuf_[key] = ActValue(val, MOD);
		}

		bool GetValue(const KEY& key, pointer& val)
		{
			bool ret = false;
			auto itAct = actionBuf_.find(key);
			if (itAct != actionBuf_.end())
			{
				if (itAct->second.type_ != DEL)
				{
					val = itAct->second.value_;
					ret = true;
				}
				//else ret = false;
			}
			else
			{
				auto itData = data_.find(key);
				if (itData != data_.end())
				{
					if (itData->second.type_ != DEL)
					{
						val = std::make_shared<VALUE>(*itData->second.value_);
						actionBuf_[key] = ActValue(val, MOD);
						ret = true;
					}
					//else ret = false;
				}
				else
				{
					if (GetFromDB(key, val))
					{
						actionBuf_[key] = ActValue(val, ADD);
						ret = true;
					}
					//else ret = false;
				}
			}
			return ret;
		}

    public:
		const mapKV& GetData()
		{
			return data_;
		}

		bool Set(const KEY& key, const pointer& val)
		{
            bool ret = true;

			try{ SetValue(key, val); }
            catch(std::exception& e)
            { 
                LOG_ERROR("set exception, detail: %s", e.what());
                ret = false;
            }

		    return ret;
		}

        bool Get(const KEY& key, pointer& val)
        {
            bool ret = true;

            try{ ret = GetValue(key, val); }
            catch(std::exception& e)
            { 
                LOG_ERROR("get exception, detail: %s", e.what());
                ret = false;
            }
            return ret;
        }

        bool Del(const KEY& key)
        {
            bool ret = true;

            try{ actionBuf_[key] = ActValue(DEL); }
            catch(std::exception& e)
            { 
                LOG_ERROR("delete exception, detail: %s", e.what());
                ret = false;
            }

            return ret;
        }

        bool RevertCommit()
        {
            try
            {
				for (auto act : actionBuf_)
				{
					if(act.second.type_ == ADD)
						revertBuf_[act.first] = ActValue(REV); //if type_ == REV when UnCommit, data_.erase(key)
					else
						revertBuf_[act.first] = data_[act.first];

					data_[act.first] = act.second; //include type_ == DEL(UpdateToDB will use DEL and key)
				}
            }
            catch(std::exception& e)
            { 
                LOG_ERROR("commit exception, detail: %s", e.what());

				UnRevertCommit();
				actionBuf_.clear();
				revertBuf_.clear();

                return false;
            }

			revertBuf_.clear();
            return true;
        }

		bool UnRevertCommit()
		{
            bool ret = true;

			try
			{
				for (auto rev : revertBuf_)
				{
					if (rev.second.type_ == REV)
						data_.erase(rev.first);
					else
						data_[rev.first] = rev.second;
				}
			}
            catch(std::exception& e)
            { 
                LOG_ERROR("uncommit exception, detail: %s", e.what());
                ret = false;
            }

            return ret;
		}

        bool CopyCommit()
        {
            try
            {
                dataCopy_ = data_;
				for (auto act : actionBuf_)
				{
					dataCopy_[act.first] = act.second;
				}
            }
			catch (std::exception& e)
			{
				LOG_ERROR("copy commit exception, detail: %s", e.what());
				actionBuf_.clear();
				dataCopy_.clear();

				return false;
			}

			data_.swap(dataCopy_);
			dataCopy_.clear();

            return true;
        }

		void ClearChange()
		{
			actionBuf_.clear();
			revertBuf_.clear();
			dataCopy_.clear();
		}

        virtual bool GetFromDB(const KEY& key, pointer& val){ return false; }
		virtual void updateToDB(){}
	};
}

#endif //TEMPLATE_ATOMIC_MAP_H

