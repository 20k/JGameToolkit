#include "fs_helpers.hpp"

#include "clock.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <atomic>
#include <stdlib.h>
#include <optional>

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif // __WIN32__

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/fetch.h>
#endif // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__
EM_JS(void, syncer, (),
{
    FS.syncfs(false, function (err) {

    });
});
#endif // __EMSCRIPTEN__

namespace
{
    thread_local int syncs{0};
    #ifdef __EMSCRIPTEN__
    thread_local bool syncs_dirty = false;
    #endif
}

void sync_writes()
{
    #ifdef __EMSCRIPTEN__
    syncs_dirty = true;

    if(syncs == 0 && syncs_dirty)
    {
        syncer();
        syncs_dirty = false;
    }
    #endif // __EMSCRIPTEN__
}

file::manual_fs_sync::manual_fs_sync()
{
    syncs++;
}

file::manual_fs_sync::~manual_fs_sync()
{
    syncs--;

    sync_writes();
}

std::string read_impl(const std::string& file, file::mode::type m)
{
    const char* fmode = (m == file::mode::BINARY) ? "rb" : "r";

    FILE* f = fopen(file.c_str(), fmode);

    if(f == nullptr)
        return "";

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(fsize == -1L)
    {
        fclose(f);
        return "";
    }

    std::string buffer;
    buffer.resize(fsize);
    size_t real = fread(buffer.data(), 1, fsize, f);
    buffer.resize(real);

    fclose(f);

    return buffer;
}

std::string file::read(const std::string& file, file::mode::type m)
{
    #ifndef __EMSCRIPTEN__
    return read_impl(file, m);
    #else
    return read_impl("web/" + file, m);
    #endif
}

std::optional<std::string> file::request::read(const std::string& file, file::mode::type m)
{
    #ifndef __EMSCRIPTEN__
    if(!file::exists(file))
        return std::nullopt;

    return file::read(file, m);
    #else
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    emscripten_fetch_t *fetch = emscripten_fetch(&attr, file.c_str()); // Blocks here until the operation is complete.

    std::optional<std::string> result;

    if(fetch->status == 200)
    {
        printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);

        result.emplace(fetch->data, fetch->data + fetch->numBytes);
    }
    else
    {
        //printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
    }

    emscripten_fetch_close(fetch);

    if(m == file::mode::TEXT && result.has_value())
    {
        std::string::size_type pos = 0;
        while ((pos = result.value().find("\r\n", pos)) != std::string::npos)
        {
            result.value().replace(pos, 2, "\n");
        }
    }

    return result;
    #endif
}

void file::write(const std::string& file, const std::string& data, file::mode::type m)
{
    {
        if(m == file::mode::BINARY)
        {
            #ifndef __EMSCRIPTEN__
            std::ofstream out(file, std::ios::binary);
            #else
            std::ofstream out("web/" + file, std::ios::binary);
            #endif
            out << data;
        }
        else if(m == file::mode::TEXT)
        {
            #ifndef __EMSCRIPTEN__
            std::ofstream out(file);
            #else
            std::ofstream out("web/" + file);
            #endif
            out << data;
        }
    }

    sync_writes();
}

void file::write_atomic(const std::string& in_file, const std::string& data, file::mode::type m)
{
    if(data.size() == 0)
        return;

    #ifndef __EMSCRIPTEN__
    std::string file = in_file;
    #else
    std::string file = "web/" + in_file;
    #endif

    std::string atomic_extension = ".atom";
    std::string atomic_file = file + atomic_extension;
    std::string backup_file = file + ".back";

    /*#ifdef __WIN32__
    HANDLE handle = CreateFile(atomic_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    WriteFile(handle, &data[0], data.size(), nullptr, nullptr);
    //FlushFileBuffers(handle);
    CloseHandle(handle);
    #else
    int fd = open(atomic_file.c_str(), O_CREAT | O_DIRECT | O_SYNC | O_TRUNC | O_WRONLY, 0777);

    int written = 0;

    while(written < data.size())
    {
        int rval = ::write(fd, &data[written], (int)data.size() - written);

        if(rval == -1)
        {
            close(fd);
            throw std::runtime_error("Errno in atomic write " + std::to_string(errno));
        }

        written += rval;
    }

    close(fd);

    return;

    #endif // __WIN32__*/

    if(m == file::mode::TEXT)
    {
        std::ofstream out(atomic_file);

        out << data;
    }
    else
    {
        std::ofstream out(atomic_file, std::ios::binary);

        out << data;
    }

    sync_writes();

    if(!file::exists(file))
    {
        ::rename(atomic_file.c_str(), file.c_str());
        sync_writes();
        return;
    }

    if(file::exists(backup_file))
    {
        ::remove(backup_file.c_str());
    }

    /*steady_timer timer;

    bool write_success = false;
    bool any_errors = false;

    do
    {
        #ifdef __WIN32__
        //bool err = ReplaceFileA(file.c_str(), atomic_file.c_str(), backup_file.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) == 0;
        bool err = ReplaceFileA(file.c_str(), atomic_file.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) == 0;
        #else
        bool err = ::rename(atomic_file.c_str(), file.c_str()) != 0;
        #endif // __WIN32__

        //bool err = ReplaceFileA(file.c_str(), atomic_file.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) == 0;

        if(!err)
        {
            write_success = true;
            break;
        }

        if(err)
        {
            #ifdef __WIN32__
            printf("atomic write error %lu ", GetLastError());
            #else
            printf("atomic write error %i %s\n", errno, file.c_str());
            #endif // __WIN32__

            any_errors = true;
        }

        sync_writes();
    }
    while(timer.get_elapsed_time_s() < 1);*/

    ::rename(file.c_str(), backup_file.c_str());
    ::rename(atomic_file.c_str(), file.c_str());

    /*if(!write_success)
    {
        throw std::runtime_error("Explod in atomic write");
    }*/

    /*if(any_errors)
    {
        printf("atomic_write had errors but recovered");
    }*/
}

bool file::exists(const std::string& name)
{
    #ifndef __EMSCRIPTEN__
    std::ifstream f(name.c_str());
    #else
    std::ifstream f(("web/" + name).c_str());
    #endif
    return f.good();
}

void file::rename(const std::string& from, const std::string& to)
{
    #ifndef __EMSCRIPTEN__
    ::rename(from.c_str(), to.c_str());
    #else
    ::rename(("web/" + from).c_str(), ("web/" + to).c_str());
    #endif

    sync_writes(); //?
}

bool file::remove(const std::string& name)
{
    #ifndef __EMSCRIPTEN__
    return ::remove(name.c_str()) == 0;
    #else
    return ::remove(("web/" + name).c_str()) == 0;
    #endif
}

void file::mkdir(const std::string& name)
{
    #ifndef __EMSCRIPTEN__
    #ifdef __WIN32__
    ::_mkdir(name.c_str());
    #else
    ::mkdir(name.c_str(), 0777);
    #endif // __WIN32__
    #else
    ::mkdir(("web/" + name).c_str(), 0777);
    #endif
}

#ifdef __EMSCRIPTEN__
EM_JS(void, handle_download, (const char* fullname),
{
    var memoryFSname = UTF8ToString(fullname);

    console.log(memoryFSname);

    var content = FS.readFile(memoryFSname);
    var mime = "application/octet-stream";

    var a = document.createElement('a');
    a.download = memoryFSname;
    a.href = URL.createObjectURL(new Blob([content], {type: mime}));
    a.style.display = 'none';

    document.body.appendChild(a);
    a.click();
    setTimeout(() => {
    document.body.removeChild(a);
    URL.revokeObjectURL(a.href);
    }, 2000);
});

void file::download(const std::string& name, const std::string& data)
{
    {
        std::ofstream out("download/" + name, std::ios::binary);
        out << data;
    }

    std::string full_path = "download/" + name;

    handle_download(full_path.c_str());
}
#endif // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__
EM_JS(void, handle_mounting, (),
{
    FS.mkdir('/web');
    FS.mkdir('/download');
    FS.mount(IDBFS, {}, "/web");
    FS.mount(MEMFS, {}, "/download");

    Module.syncdone = 0;

    FS.syncfs(true, function (err) {
        Module.syncdone = 1;
    });
});

struct em_helper
{
    em_helper()
    {
        handle_mounting();

        printf("Mounted\n");

        while(emscripten_run_script_int("Module.syncdone") == 0)
        {
            emscripten_sleep(100);
        }

        printf("Finished mounting\n");
    }
};

#endif

void file::init()
{
    #ifdef __EMSCRIPTEN__
    static em_helper help;
    #endif // __EMSCRIPTEN__
}
