#include "haos_emulation.h"
#include "haos.h"
#include "haos_api.h"
#include "odt_modules.h"

int main (int argc, const char * argv[])
{
	HAOS::init(argc, argv);
	HAOS::addModules(ODT::getMasterTable());
	HAOS::run();
}
