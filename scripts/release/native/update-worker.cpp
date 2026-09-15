// Separate process: the pinned SDK's stop() is not implemented. The parent
// cancels this process group, preserving its own renderer and the installed file.
#include <appimage/update.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <regex>
static void report(const std::string& value) {
    const std::string line="GUIPPER_UPDATE "+value+"\n";
    // Protocol on fd 3; SDK stdout/stderr are not interpreted as commands.
    if (::write(3,line.data(),line.size())<0) std::exit(1);
}
int main(int argc,char** argv) {
    if (argc!=4) return 2;
    try {
        std::filesystem::current_path(argv[3]);
        // Same-filesystem hard link: no full copy and no SDK writes alongside
        // users' other files. SDK downloads and backups stay in this private dir.
        const auto installed=std::filesystem::current_path()/"installed.AppImage";
        std::filesystem::create_hard_link(argv[1],installed);
        appimage::update::Updater updater(installed.string(),false);
        updater.setUpdateInformation(argv[2]);
        bool available=false;
        if (!updater.checkForChanges(available)) throw std::runtime_error("Could not check updates. Check the connection and channel.");
        std::string next;
        if (!updater.pathToNewFile(next)) throw std::runtime_error("Update metadata did not name a package.");
        auto filename=std::filesystem::path(next).filename();
        if (filename.empty() || filename=="installed.AppImage") throw std::runtime_error("Invalid update filename.");
        // The SDK checks its destination filename, which may differ from the
        // user's renamed AppImage. Compare the installed bytes under that name.
        std::filesystem::create_symlink(installed,filename);
        const bool checked=updater.checkForChanges(available);
        std::filesystem::remove(filename);
        if (!checked) throw std::runtime_error("Could not compare installed version.");
        if (!available) { report("IDLE"); return 0; }
        report("AVAILABLE "+std::filesystem::path(next).filename().string());
        std::stringstream feed(argv[2]);std::string part;std::vector<std::string> parts;
        while (std::getline(feed,part,'|')) parts.push_back(part);
        if (parts.size()==5 && parts[0]=="gh-releases-zsync" &&
            std::regex_match(parts[1],std::regex("[A-Za-z0-9_.-]+")) &&
            std::regex_match(parts[2],std::regex("[A-Za-z0-9_.-]+")) &&
            std::regex_match(parts[3],std::regex("[A-Za-z0-9_.-]+")))
            report("NOTES https://github.com/"+parts[1]+"/"+parts[2]+"/releases/tag/"+parts[3]);
        std::string command;
        if (!std::getline(std::cin,command) || command!="DOWNLOAD") return 0;
        if (!updater.start()) throw std::runtime_error("Could not start download.");
        while (!updater.isDone()) {
            double progress=0;updater.progress(progress);
            report("PROGRESS "+std::to_string(progress));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (updater.hasError()) throw std::runtime_error("Download failed; current version kept.");
        report("VERIFYING");
        if (updater.validateSignature()!=appimage::update::Updater::VALIDATION_PASSED)
            throw std::runtime_error("Update signature rejected; current version kept.");
        report("READY");
        if (!std::getline(std::cin,command) || command!="INSTALL") return 0;
        if (!updater.pathToNewFile(next) || updater.validateSignature()!=appimage::update::Updater::VALIDATION_PASSED)
            throw std::runtime_error("Update changed after verification; installation rejected.");
        updater.copyPermissionsToNewFile();
        report("INSTALL "+next);
        return 0;
    } catch (const std::exception& e) { report("ERROR "+std::string(e.what())); return 1; }
}
