
#ifndef TEMPLATE_ATOMIC_BATCH_H
#define TEMPLATE_ATOMIC_BATCH_H

#include <map>
#include <string>
#include <exception>
#include "logger.h"

namespace bubi
{
    template<class Index, class Key, class Value, class CompareKey = std::less<Key>, class CompareIndex = std::less<Index>>
    class AtomBatch
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

	    class MapPack
        {
		protected:
            bool   isInit_;
            Index  index_;
	    	mapKV* data_;
	    	mapKV* buff_;
            AtomBatch* env_;

	    public:
            MapPack(): isInit_(false), env_(nullptr), data_(nullptr), buff_(nullptr){}

            MapPack(AtomBatch* env, const Index* index): env_(env), index_(*index), isInit_(false), data_(nullptr), buff_(nullptr)
            {
                if(env && index)
                    isInit_ == true;
            }

            bool IsInit(){ return isInit_; }

            bool Init(AtomBatch* env, const Index* index)
            {
				if ((!env) || (!index))
					return false;

                env_    = env;
                index_  = *index;
				isInit_ = true;

				return true;
            }

	    	bool Set(const Key& key, const Value& value)
            {
                bool ret = false;

                try{ ret = SetValue(key, value); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("set exception, detail: %s", e.what());
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
                }

                return ret;
            }

	    	bool Del(const Key& key)
            {
                bool ret = false;

                try{ ret = DelValue(key); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("delete exception, detail: %s", e.what());
                }

                return ret;
            }

			virtual bool GetFromDB(const Key& key, Value& value){ return false; }
			virtual bool UpdateToDB(){ return false; }

        private:
            bool ConnectData()
            {
				if (!IsInit())
					return false;

                if(!data_)
                    data_ = env_->GetData(index_);

				return true;
            }

            bool ConnectBuff()
            {
				if (!IsInit())
					return false;

                if(!buff_)
                    buff_ = env_->GetBuff(index_);

				return true;
            }

	    	bool GetValue(const Key& key, Value& value)
	    	{
                if((ConnectBuff() == false) || (ConnectData() == false))
                    return false;

	    		bool ret = false;
	    		auto itBuf = buff_->find(key);
	    		if (itBuf != buff_->end())
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
	    			auto itData = data_->find(key);
	    			if (itData != data_->end())
	    			{
	    				if (itData->second.first != DEL)
	    				{
	    					(*buff_)[key] = itData->second;
	    					value = (*buff_)[key].second;
	    					ret = true;
	    				}
	    				//else ret = false;
	    			}
	    			else
	    			{
	    				if (GetFromDB(key, value))
	    				{
	    					(*buff_)[key] = action(ADD, value);
	    					ret = true;
	    				}
	    				//else ret = false;
	    			}
	    		}

	    		return ret;
	    	}

	    	bool SetValue(const Key& key, const Value& value)
	    	{
                if((ConnectBuff() == false) || (ConnectData() == false))
                    return false;

	    		if (data_->find(key) == data_->end())
	    			(*buff_)[key] = action(ADD, value);
	    		else
	    			(*buff_)[key] = action(MOD, value);

	    		return true;
	    	}

	    	bool DelValue(const Key& key)
	    	{
                if((ConnectBuff() == false) || (ConnectData() == false))
                    return false;

	    		(*buff_)[key] = action(DEL, Value());
	    		return true;
	    	}
	    };

    private:
        std::map<Index, mapKV, CompareIndex> actionBuf_;
        std::map<Index, mapKV, CompareIndex> revertBuf_;
		std::map<Index, mapKV, CompareIndex> totalData_;

	protected:
        std::map<Index, MapPack, CompareIndex> entries_;

    public:
        mapKV* GetBuff(const Index& index)
        {
            if(actionBuf_.find(index) == actionBuf_.end())
                actionBuf_[index] = mapKV();

            return &actionBuf_[index];
        }

        mapKV* GetData(const Index& index)
        {
            if(totalData_.find(index) == totalData_.end())
                totalData_[index] = mapKV();

            return &totalData_[index];
        }

        bool Set(const Index& index, MapPack& obj)
        {
            if(!obj)
                return false;

            entries_[index] = obj;

            if(!obj->IsInit())
                obj->Init(this, index);

            return true;
        }

        bool Get(const Index& index, MapPack& obj)
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
                    entries_[index] = obj; 
                    ret = true;
                }
            }

            return ret;
        }

        bool Commit()
        {
            for(auto indexBuf : actionBuf_)
            {
                const Index& index = indexBuf.first;
                const mapKV& mkv   = indexBuf.second;

                for(auto keyAct : mkv)
                {
                    const Key&    key = keyAct.first;
                    const action& act = keyAct.second;

                    if(act.first == ADD)
                        revertBuf_[index][key] = action(REV, Value());
                    else
                        revertBuf_[index][key] = totalData_[index][key];

					totalData_[index][key] = actionBuf_[index][key];
                }
            }

            return true;
        }

        bool UnCommit()
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
                        totalData_[index].erase(key);
                    else
                        totalData_[index][key] = revertBuf_[index][key];
                }
            }
            return true;
        }

        void ClearChange()
        {
            actionBuf_.clear();
            revertBuf_.clear();
        }

		virtual bool GetFromDB(const Index& index, MapPack& obj){ return false; }
		virtual bool UpdateToDB(){ return false; }
    };
}

#endif //TEMPLATE_ATOMIC_BATCH_H
