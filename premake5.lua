-- premake5.lua
workspace "voxium"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "voxium"


    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    project "voxium"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        staticruntime "off"
        vectorextensions "AVX2"
        systemversion "latest"
    
        targetdir ("bin/%{prj.name}-%{cfg.buildcfg}/out")
        objdir ("bin/%{prj.name}-%{cfg.buildcfg}/int")
    
        includedirs {
            "include",

            "vendor/simdjson/include",
            "vendor/glm",
        }
    
        files {
            "src/**.cpp"
        }
    
        links {
            "simdjson"
        }

        flags {
            "MultiProcessorCompile",
            "FatalWarnings"
        }
    
        filter "configurations:Debug"
            defines {
                "_DEBUG"
            }
            runtime "Debug"
            optimize "off"
            symbols "on"
            sanitize "Address"
        
        filter "configurations:Release"
            runtime "Release"
            optimize "speed"
            symbols "off"
            flags {
                "LinkTimeOptimization"
            }
    
group "Dependencies"
    include "vendor/support/simdjson"
group ""
    
