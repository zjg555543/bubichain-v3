
#include "general.h"

namespace bubi {
	std::string GetDataSecuretKey() {
		//key must be string, ended with 0, length must be 32 + 1.
		char key[] = { 'H', 'C', 'P', 'w', 'z', '!', 'H', '1', 'Y', '3', 'j', 'a', 'J', '*', '|', 'q', 'w', '8', 'K', '<', 'e', 'o', '7', '>', 'Q', 'i', 'h', ')', 'r', 'P', 'q', '1', 0 };		
		return key;
	}
}
