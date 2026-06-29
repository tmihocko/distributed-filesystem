#include "Singleton.h"
#include <string>

class DiskManager : Singleton<DiskManager> {
	void read_page();
};