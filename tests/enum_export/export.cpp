#include "EnumExport.h"

#define VRT_ENUM_FILE "FixtureEnums.def"
#define VRT_ENUM_HEADER "Fixture.h"
#define VRT_ENUM_CATALOG fixture
#include "EnumCatalog.h"

#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
#else
int main(int argc, char **argv)
#endif
{
    return visionrt::codegen::run(argc, argv, {fixture::enums});
}
