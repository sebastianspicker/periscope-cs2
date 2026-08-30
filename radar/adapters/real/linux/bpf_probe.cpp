// bpf_probe.cpp — eBPF / bpffs probes.

#include "real/linux/bpf_probe.hpp"
#include "real/platform.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <sys/stat.h>
#endif

namespace real::linux::bpf {

bool mounts_has_bpffs(const std::string& mounts_text,
                      std::string& path_out) noexcept {
    std::istringstream in(mounts_text);
    std::string line;
    while (std::getline(in, line)) {
        // device mountpoint fstype ...
        std::istringstream ls(line);
        std::string dev, mnt, type;
        if (!(ls >> dev >> mnt >> type)) continue;
        if (type == "bpf") {
            path_out = mnt;
            return true;
        }
    }
    // conventional path even if not in mounts text
    if (mounts_text.find("bpf") != std::string::npos) {
        path_out = "/sys/fs/bpf";
        return true;
    }
    return false;
}

Result<BpfStatus> probe() noexcept {
    BpfStatus st;
#if LR_PLATFORM_LINUX
    std::ifstream mounts("/proc/mounts");
    if (mounts) {
        std::ostringstream ss;
        ss << mounts.rdbuf();
        std::string path;
        if (mounts_has_bpffs(ss.str(), path)) {
            st.bpffs_mounted = true;
            st.bpffs_path = path;
        }
    }
    if (!st.bpffs_mounted) {
        struct stat s {};
        if (::stat("/sys/fs/bpf", &s) == 0 && S_ISDIR(s.st_mode)) {
            st.bpffs_mounted = true;
            st.bpffs_path = "/sys/fs/bpf";
            st.notes.push_back("bpffs path exists (may be unmounted empty dir)");
        }
    }
    if (st.bpffs_mounted && !st.bpffs_path.empty()) {
        DIR* d = ::opendir(st.bpffs_path.c_str());
        if (d) {
            while (dirent* e = ::readdir(d)) {
                if (e->d_name[0] == '.') continue;
                st.pinned_objects.emplace_back(e->d_name);
            }
            ::closedir(d);
        }
    }
#  ifdef __NR_bpf
    st.bpf_syscall_known = true;
#  else
    st.bpf_syscall_known = false;
    st.notes.push_back("__NR_bpf not defined at compile time");
#  endif
    return st;
#else
    st.notes.push_back("bpf probe requires Linux");
    return Result<BpfStatus>(st, "probe requires Linux");
#endif
}

} // namespace real::linux::bpf
