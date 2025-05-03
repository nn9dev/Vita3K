//
// Created by good afternoon on 12/11/24.
//

#include <host/dialog/filesystem.h>
#include <string>
#include <mutex>
#include <map>

static std::atomic<bool> file_dialog_running = false;

// the result from the dialog ///todo: check what the encoding of the returned string is
static fs::path dialog_result_path = "";
// the resulting file descriptor from the dialog
static int dialog_result_fd = -1;

static std::map<fs::path, int> path_mapping;

// define obj-c functions
extern "C" {
    const char* showFilePickerIOS(const char* default_path, const char* file_extensions);
    const char* showFolderPickerIOS(const char* default_path);
    const char* resolveHandleIOS(const char* file_path);
    const char* getLastIOSError();
}

std::string format_file_filter_extension_list(const std::vector<host::dialog::filesystem::FileFilter> &file_filters) {
    std::string formatted_string = "";
    for (const auto &filter : file_filters) {
        for (const auto &ext : filter.file_extensions) {
            if (!formatted_string.empty()) {
                formatted_string += ",";
            }
            formatted_string += ext;
        }
    }
    return formatted_string;
}


namespace host {
namespace dialog {
namespace filesystem {

// Mutex to ensure thread-safe error handling
std::mutex error_mutex;
std::string last_error;

Result open_file(fs::path &resulting_path, const std::vector<FileFilter>& file_filters, const fs::path& default_path) {
    file_dialog_running = true;
    std::string filters = format_file_filter_extension_list(file_filters);
    const char* picked_path = showFilePickerIOS(default_path.string().c_str(), filters.c_str());
    file_dialog_running = false;

    if (!picked_path) {
        std::lock_guard<std::mutex> lock(error_mutex);
        last_error = getLastIOSError();
        return Result::CANCEL;
    }
    resulting_path = fs::path(picked_path);
    return Result::SUCCESS;
}

Result pick_folder(fs::path &resulting_path, const fs::path& default_path) {
    file_dialog_running = true;
    const char* picked_path = showFolderPickerIOS(default_path.string().c_str());
    file_dialog_running = false;

    if (!picked_path) {
        std::lock_guard<std::mutex> lock(error_mutex);
        last_error = getLastIOSError();
        return Result::CANCEL;
    }
    resulting_path = fs::path(picked_path);
    return Result::SUCCESS;
}


FILE *resolve_host_handle(const fs::path &path) {
    auto it = path_mapping.find(path);
    
    if (it != path_mapping.end()) {
        int fd = it->second;
        return fdopen(fd, "rb");
    } else {
        return fopen(path.c_str(), "rb");
    }
}

std::string get_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error;
}

} // namespace filesystem
} // namespace dialog
} // namespace host
