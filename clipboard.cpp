#include "clipboard.hpp"
#include <imgui/imgui.h>

#include <stdexcept>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__
EM_JS(void, init_copy, (),
{
    var clipboardBuffer = document.createElement('textarea');
    clipboardBuffer.style.cssText = 'position:fixed; top:-10px; left:-10px; height:0; width:0; opacity:0;';
    document.body.appendChild(clipboardBuffer);

    Module.clipbuffer = clipboardBuffer;
});

EM_JS(void, copy_js, (const char* data),
{
    var set_name = UTF8ToString(data);

    Module.clipbuffer.focus();
    Module.clipbuffer.value = set_name;
    Module.clipbuffer.setSelectionRange(0, Module.clipbuffer.value.length);
    var succeeded = document.execCommand('copy');
});

EM_JS(void, update_clipboard_data, (),
{
    if(!document.hasFocus())
        return;

    //Module.osclipdata = ClipboardEvent.clipboardData.getData('Text');
    //Module.osclipdata = window.clipboardData.getData('Text');

    if(!Module.osclipdata)
        Module.osclipdata = "";

    navigator.clipboard.readText().then(function(clipText)
    {
        Module.osclipdata = clipText
    });
});

EM_JS(int, get_osclipdata_length, (),
{
    return Module.osclipdata.length;
});

EM_JS(void, get_osclipdata, (char* out, int len),
{
    stringToUTF8(Module.osclipdata, out, len);
});

#endif // __EMSCRIPTEN__

void clipboard::set(const std::string& data)
{
    #ifndef __EMSCRIPTEN__
    ImGui::SetClipboardText(data.c_str());
    #else
    init_copy();
    copy_js(data.c_str());
    #endif
}

std::string clipboard::get()
{
    #ifndef __EMSCRIPTEN__
    const char* clip = ImGui::GetClipboardText();

    if(clip == nullptr)
        return "";

    return std::string(clip);
    #else
    int clip_len = get_osclipdata_length();
    std::string clip_buf;
    clip_buf.resize(clip_len + 1);
    get_osclipdata(&clip_buf[0], clip_len + 1);

    int cstrlen = strlen(clip_buf.c_str());

    clip_buf = std::string(clip_buf.begin(), clip_buf.begin() + cstrlen);

    return clip_buf;
    #endif
}

void clipboard::poll()
{
    #ifdef __EMSCRIPTEN__
    update_clipboard_data();
    #endif // __EMSCRIPTEN__
}
