#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <vector>

#ifdef HOT_RELOAD
    #include "hot-reload/module_loader.h"
#endif

using Group = uint16_t;
using Instance = uint16_t;

static constexpr inline Group group_none = -1;
static constexpr inline Instance instance_none = -1;

struct Stage
{
    Group group = group_none;
    Instance instance = instance_none;
    friend std::ostream& operator<<(std::ostream& output, const Stage& stage) {
        return output << "{" << stage.group << ", " << stage.instance << "}";
    }
    friend bool operator<(const Stage& a, const Stage& b)
    {
        return a.group < b.group || a.instance < b.instance;
    }
};

using GroupSet = std::set<Stage>;

/*
 * Pointer to a function in discrete time that acts on prop tuples
 * Jolts are SunLambdas registered with a partial lifetime function called when adding a prop that creates a novel tuple
 */
struct SunLambda // λ
{
    using Id = std::size_t;
    using Caller = void (*)(const SunLambda&);
    using JoltCaller = void(*)(const SunLambda&, std::vector<size_t>);
    using Functor = void*;

    Id id;
    Caller caller = nullptr;
    JoltCaller jolt; // PropIdRaw
    Functor functor = nullptr;
    const char* name = nullptr;

    void operator() () const
    {
        Act();
    }

    void Act() const
    {
        (*caller)(*this);
    }
    
    void Emerge(std::vector<size_t> sunData) const
    {
//         std::cout << "Emerging with " << sunData.size() << " sun data" << std::endl;
        (*jolt)(*this, sunData);
    }

    void Breakup(std::vector<size_t> sunData) const
    {
        (*jolt)(*this, sunData);
    }
};

class SunLambdaRegistry
{
public:
    std::vector<std::function<void(const SunLambda&)>> OnRegisterSunLambda;
    
    void Register(const SunLambda& lambda)
    {
        sunLambdas[lambda.id] = lambda;
        for (auto& Callback : OnRegisterSunLambda)
        {
            Callback(lambda);
        }
    }

    template<typename T>
    void Register()
    {
        Register(T{});
    }

    SunLambda& Get(SunLambda::Id id)
    {
        return sunLambdas[id];
    }

    static SunLambdaRegistry& GetInstance()
    {
        static SunLambdaRegistry registry;
        return registry;
    }

#ifdef HOT_RELOAD
    void Unload()
    {
        if(module.IsValid())
        {
            module.Unload();
        }
    }

    void Reload()
    {
        Unload();
        std::cout << HOT_RELOAD_LIB << std::endl;
        module = Module::Load(HOT_RELOAD_LIB);

        for(auto& [id, lambda] : sunLambdas)
        {
            lambda.functor = module.GetFunction(lambda.name + std::string("_Act"));
        }
    }
#endif

    SunLambdaRegistry() = default;
    std::unordered_map<SunLambda::Id, SunLambda> sunLambdas;

#ifdef HOT_RELOAD
    Module module;
#endif
};

#ifndef HOT_RELOAD
    #define SUN_EXPORT
#else
    #ifdef _WIN32
        #define SUN_EXPORT extern "C" __declspec( dllexport )
    #else
        #define SUN_EXPORT extern "C"
    #endif
#endif

#define DeclareSunLambda(LAMBDA_NAME, ...)      \
\
SUN_EXPORT void LAMBDA_NAME ## _Act(__VA_ARGS__);      \
\
inline void LAMBDA_NAME ## _Caller(const SunLambda& lambda) \
{\
   mango::IterateProps(reinterpret_cast<void (*)(__VA_ARGS__)>(lambda.functor), lambda.id);\
}      \
\
inline void LAMBDA_NAME ## _TypesetCaller(const SunLambda& lambda, std::vector<size_t> sunData)\
{\
    mango::CallJolt<__VA_ARGS__>(reinterpret_cast<void (*)(__VA_ARGS__)>(lambda.functor), lambda.id, sunData);\
}\
struct LAMBDA_NAME : SunLambda               \
{                                             \
    LAMBDA_NAME()                              \
    {                                           \
        caller = &LAMBDA_NAME ## _Caller;          \
        jolt = &LAMBDA_NAME ## _TypesetCaller;\
        functor = reinterpret_cast<void*>(&LAMBDA_NAME ## _Act);          \
        name = #LAMBDA_NAME;                      \
        SunLambda::id = LAMBDA_NAME::Id();         \
        mango::ConsiderTypeset<__VA_ARGS__>(Id());\
    };                                              \
    static Id Id(){ return std::type_index(typeid(LAMBDA_NAME)).hash_code(); } \
    static const inline int ForceInit = (SunLambdaRegistry::GetInstance().Register<LAMBDA_NAME>(), 0); \
};

enum LOOP_TIMES : uint8_t
{
    FORAGE,
    NETWORK_RECEIVE,
    INPUT,
    AI,
    UPDATE,
    EVENTS,
    NETWORK_SEND,
    ANIMATION,
    FX,
    RENDER,
    DISPLAY,
};