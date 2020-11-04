#pragma once

#ifdef _MSC_VER
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif // _MSC_VER

class Module
{
public:
    static Module Load(const std::string& path)
    {
        Module module;

#ifdef _MSC_VER
        module.handle = LoadLibraryA(path.data());
#else
        module.handle = dlopen(path.data(), RTLD_LAZY);
#endif

        return module;
    }

    void Unload()
    {
#ifdef _MSC_VER
        try()
        {
            if (!FreeLibrary(static_cast<HMODULE>(handle)))
            {
                throw "Could not unload module";
            }
        }
        catch()
        {

        }
#else
        if (dlclose(handle))
        {
            throw "Could not unload module";
        }
#endif

        handle = nullptr;
    }

    template <typename R, typename...Args>
    using FuncType = R (*)(Args...);

    void* GetFunction(const std::string& name)
    {
#ifdef _MSC_VER
        // Since GetProcAddress returns a function pointer we reinterpret_cast to void* to silence -Wmicrosoft-cast.
        void* funcPtr = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.data()));
#else
        void* funcPtr = dlsym(handle, name.data());
#endif

        return funcPtr;
    }

    template <typename R = void, typename...Args>
    FuncType<R, Args...> GetFunction(const std::string& name)
    {
        return reinterpret_cast<FuncType<R, Args...>>(GetFunction(name));
    }

    bool IsValid() const
    {
        return handle;
    }

private:
    void* handle = nullptr;
};