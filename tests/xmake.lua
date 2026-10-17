-- Real production core, isolated from CommonLib relocations and deployment rules.
target("osfdefeat-test-core")
    set_kind("static")
    set_default(false)
    set_group("tests")
    add_includedirs("native/stubs", "native", "../src", {public = true})
    add_packages("nlohmann_json", {public = true})
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", {public = true})
    add_files("../src/Core/DefeatRegistry.cpp", "../src/Core/Config.cpp",
        "../src/Core/Conditions.cpp", "../src/Core/Consequence.cpp",
        "../src/Core/Truce.cpp", "../src/Core/Defeat.cpp", "../src/Core/Recovery.cpp", "../src/Core/Capture.cpp",
        "../src/Core/Persistence.cpp", "../src/Core/RuntimeMutation.cpp",
        "../src/Core/Eligibility.cpp", "../src/Core/Decision.cpp",
        "../src/Core/DefeatContext.cpp", "../src/Core/Events.cpp", "native/Doubles.cpp", "native/DamageDriver.cpp")
target_end()

-- Instrumentation exists only in this explicitly selected, privately staged DLL.
rule("osfdefeat.test_install")
    on_config(function(target)
        target:set("installdir", path.join(os.projectdir(), "build", "tests", "runtime-install"))
        target:set("installfiles", {})
    end)
rule_end()
target("osfdefeat-runtime-tests")
    set_default(false)
    add_rules("commonlibsf.plugin", {
        name = "OSF Defeat", author = "ozooma10", description = "OSF Defeat runtime test build"
    })
    add_rules("osfdefeat.test_install")
    set_basename("OSF Defeat")
    set_targetdir("$(builddir)/tests/runtime")
    set_installdir("$(builddir)/tests/runtime-install")
    add_files("../src/**.cpp", "runtime/DefeatRuntimeTests.cpp", "runtime/AcceptanceTests.cpp")
    add_includedirs("../src", "runtime")
    add_packages("nlohmann_json")
    add_defines("OSF_DEFEAT_TESTING", "NOMINMAX", "WIN32_LEAN_AND_MEAN", "_CRT_SECURE_NO_WARNINGS")
    set_pcxxheader("../src/pch.h")
target_end()

local suites = {"registry", "config", "conditions", "consequence", "state", "recovery", "events", "persistence", "damage"}
for _, suite in ipairs(suites) do
    target("osfdefeat-" .. suite .. "-tests")
        set_kind("binary")
        set_default(false)
        set_group("tests")
        set_targetdir("$(builddir)/tests")
        add_deps("osfdefeat-test-core")
        add_files("native/" .. suite .. "_tests.cpp", "native/Main.cpp")
        add_tests("default")
    target_end()
end

target("osfdefeat-tests")
    set_kind("phony")
    set_default(false)
    for _, suite in ipairs(suites) do
        add_deps("osfdefeat-" .. suite .. "-tests")
    end
target_end()
