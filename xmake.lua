-- include subprojects
includes("lib/commonlibsf")

-- set project constants
set_project("OSF Defeat")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
-- target name == MO2 mod folder: deploy goes to XSE_SF_MODS_PATH\<target name>\SFSE\Plugins
target("OSF Defeat")
    add_rules("commonlibsf.plugin", {
        name = "OSF Defeat",  -- plugin name -> SFSE log file (OSFDefeat.log)
        author = "ozooma10",
        description = "Native defeat / death-alternative framework for Starfield",
        email = "98544147+ozooma10@users.noreply.github.com"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")

    -- deploy the built plugin + symbols into the MO2 mod folder after each build
    after_build(function(target)
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
                os.trycp(src, path.join(plugindir, path.filename(src)))
                cprint("${green}[OSF Defeat] copied:${clear} %s", path.filename(src))
            end
        end
    end)
