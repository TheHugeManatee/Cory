#include <catch2/catch_session.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Cory.hpp>

#include <Corrade/Utility/Debug.h>

int main(int argc, char *argv[])
{
    std::ostringstream corradeOutput;
    Corrade::Utility::Debug debugRedirect{&corradeOutput};

    Cory::Init();
    Cory::Log::SetCoreLevel(spdlog::level::trace);

    int result = Catch::Session().run(argc, argv);

    return result;
}