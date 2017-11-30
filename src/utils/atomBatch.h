#ifndef TEMPLATE_ATOMIC_BATCH_PROCESS_H
#define TEMPLATE_ATOMIC_BATCH_PROCESS_H

#include <map>
#include <string>
#include <exception>
#include "logger.h"

//#define CATCH_AND_PROCESS_EXCEPTION catch(std::exception& e){ LOG_ERROR(e.what()); needCommit_ = false; }

namespace bubi
{
    template<class KEY, class OBJ, class COMPARE = std::less<KEY>>
    class AtomBatch{

        public:
            enum actType
            {
                ADD = 0,
                MOD = 1,
                DEL = 2,
                GET = 3,
				REV = 4,
				MAX,
            };

			struct CacheValue
			{
				OBJ value_;
				actType type_;
				CacheValue(actType type = GET) :type_(type){}
				CacheValue(const OBJ& obj, actType type = GET) :value_(obj), type_(type){}
			};

            typedef std::map<KEY, CacheValue, COMPARE> mapKC;

		protected:
            bool needCommit_;
            bool committed_;
			bool uncommitted_;
            mapKC actionBuf_;
            mapKC revertBuf_;
            mapKC dataCopy_;
            mapKC data_;

        public:
            AtomBatch(): needCommit_(true), committed_(false), uncommitted_(false){}

			virtual ~AtomBatch(){}

            void Reset()
            {
                needCommit_  = true;
                committed_   = false;
				uncommitted_ = false;

				actionBuf_.clear();
				revertBuf_.clear();
				dataCopy_.clear();
            }

			bool Set(const KEY& key, const OBJ& obj)
			{
                if(!needCommit_)
                    return false;

				try{ SetObj(key, obj); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("set exception, detail: %s", e.what());
                    needCommit_ = false;
                }

			    return needCommit_;
			}

            bool Get(const KEY& key, OBJ& obj)
            {
                bool ret = true;

                try{ ret = GetObj(key, obj); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("get exception, detail: %s", e.what());
                    needCommit_ = false;
                }
                return ret;

            }

            bool Del(const KEY& key)
            {
                if(!needCommit_)
                    return false;

                try{ actionBuf_[key] = CacheValue(DEL); }
                catch(std::exception& e)
                { 
                    LOG_ERROR("delete exception, detail: %s", e.what());
                    needCommit_ = false;
                }

                return needCommit_;
            }

            bool Commit()
            {
                if(!needCommit_)
                    return false;

                if(committed_)
                    return true;

                try
                {
					for (auto act : actionBuf_)
					{
						if(act.second.type_ == ADD)
							revertBuf_[act.first] = CacheValue(REV); //if type_ == REV when UnCommit, data_.erase(key)
						else
							revertBuf_[act.first] = data_[act.first];

						data_[act.first] = act.second; //include type_ == DEL(UpdateToDB will use DEL and key)
					}

                    committed_  = true;
					needCommit_ = false;
                }
                catch(std::exception& e)
                { 
                    LOG_ERROR("commit exception, detail: %s", e.what());
                }

                return committed_;
            }

			bool UnCommit()
			{
                if((!committed_) || uncommitted_)
                    return true;

				try
				{
					for (auto rev : revertBuf_)
					{
						if (rev.second.type_ == REV)
							data_.erase(rev.first);
						else
							data_[rev.first] = rev.second;
					}

					uncommitted_ = true;
				}
                catch(std::exception& e)
                { 
                    LOG_ERROR("uncommit exception, detail: %s", e.what());
                    uncommitted_ = false;
                }

                return uncommitted_;
			}

            bool CopyCommit()
            {
				if (!needCommit_)
					return false;

				if (committed_)
					return true;

                try
                {
                    dataCopy_ = data_;
					for (auto act : actionBuf_)
					{
						dataCopy_[act.first] = act.second;
					}

                    data_.swap(dataCopy_);
					committed_  = true;
					needCommit_ = false;
                }
				catch (std::exception& e)
				{
					LOG_ERROR("copy commit exception, detail: %s", e.what());
					ret = false;
				}

                return committed_;
            }

            bool UnCopyCommit()
            {
                if((!committed_) || uncommitted_)
                    return true;

                data_.swap(dataCopy_);
				uncommitted_ = true;

                return true;
            }

            virtual bool GetFromDB(const KEY& key, OBJ& obj){ return false; }

			virtual void updateToDB(){}

        private:

			void SetObj(const KEY& key, const OBJ& obj)
            {
				if (data_.find(key) == data_.end())
					actionBuf_[key] = CacheValue(obj, ADD);
				else
					actionBuf_[key] = CacheValue(obj, MOD);
            }

            bool GetObj(const KEY& key, OBJ& obj)
            {
				bool ret = false;
                auto itAct = actionBuf_.find(key);
                if(itAct != actionBuf_.end())
                {
					if (itAct->second.type_ != DEL)
					{
						obj = itAct->second.value_;
						ret = true;
					}
					//else ret = false;
                }
                else
                {
                    auto itData = data_.find(key);
                    if(itData != data_.end())
                    {
						if (itData->second.type_ != DEL)
						{
							actionBuf_[key] = itData->second;
							obj = actionBuf_[key].value_;
							ret = true;
						}
						//else ret = false;
                    }
                    else
                    {
						actionBuf_[key] = CacheValue(ADD);
						if (GetFromDB(key, actionBuf_[key].value_))
						{
							obj = actionBuf_[key].value_;
							ret = true;
						}
						//else ret = false;
                    }
				}
				return ret;
            }

			//template<typename OPERATE_TYPE>
			//bool Do(const KEY& key, OPERATE_TYPE type)
			//{
			//    OBJ obj;
			//    if(Get(key, obj))
			//    {
			//        (getRef(obj).*func)(type);
			//    }
			//}
    };
}

#endif //TEMPLATE_ATOMIC_BATCH_PROCESS_H
