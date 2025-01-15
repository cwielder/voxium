project "simdjson"
    kind "StaticLib"
    language "C++"
	cppdialect "C++20"
    staticruntime "off"
    vectorextensions "AVX2"

    targetdir ("bin/%{prj.name}-%{cfg.buildcfg}/out")
    objdir ("bin/%{prj.name}-%{cfg.buildcfg}/int")

    includedirs {
        "../../simdjson/include",
        "../../simdjson/src"
    }

    files {
        "../../simdjson/src/simdjson.cpp"
    }

    filter "configurations:Debug"
        defines "DEBUG"
        runtime "Debug"
        optimize "off"
        symbols "on"
    
    filter "configurations:Release"
        defines "NDEBUG"
        runtime "Release"
        optimize "speed"
        symbols "off"
