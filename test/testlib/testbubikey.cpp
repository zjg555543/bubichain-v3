#include <common/private_key.h>


void test_bubikey(){


    bubi::PrivateKey skey(bubi::SIGNTYPE_ED25519);
	std::string strpkey = skey.GetEncPublicKey();
	for (int i = 0; i < 10000; i++)
	{
		//bubi::PublicKey pkey(strpkey);
		std::string sig = skey.Sign("hello");
		
		//auto ppp = pkey.GetBase58PublicKey();

		assert(bubi::PublicKey::Verify("hello", sig, strpkey));
		//auto addr1 = skey.GetBase58Address();
		//auto addr2 = pkey.GetBase58Address();
		//assert(addr1 == addr2);
		printf("%d\n", i);
	}


}