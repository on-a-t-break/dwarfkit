#include <dwarfkit/session/storage.hpp>

#include <fstream>
#include <sstream>

namespace dwarfkit {

FileSessionStorage::FileSessionStorage(std::filesystem::path directory, std::string keyPrefix)
    : directory_(std::move(directory)), keyPrefix_(std::move(keyPrefix)) {}

std::string FileSessionStorage::storageKey(std::string_view key) const {
    // the key becomes a filename. Every caller in the library passes a
    // literal, but an embedder using its own key must not be able to escape
    // the directory with separators or a parent reference.
    std::string safe(key);
    for (char& c : safe) {
        if (c == '/' || c == '\\' || c == ':') {
            c = '_';
        }
    }
    if (safe.find("..") != std::string::npos) {
        safe = "_" + safe;
    }
    return "wharf-" + keyPrefix_ + "-" + safe;
}

Result<void> FileSessionStorage::write(std::string_view key, std::string_view data) {
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    const std::filesystem::path path = directory_ / storageKey(key);
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.good()) {
            return err(ErrorKind::Storage, "Unable to open storage file for " + std::string(key));
        }
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!file.good()) {
            return err(ErrorKind::Storage,
                       "Unable to write storage file for " + std::string(key));
        }
    }
#ifndef _WIN32
    // a session written by WalletPluginPrivateKey holds the private key in
    // cleartext, so it must not land world-readable
    std::filesystem::permissions(path,
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
#endif
    return {};
}

Result<std::optional<std::string>> FileSessionStorage::read(std::string_view key) {
    std::ifstream file(directory_ / storageKey(key), std::ios::binary);
    if (!file.good()) {
        return std::optional<std::string>{};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return std::optional(buffer.str());
}

Result<void> FileSessionStorage::remove(std::string_view key) {
    std::error_code ec;
    std::filesystem::remove(directory_ / storageKey(key), ec);
    return {};
}

}  // namespace dwarfkit
