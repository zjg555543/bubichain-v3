#ifndef TEMPLATE_ATOMIC_BATCH_PROCESS_H
#define TEMPLATE_ATOMIC_BATCH_PROCESS_H

#include <map>
#include <set>
#include <stack>
#include <vector>
#include <string>
#include <memory>
#include <exception>

#define CATCH_AND_PROCESS_EXCEPTION catch(std::exception& e){ LOG_ERROR(e.what()); needCommit_ = false; }
//#define CATCH_AND_PROCESS_EXCEPTION catch(...){}

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
				DEF,
            };

			struct CacheValue
			{
				OBJ value_;
				actType type_;
				CacheValue(actType type = GET) :type_(type){}
				CacheValue(OBJ obj, actType type = GET) :value_(obj), type_(type){}
			};

            typedef std::map<KEY, OBJ, COMPARE> mapKO;
            typedef std::map<KEY, CacheValue, COMPARE> mapKC;

		protected:
            bool needCommit_;
            bool committed_;
			bool uncommitted_;
            mapKC  actionBuf_;
            mapKC  revertBuf_;
            mapKO  standby_;
            mapKO  dataCopy_;
            mapKO* data_;

        public:
            AtomBatch(): data_(&standby_), needCommit_(true), committed_(false), uncommitted_(false){}

			AtomBatch(mapKO* pData) : needCommit_(true), committed_(false), uncommitted_(false)
			{
                data_ = pData ? pData : &standby_; //avoid new\delete or malloc\free 
			}

			virtual ~AtomBatch(){}

            void ResetCommitFlag()
            {
                needCommit_  = true;
                committed_   = false;
				uncommitted_ = false;
            }

			void ClearBuf()
			{
				actionBuf_.clear();
				revertBuf_.clear();
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
					for (auto itAct = actionBuf_.begin(); itAct != actionBuf_.end(); itAct++)
					{
						switch (itAct->second.type_)
						{
						case ADD:
						{
							revertBuf_[itAct->first] = CacheValue(DEL);
							(*data_)[itAct->first] = itAct->second.value_;
							break;
						}
						case GET:
						case MOD:
						{
							revertBuf_[itAct->first] = CacheValue((*data_)[itAct->first], MOD);
							(*data_)[itAct->first] = itAct->second.value_;
							break;
						}
						case DEL:
						{
							revertBuf_[itAct->first] = CacheValue((*data_)[itAct->first], ADD);
							(*data_).erase(itAct->first);
							break;
						}
						default:
							break;
						}
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
                    
                bool ret = true;

				try
				{
					for (auto itRev = revertBuf_.begin(); itRev != revertBuf_.end(); itRev++)
					{
						switch (itRev->second.type_)
						{
						case ADD:
						{
							(*data_)[itRev->first] = itRev->second.value_;
							break;
						}
						case MOD:
						{
							(*data_)[itRev->first] = itRev->second.value_;
							break;
						}
						case DEL:
						{
							(*data_).erase(itRev->first);
							break;
						}

						default:
							break;
						}
					}
				}
                catch(std::exception& e)
                { 
                    LOG_ERROR("uncommit exception, detail: %s", e.what());
                    ret = false;
                }

				uncommitted_ = true;
                return ret;
			}

            bool CopyCommit()
            {
				if (!needCommit_)
					return false;

				if (committed_)
					return true;

                try
                {
                    dataCopy_ = *data_;

					for (auto itAct = actionBuf_.begin(); itAct != actionBuf_.end(); itAct++)
					{
						switch (itAct->second.type_)
						{
						case ADD:
						case MOD:
						case GET:
						{
							(*data_)[itAct->first] = itAct->second.value_;
							break;
						}
						case DEL:
						{
							(*data_).erase(itAct->first);
							break;
						}
						default
							break;
						}
					}

                    data_->swap(dataCopy_);
					committed_ = true;
					needCommit_ = false;
                }
				catch (std::exception& e)
				{
					LOG_ERROR("uncommit exception, detail: %s", e.what());
					ret = false;
				}

                return committed_;
            }

            bool UnCopyCommit()
            {
                if((!committed_) || uncommitted_)
                    return true;

                data_->swap(dataCopy_);
				uncommitted_ = true;

                return true;
            }

            virtual bool GetFromDB(const KEY& key, OBJ& obj){ return false; }

			virtual void updateToDB(){}

        private:
			void SetObj(const KEY& key, const OBJ& obj)
            {
				if (data_->find(key) == data_->end())
					actionBuf_[key] = CacheValue(obj, ADD);
				else
					actionBuf_[key] = CacheValue(obj, MOD);
            }

            bool GetObj(const KEY& key, OBJ& obj)
            {
				bool ret = true;
                auto itAct = actionBuf_.find(key);
                if(itAct != actionBuf_.end())
                {
					if (itAct->second.type_ != DEL)
						obj = itAct->second.value_;
					else
						ret = false;
                }
                else
                {
                    auto itData = data_->find(key);
                    if(itData != data_->end())
                    {
                        actionBuf_[key] = CacheValue(itData->second, GET);
						obj = actionBuf_[key].value_;
                    }
                    else
                    {
						actionBuf_[key] = CacheValue(GET);
						if (GetFromDB(key, actionBuf_[key].value_))
							obj = actionBuf_[key].value_;
						else
							ret = false;
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
