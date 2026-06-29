#include "Singleton.h"
#include <string>

class DiskManager : Singleton<DiskManager> {
	void select(std::string name);

	void create_table();
};