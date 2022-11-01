#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <Cory/Cory.hpp>

int main( int argc, char* argv[] ) {
    Cory::Init();

    int result = Catch::Session().run( argc, argv );

    // your clean-up...

    return result;
}