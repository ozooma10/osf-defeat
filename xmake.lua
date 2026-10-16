-- include subprojects
includes("lib/commonlibsf")

-- CommonLib's default after-build action installs every default target, even when
-- building a non-default test DLL. Scope installation to this production target.
rule("commonlib.plugin")
    after_build(function(target)
        if target:name() == "OSF Defeat" then
            import("core.project.task")
            task.run("install", {target = target:name()})
        end
    end)
rule_end()

-- CommonLib's plugin rule automatically runs its install action after a rebuild. Apply this rule
-- after commonlibsf.plugin so verification builds stage that mandatory install inside the repo.
rule("osfdefeat.install_target")
    on_config(function(target)
        if os.getenv("OSF_DEFEAT_SKIP_DEPLOY") == "1" then
            target:set("installdir", path.join(os.projectdir(), "build", ".verify-mods", target:name()))
        end
    end)
rule_end()

set_project("OSF Defeat")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("nlohmann_json")

includes("tests")

target("OSF Defeat")
    add_rules("commonlibsf.plugin", {
        name = "OSF Defeat",
        author = "ozooma10",
        description = "Native defeat framework foundation for Starfield",
        email = "98544147+ozooma10@users.noreply.github.com"
    })
    add_rules("osfdefeat.install_target")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    add_packages("nlohmann_json")
    set_pcxxheader("src/pch.h")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", "_CRT_SECURE_NO_WARNINGS")

    after_build(function(target)
        if os.getenv("OSF_DEFEAT_SKIP_DEPLOY") == "1" then
            cprint("${yellow}[OSF Defeat] OSF_DEFEAT_SKIP_DEPLOY=1; skipping post-build deployment")
            return
        end
        local modsroot = os.getenv("XSE_SF_MODS_PATH")
        local gameroot = os.getenv("XSE_SF_GAME_PATH")
        local plugindir = nil

        if modsroot and #modsroot > 0 then
            plugindir = path.join(modsroot, target:name(), "SFSE", "Plugins")
        elseif gameroot and #gameroot > 0 then
            plugindir = path.join(gameroot, "Data", "SFSE", "Plugins")
        else
            cprint("${yellow}[OSF Defeat] no XSE_SF_MODS_PATH/XSE_SF_GAME_PATH set; skipping post-build copy")
            return
        end

        os.mkdir(plugindir)
        cprint("${bright cyan}[OSF Defeat] copying build outputs to %s", plugindir)
        for _, src in ipairs({ target:targetfile(), target:symbolfile() }) do
            if src and os.isfile(src) then
                os.cp(src, path.join(plugindir, path.filename(src)))
                cprint("${green}[OSF Defeat] copied:${clear} %s", path.filename(src))
            end
        end

        local scriptroot = (modsroot and #modsroot > 0) and path.join(modsroot, target:name(), "Scripts")
                           or path.join(gameroot, "Data", "Scripts")
        for _, pex in ipairs(os.files(path.join(os.projectdir(), "dist", "Scripts", "*.pex"))) do
            os.mkdir(scriptroot)
            os.cp(pex, path.join(scriptroot, path.filename(pex)))
            cprint("${green}[OSF Defeat] deployed script:${clear} %s", path.filename(pex))
        end
        for _, psc in ipairs(os.files(path.join(os.projectdir(), "dist", "Scripts", "Source", "*.psc"))) do
            os.mkdir(path.join(scriptroot, "Source"))
            os.cp(psc, path.join(scriptroot, "Source", path.filename(psc)))
        end
        local dataroot = (modsroot and #modsroot > 0) and path.join(modsroot, target:name())
                         or path.join(gameroot, "Data")
        for _, config in ipairs(os.files(path.join(os.projectdir(), "dist", "SFSE", "OSFDefeat", "**"))) do
            local rel = path.relative(config, path.join(os.projectdir(), "dist"))
            local dst = path.join(dataroot, rel)
            os.mkdir(path.directory(dst))
            os.cp(config, dst)
            cprint("${green}[OSF Defeat] deployed config:${clear} %s", rel)
        end
        -- OSF Animation scene defs (Data/OSF/*.osf.json) — the player down-state cower scene.
        for _, scene in ipairs(os.files(path.join(os.projectdir(), "dist", "OSF", "**"))) do
            local rel = path.relative(scene, path.join(os.projectdir(), "dist"))
            local dst = path.join(dataroot, rel)
            os.mkdir(path.directory(dst))
            os.cp(scene, dst)
            cprint("${green}[OSF Defeat] deployed scene:${clear} %s", rel)
        end
    end)
