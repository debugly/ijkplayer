#ifndef IJKSDL_UTILS_WIN_H
#define IJKSDL_UTILS_WIN_H

void *SDL_LoadObject(const char *sofile);
void *SDL_LoadFunction(void *handle, const char *name);

#define LOAD_FUNC(NAME) \
egl->egl_data->NAME = SDL_LoadFunction(egl->egl_data->dll_handle, #NAME); \
if (!egl->egl_data->NAME) \
{ \
    return ALOGE("Could not retrieve EGL function " #NAME); \
}

#endif