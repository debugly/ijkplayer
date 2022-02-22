#include <windows.h>

void *
SDL_LoadObject(const char *sofile)
{
    LPTSTR tstr = (LPTSTR)sofile;
#ifdef __WINRT__
    /* WinRT only publically supports LoadPackagedLibrary() for loading .dll
       files.  LoadLibrary() is a private API, and not available for apps
       (that can be published to MS' Windows Store.)
    */
    void *handle = (void *) LoadPackagedLibrary(tstr, 0);
#else
    void *handle = (void *) LoadLibrary(tstr);
#endif
    free(tstr);

    /* Generate an error message if all loads failed */
    if (handle == NULL) {
		int err = 0;
		err = GetLastError();
        char errbuf[512];
        strncpy(errbuf, "Failed loading ", sizeof(errbuf));
        strncat(errbuf, sofile, sizeof(errbuf));
	}
    return handle;
}

void *
SDL_LoadFunction(void *handle, const char *name)
{
    void *symbol = (void *) GetProcAddress((HMODULE) handle, name);
    if (symbol == NULL) {
        char errbuf[512];
        strncpy(errbuf, "Failed loading ", sizeof(errbuf));
        strncat(errbuf, name, sizeof(errbuf));
	}
    return symbol;
}

#define LOAD_FUNC(NAME) \
egl->egl_data->NAME = SDL_LoadFunction(egl->egl_data->dll_handle, #NAME); \
if (!egl->egl_data->NAME) \
{ \
    return ALOGE("Could not retrieve EGL function " #NAME); \
}

