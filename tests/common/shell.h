#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

/*---------------------------------------------------------*\
| Run the project's shell tools for real, in a scratch HOME |
\*---------------------------------------------------------*/
struct ShellResult
{
    int         code;
    std::string out;
};

inline std::string Root()
{
    const char* env = getenv("REPO_ROOT");

    if(env)
    {
        return env;
    }

    char cwd[4096];

    return getcwd(cwd, sizeof(cwd)) ? cwd : ".";
}

inline ShellResult Sh(const std::string& command)
{
    ShellResult result;
    std::string full = command + " 2>&1";
    FILE* pipe = popen(full.c_str(), "r");
    char buf[512];

    result.out = "";
    result.code = -1;

    if(!pipe)
    {
        return result;
    }

    while(fgets(buf, sizeof(buf), pipe))
    {
        result.out += buf;
    }

    int status = pclose(pipe);
    result.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return result;
}

inline std::string ReadFile(const std::string& path)
{
    std::ifstream in(path);
    std::stringstream ss;

    ss << in.rdbuf();

    return ss.str();
}

inline bool Exists(const std::string& path)
{
    struct stat st;

    return stat(path.c_str(), &st) == 0;
}

inline bool IsExecutable(const std::string& path)
{
    return access(path.c_str(), X_OK) == 0;
}

inline void WriteFile(const std::string& path, const std::string& content, bool executable = false)
{
    Sh("mkdir -p \"$(dirname '" + path + "')\"");

    std::ofstream out(path);
    out << content;
    out.close();

    if(executable)
    {
        chmod(path.c_str(), 0755);
    }
}

inline std::string TempDir()
{
    char tmpl[] = "/tmp/dareu-e2e-XXXXXX";

    return mkdtemp(tmpl);
}

inline bool Contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

/*---------------------------------------------------------*\
| A scratch HOME with a fake `openrgb` that records its      |
| arguments, a fake `omarchy`, and one theme                 |
\*---------------------------------------------------------*/
struct ToolEnv
{
    std::string home;
    std::string log;
    std::string prefix;

    ToolEnv(const std::string& theme_dir, const std::string& colors_toml)
    {
        home    = TempDir();
        log     = home + "/openrgb.args";

        WriteFile(home + "/bin/openrgb", "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$FAKE_LOG\"\n", true);
        WriteFile(home + "/bin/omarchy", "#!/bin/sh\n[ \"$1 $2\" = \"theme current\" ] && echo \"$FAKE_THEME\"\n", true);

        if(!theme_dir.empty())
        {
            WriteFile(home + "/.config/omarchy/themes/" + theme_dir + "/colors.toml", colors_toml);
        }

        /* XDG_CONFIG_HOME is pinned here too: the ambient one (set by the desktop session
           this suite happens to run under) would otherwise leak into the colour-profile
           path apply-theme.sh resolves, breaking the scratch HOME's isolation. */
        prefix = "env HOME='" + home + "' XDG_CONFIG_HOME='" + home + "/.config' PATH='" + home + "/bin:" +
                 getenv("PATH") + "' OPENRGB='" + home + "/bin/openrgb' COLOR_TRANSFORM='" + Root() +
                 "/tools/color_transform.py' FAKE_LOG='" + log + "' FAKE_THEME='Tokyo Night' ";
    }

    ~ToolEnv()
    {
        Sh("rm -rf '" + home + "'");
    }

    /* openrgb's arguments, one per line joined with | */
    std::string Args()
    {
        std::string text = ReadFile(log);
        std::string out;

        for(char c : text)
        {
            out += (c == '\n') ? '|' : c;
        }

        return out;
    }
};
