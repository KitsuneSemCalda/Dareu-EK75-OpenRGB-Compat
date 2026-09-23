/*---------------------------------------------------------*\
| End-to-end tests of the shell tools, the udev rule and    |
| the probe script. They run for real in a scratch HOME with |
| a fake openrgb, so nothing on the machine is touched.      |
\*---------------------------------------------------------*/
#include "cest.h"

#include <string>
#include "DareuEK75Controller.h"
#include "helpers.h"
#include "shell.h"

#define THEME_TOKYO "accent = \"#7aa2f7\"\nbackground = \"#1a1b26\"\n"

static std::string Tool(const std::string& name)
{
    return Root() + "/tools/" + name;
}

static std::string Hex(unsigned int v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%04x", v);

    return buf;
}

void run_tools_tests()
{
    describe("apply-theme.sh", {
        it("sets the accent colour of the named theme, keeping whatever mode is active", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " 'Tokyo Night'");

            expect(r.code).toEqual(0);
            expect(env.Args()).toEqual("--noautoconnect|-d|Dareu EK75|-c|7aa2f7|");
            expect(r.out).toContain("EK75 -> #7aa2f7 (Tokyo Night, accent)");
        });

        it("turns a name with capitals and spaces into the theme directory", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " 'TOKYO NIGHT'");

            expect(r.code).toEqual(0);
        });

        it("uses the current Omarchy theme when no name is given", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh"));

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|7aa2f7|");
        });

        it("picks another key with COLOR_KEY", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + "COLOR_KEY=background " + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|1a1b26|");
        });

        it("accepts a colour written without the hash", {
            ToolEnv env("plain", "accent = \"FFAA00\"\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " plain");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|FFAA00|");
        });

        it("fails without touching the keyboard when the theme does not exist", {
            ToolEnv env("", "");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " nope");

            expect(r.code).toEqual(1);
            expect(r.out).toContain("theme 'nope' not found");
            expect(Exists(env.log)).toBeFalsy();
        });

        it("fails when the theme has no such colour", {
            ToolEnv env("bare", "mode = \"dark\"\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " bare");

            expect(r.code).toEqual(1);
            expect(r.out).toContain("no 'accent'");
            expect(Exists(env.log)).toBeFalsy();
        });

        it("rejects a colour that is not six hex digits", {
            ToolEnv env("short", "accent = \"#abc\"\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " short");

            expect(r.code).toEqual(1);
            expect(Exists(env.log)).toBeFalsy();
        });

        it("prefers keyboard.rgb over colors.toml when the theme has one", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/omarchy/themes/tokyo-night/keyboard.rgb", "#c0ffee\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|c0ffee|");
            expect(r.out).toContain("keyboard.rgb");
        });

        it("falls back to COLOR_KEY when the theme has no keyboard.rgb", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + "COLOR_KEY=background " + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|1a1b26|");
        });

        it("rejects a keyboard.rgb that is not six hex digits", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/omarchy/themes/tokyo-night/keyboard.rgb", "not-a-colour\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(1);
            expect(Exists(env.log)).toBeFalsy();
        });

        it("does not run the colour transform when there is no profile file", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toEqual("--noautoconnect|-d|Dareu EK75|-c|7aa2f7|");
        });

        it("runs the colour transform and sends brightness when a profile file exists", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/dareu-ek75/color-profile.toml",
                      "[keyboard]\nbrightness = 60\n\n[channels]\nred_gain = 1.2\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-b|60|");
            expect(Contains(env.Args(), "-c|7aa2f7|")).toBeFalsy();
        });

        it("a neutral profile file leaves the colour unchanged and adds no brightness", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/dareu-ek75/color-profile.toml", "[color]\nmax_chroma = 5.0\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toEqual("--noautoconnect|-d|Dareu EK75|-c|7aa2f7|");
        });

        it("fails without touching the keyboard when the profile file is malformed", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/dareu-ek75/color-profile.toml", "this is not [ valid toml");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(1);
            expect(Exists(env.log)).toBeFalsy();
        });

        it("fails without touching the keyboard when the profile brightness is out of range", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/dareu-ek75/color-profile.toml", "[keyboard]\nbrightness = 150\n");
            ShellResult r = Sh(env.prefix + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(1);
            expect(r.out).toContain("brightness");
            expect(Exists(env.log)).toBeFalsy();
        });

        it("respects DAREU_COLOR_PROFILE to point at another profile file", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            std::string profile = env.home + "/alt-profile.toml";
            WriteFile(profile, "[keyboard]\nbrightness = 42\n");
            ShellResult r = Sh(env.prefix + "DAREU_COLOR_PROFILE='" + profile + "' " + Tool("apply-theme.sh") + " tokyo-night");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-b|42|");
        });
    });

    describe("install-hook.sh", {
        it("installs an executable copy of apply-theme.sh", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + Tool("install-hook.sh"));
            std::string hook = env.home + "/.config/omarchy/hooks/theme-set.d/dareu-ek75";

            expect(r.code).toEqual(0);
            expect(IsExecutable(hook)).toBeTruthy();
            expect(ReadFile(hook)).toEqual(ReadFile(Tool("apply-theme.sh")));
        });

        it("also installs the colour-transform helper the hook depends on", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            ShellResult r = Sh(env.prefix + Tool("install-hook.sh"));
            std::string lib = env.home + "/.local/lib/dareu-ek75/color_transform.py";

            expect(r.code).toEqual(0);
            expect(IsExecutable(lib)).toBeTruthy();
            expect(ReadFile(lib)).toEqual(ReadFile(Tool("color_transform.py")));
        });

        it("makes the hook apply the theme Omarchy passes to it, not the current one", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/.config/omarchy/themes/dusk/colors.toml", "accent = \"#c0ffee\"\n");
            Sh(env.prefix + Tool("install-hook.sh"));
            ShellResult r = Sh(env.prefix + env.home + "/.config/omarchy/hooks/theme-set.d/dareu-ek75 Dusk");

            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|c0ffee|");
        });

        it("can be run twice", {
            ToolEnv env("tokyo-night", THEME_TOKYO);

            expect(Sh(env.prefix + Tool("install-hook.sh")).code).toEqual(0);
            expect(Sh(env.prefix + Tool("install-hook.sh")).code).toEqual(0);
        });
    });

    describe("install-launcher.sh", {
        it("runs the installed hook after removing the source binary and scripts", {
            ToolEnv env("tokyo-night", THEME_TOKYO);
            WriteFile(env.home + "/system.desktop", "[Desktop Entry]\nExec=/usr/bin/openrgb\n");
            std::string checkout = env.home + "/checkout";
            expect(Sh("mkdir -p '" + checkout + "/tools' && cp '" + Root() + "/tools/'*.sh '" + Root() +
                       "/tools/color_transform.py' '" + checkout + "/tools/'").code).toEqual(0);
            expect(Sh(env.prefix + "SYSTEM_ENTRY='" + env.home + "/system.desktop' " + checkout + "/tools/install-launcher.sh").code).toEqual(0);
            expect(Sh(env.prefix + checkout + "/tools/install-hook.sh").code).toEqual(0);
            expect(Sh("rm -rf '" + checkout + "' '" + env.home + "/bin/openrgb'").code).toEqual(0);
            ShellResult r = Sh(env.prefix + "env -u OPENRGB " + env.home + "/.config/omarchy/hooks/theme-set.d/dareu-ek75");
            expect(r.code).toEqual(0);
            expect(env.Args()).toContain("-c|7aa2f7|");
        });

        it("points the menu entry at the installed copy and keeps the other lines", {
            ToolEnv env("", "");
            WriteFile(env.home + "/system.desktop", "[Desktop Entry]\nName=OpenRGB\nExec=/usr/bin/openrgb\nIcon=openrgb\n");
            ShellResult r = Sh(env.prefix + "SYSTEM_ENTRY='" + env.home + "/system.desktop' " + Tool("install-launcher.sh"));
            std::string entry = ReadFile(env.home + "/.local/share/applications/org.openrgb.OpenRGB.desktop");

            expect(r.code).toEqual(0);
            expect(entry).toContain("Exec=" + env.home + "/.local/lib/dareu-ek75/openrgb\n");
            expect(entry).toContain("Name=OpenRGB");
            expect(entry).toContain("Icon=openrgb");
            expect(Contains(entry, "/usr/bin/openrgb")).toBeFalsy();
        });

        it("updates an existing login autostart entry", {
            ToolEnv env("", "");
            WriteFile(env.home + "/system.desktop", "[Desktop Entry]\nExec=/usr/bin/openrgb\n");
            WriteFile(env.home + "/.config/autostart/OpenRGB.desktop", "[Desktop Entry]\nExec=/usr/bin/openrgb --startminimized\n");
            Sh(env.prefix + "SYSTEM_ENTRY='" + env.home + "/system.desktop' " + Tool("install-launcher.sh"));

            expect(ReadFile(env.home + "/.config/autostart/OpenRGB.desktop")).toContain("Exec=" + env.home + "/.local/lib/dareu-ek75/openrgb\n");
        });

        it("does not create an autostart entry that was not there", {
            ToolEnv env("", "");
            WriteFile(env.home + "/system.desktop", "[Desktop Entry]\nExec=/usr/bin/openrgb\n");
            Sh(env.prefix + "SYSTEM_ENTRY='" + env.home + "/system.desktop' " + Tool("install-launcher.sh"));

            expect(Exists(env.home + "/.config/autostart/OpenRGB.desktop")).toBeFalsy();
        });

        it("asks for a build first when the binary is missing", {
            ToolEnv env("", "");
            ShellResult r = Sh(env.prefix + "OPENRGB='" + env.home + "/missing' " + Tool("install-launcher.sh"));

            expect(r.code).toEqual(1);
            expect(r.out).toContain("run tools/build.sh");
        });
    });

    describe("build.sh --sync-only", {
        it("copies the driver into the OpenRGB tree and drops stale files", {
            ToolEnv env("", "");
            std::string tree = env.home + "/OpenRGB";
            WriteFile(tree + "/Controllers/DareuEK75Controller/stale.txt", "old");
            ShellResult r = Sh(env.prefix + "OPENRGB_DIR='" + tree + "' " + Tool("build.sh") + " --sync-only");
            ShellResult d = Sh("diff -r '" + Root() + "/src/DareuEK75Controller' '" + tree + "/Controllers/DareuEK75Controller'");

            expect(r.code).toEqual(0);
            expect(d.code).toEqual(0);
            expect(Exists(tree + "/Controllers/DareuEK75Controller/stale.txt")).toBeFalsy();
        });
    });

    describe("install.sh", {
        it("runs every installer, udev first and the hook last", {
            std::string script = ReadFile(Root() + "/install.sh");
            size_t udev = script.find("tools/install-udev.sh");
            size_t build = script.find("tools/build.sh");
            size_t launcher = script.find("tools/install-launcher.sh");
            size_t hook = script.find("tools/install-hook.sh");

            expect(udev != std::string::npos).toBeTruthy();
            expect(udev < build).toBeTruthy();
            expect(build < launcher).toBeTruthy();
            expect(launcher < hook).toBeTruthy();
        });

        it("only refers to scripts that exist and are executable", {
            std::string script = ReadFile(Root() + "/install.sh");

            for(std::string name : { "install-udev.sh", "build.sh", "install-launcher.sh", "install-hook.sh" })
            {
                expect(Contains(script, "tools/" + name)).toBeTruthy();
                expect(IsExecutable(Tool(name))).toBeTruthy();
            }
        });
    });

    describe("udev rule", {
        it("matches the USB ids the driver detects, for usb and hidraw", {
            std::string rule = ReadFile(Root() + "/udev/70-dareu-ek75.rules");
            std::string vid = "idVendor}==\"" + Hex(DAREU_VID) + "\"";

            expect(rule).toContain(vid);
            expect(rule).toContain("idProduct}==\"" + Hex(DAREU_EK75_RECEIVER_PID) + "\"");
            expect(rule).toContain("idProduct}==\"" + Hex(DAREU_EK75_WIRED_PID) + "\"");
            expect(rule).toContain("KERNEL==\"hidraw*\"");
            expect(rule).toContain("SUBSYSTEMS==\"usb\"");
        });

        it("grants access with uaccess and covers both wired PIDs the project knows about", {
            std::string rule = ReadFile(Root() + "/udev/70-dareu-ek75.rules");

            expect(rule).toContain("TAG+=\"uaccess\"");
            expect(rule).toContain("\"0045\"");
            expect(rule).toContain("\"0101\"");
        });

        it("is named to run before 73-seat-late, where uaccess becomes an ACL", {
            expect(Exists(Root() + "/udev/70-dareu-ek75.rules")).toBeTruthy();
            expect(IsExecutable(Root() + "/udev/70-dareu-ek75.rules")).toBeFalsy();
        });

        it("installer references that same file", {
            expect(ReadFile(Tool("install-udev.sh"))).toContain("70-dareu-ek75.rules");
        });

        it("is accepted by udevadm verify when it is available", {
            if(Sh("command -v udevadm").code == 0)
            {
                ShellResult r = Sh("udevadm verify '" + Root() + "/udev/70-dareu-ek75.rules'");
                expect(r.code).toEqual(0);
            }
        });
    });

    describe("probe.py", {
        it("has a valid syntax", {
            expect(Sh("python3 -m py_compile '" + Tool("probe.py") + "'").code).toEqual(0);
        });

        it("builds the same packet as the driver", {
            g_receiver.Reset();
            DareuEK75Device dev((hid_device*)&g_receiver, "p");
            dev.Connect();
            g_receiver.sent.clear();
            unsigned char level;
            dev.GetBrightness(4, level);

            std::string driver;
            for(int i = 0; i < 7; i++)
            {
                char b[8];
                snprintf(b, sizeof(b), "%02x", g_receiver.sent.back()[i]);
                driver += b;
            }

            std::string script =
                "import sys; sys.path.insert(0, '" + Root() + "/tools'); import probe\n"
                "sent = []\n"
                "def fake(fd, req, buf, *a):\n"
                "    if req == probe._ioc(0x06, 65):\n"
                "        sent.append(bytes(buf))\n"
                "    else:\n"
                "        buf[1] = 0x02\n"
                "probe.fcntl.ioctl = fake\n"
                "probe.xfer(0, 0x10, 2, probe.CLASS_LIGHTING, probe.LED_BRIGHTNESS | probe.GET, 1, bytes([4]))\n"
                "print(sent[0][1:8].hex())\n";
            std::string dir = TempDir();
            WriteFile(dir + "/probe_check.py", script);
            ShellResult r = Sh("python3 '" + dir + "/probe_check.py'");
            Sh("rm -rf '" + dir + "'");

            expect(r.code).toEqual(0);
            expect(r.out).toContain(driver);
        });

        it("uses the hidraw feature report ioctls for a 65 byte buffer", {
            std::string script =
                "import sys; sys.path.insert(0, '" + Root() + "/tools'); import probe\n"
                "print(hex(probe._ioc(0x06, 65)), hex(probe._ioc(0x07, 65)))\n";
            std::string dir = TempDir();
            WriteFile(dir + "/ioc_check.py", script);
            ShellResult r = Sh("python3 '" + dir + "/ioc_check.py'");
            Sh("rm -rf '" + dir + "'");

            expect(r.out).toContain("0xc0414806 0xc0414807");
        });
    });
}
