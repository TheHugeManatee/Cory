#include <catch2/catch_session.hpp>

#include <Cory/Cory.hpp>

int main( int argc, char* argv[] ) {
    Cory::Init();

    int result = Catch::Session().run( argc, argv );

    return result;
}