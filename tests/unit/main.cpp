// doctest generates main() for us - this translation unit exists
// solely to hold DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN exactly once. Test
// cases themselves live in the other tests/unit/*.cpp files.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
