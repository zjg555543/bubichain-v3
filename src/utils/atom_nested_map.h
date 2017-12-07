
#ifndef TEMPLATE_ATOMIC_NESTED_MAP_H
#define TEMPLATE_ATOMIC_NESTED_MAP_H

#include <map>
#include <string>
#include <exception>
#include "logger.h"

namespace bubi
{
    template<class Index, class Key, class Value, class CompareKey = std::less<Key>, class CompareIndex = std::less<Index>>
    class AtomNestedMap
    {
    public:
		enum actType
		{
			ADD = 0,
			MOD = 1,
			DEL = 2,
			REV = 3,
			MAX,
		};

	    typedef std::pair<actType, Value> action;
	    typedef std::map<Key, action, CompareKey> mapKV;

	    class InlayerMap
        {
		protected:
	    	mapKV data_;
	    	mapKV buff_;

	    public:
            mapKV* GetData(){ return &data_; }
            mapKV* GetBuff(){ return &buff_; }

	    	bool Set(const Key& key, const Value& value)
            {
                bool ret = true;

                try{ SetValue(key, value); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("set exception, detail: %s", e.what());
                    ret = false;
                }

                return ret;
            }

	    	bool Get(const Key& key, Value& value)
            {
                bool ret = false;

                try{ ret = GetValue(key, value); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("get exception, detail: %s", e.what());
                    ret = false;
                }

                return ret;
            }

	    	bool Del(const Key& key)
            {
                bool ret = true;

                try{ DelValue(key); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("delete exception, detail: %s", e.what());
                    ret = false;
                }

                return ret;
            }

			virtual bool GetFromDB(const Key& key, Value& value){ return false; }
			virtual bool UpdateToDB(){ return false; }

        private:

	    	bool GetValue(const Key& key, Value& value)
	    	{
	    		bool ret = false;
	    		auto itBuf = buff_.find(key);
	    		if (itBuf != buff_.end())
	    		{
	    			if (itBuf->second.first != DEL)
	    			{
	    				value = itBuf->second.second;
	    				ret = true;
	    			}
	    			//else ret = false;
	    		}
	    		else
	    		{
	    			auto itData = data_.find(key);
	    			if (itData != data_.end())
	    			{
	    				if (itData->second.first != DEL)
	    				{
	    					buff_[key] = itData->second;
	    					value = buff_[key].second;
	    					ret = true;
	    				}
	    				//else ret = false;
	    			}
	    			else
	    			{
	    				if (GetFromDB(key, value))
	    				{
	    					buff_[key] = action(ADD, value);
	    					ret = true;
	    				}
	    				//else ret = false;
	    			}
	    		}

	    		return ret;
	    	}

	    	void SetValue(const Key& key, const Value& value)
	    	{
	    		if (data_.find(key) == data_.end())
	    			buff_[key] = action(ADD, value);
	    		else
	    			buff_[key] = action(MOD, value);
	    	}

	    	void DelValue(const Key& key)
	    	{
	    		buff_[key] = action(DEL, Value());
	    	}
	    };

    private:
        std::map<Index, mapKV*, CompareIndex> actionBuf_;
        std::map<Index, mapKV,  CompareIndex> revertBuf_;
		std::map<Index, mapKV*, CompareIndex> totalData_;

	protected:
        std::map<Index, InlayerMap, CompareIndex> entries_;

    public:
		void JointData(const Index& index, InlayerMap& obj)
		{
			actionBuf_[index] = obj.GetBuff();
			totalData_[index] = obj.GetData();
		}

        bool Set(const Index& index, InlayerMap& obj)
        {
            entries_[index] = obj;
			JointData(index, entries_[index]);

            return true;
        }

        bool Get(const Index& index, InlayerMap& obj)
        {
            ret = false;

            auto it = entries_.find(index);
            if(it != entries_.end())
            {
                obj = it->second;
                ret = true;
            }
            else
            {
                if(GetFromDB(index, obj))
                {
                    Set(index, obj);
                    ret = true;
                }
            }

            return ret;
        }

        bool Commit()
        {
			bool ret = true;

            try
            {
                for(auto indexBuf : actionBuf_)
                {
                    const Index& index = indexBuf.first;
                    const mapKV* mkv   = indexBuf.second;

                    for(auto keyAct : (*mkv))
                    {
                        const Key&    key = keyAct.first;
                        const action& act = keyAct.second;

                        if(act.first == ADD)
                            revertBuf_[index][key] = action(REV, Value());
                        else
                            revertBuf_[index][key] = (*totalData_[index])[key];

			    		(*totalData_[index])[key] = (*actionBuf_[index])[key];
                    }
                }
            }
			catch (std::exception& e)
			{
				LOG_ERROR("commit exception, detail: %s", e.what());
                ret = false;
			}

            return ret;
        }

        bool UnCommit()
        {
            bool ret = true;

            try
            {
                for(auto indexBuf : revertBuf_)
                {
                    const Index& index = indexBuf.first;
                    const mapKV& mkv   = indexBuf.second;

                    for(auto keyAct : mkv)
                    {
                        const Key&    key = keyAct.first;
                        const action& act = keyAct.second;

                        if(act.first == REV)
                            (*totalData_[index]).erase(key);
                        else
                            (*totalData_[index])[key] = revertBuf_[index][key];
                    }
                }
            }
			catch (std::exception& e)
			{
				LOG_ERROR("uncommit exception, detail: %s", e.what());
                ret = false;
			}

            return ret;
        }

		void ClearChangeBuf()
        {
            for(auto buf : actionBuf_)
                buf.second->clear();
        }

		void ClearRevertBuf()
		{
			for (auto rev : revertBuf_)
				rev.second.clear();
		}

		virtual bool GetFromDB(const Index& index, InlayerMap& obj){ return false; }
		virtual bool UpdateToDB(){ return false; }
    };
}

#endif //TEMPLATE_ATOMIC_NESTED_MAP_H
