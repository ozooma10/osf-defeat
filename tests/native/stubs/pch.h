#pragma once
// Engine boundary only. No defeat/recovery/selection/persistence logic lives here.
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace TestEngine {
    inline std::vector<std::string> logs;
    inline std::uint8_t queueEnabled = 1;
    inline std::filesystem::path executable;
    inline void* persistenceExport = nullptr;
    inline bool moduleAvailable = true;
}
namespace REX {
    template<class... Args> void INFO(std::format_string<Args...> fmt, Args&&... args) {
        TestEngine::logs.push_back(std::format(fmt, std::forward<Args>(args)...));
    }
    template<class... Args> void WARN(std::format_string<Args...> fmt, Args&&... args) { INFO(fmt, std::forward<Args>(args)...); }
    template<class... Args> void ERROR(std::format_string<Args...> fmt, Args&&... args) { INFO(fmt, std::forward<Args>(args)...); }
}
namespace RE {
    using BSFixedString = std::string;
    template<class T> using BSTSmartPointer = std::shared_ptr<T>;
    template<class T> using BSScrapArray = std::vector<T>;
    template<class T> using BSTThreadScrapFunction = std::function<T>;
    enum class ACTOR_LIFE_STATE { kAlive, kDying, kDead, kUnconscious, kReanimate, kRecycle, kRestrained, kEssentialDown, kBleedOut };
    struct TESForm {
        virtual ~TESForm() = default;
        std::uint32_t id{};
        std::uint32_t GetFormID() const { return id; }
        template<class T> T* As() { return dynamic_cast<T*>(this); }
        static inline std::map<std::uint32_t, TESForm*> forms;
        static inline std::map<std::string, TESForm*> editors;
        template<class T> static T* LookupByID(std::uint32_t id) {
            auto it = forms.find(id); return it == forms.end() ? nullptr : dynamic_cast<T*>(it->second);
        }
        template<class T> static T* LookupByEditorID(const BSFixedString& name) {
            auto it = editors.find(name); return it == editors.end() ? nullptr : dynamic_cast<T*>(it->second);
        }
    };
    struct ActorValueInfo {};
    enum class ACTOR_VALUE_MODIFIER { kDamage, kTemporary, kPermanent };
    struct ActorValueOwner {
        float health=100, maximum=100;
        float GetActorValue(const ActorValueInfo&) const { return health; }
        float GetPermanentActorValue(const ActorValueInfo&) const { return maximum; }
    };
    struct TESObjectREFR : TESForm, ActorValueOwner {};
    struct BGSKeyword : TESForm {};
    struct TESFaction : TESForm {};
    struct TESObjectWEAP : TESForm {};
    struct Actor : TESObjectREFR {
        explicit Actor(std::uint32_t value = 0) { id = value; }
        bool dead = false, inScene = false;
        enum class BOOL_BITS { kProcessMe };
        struct Bits { bool enabled = true; bool all(BOOL_BITS) const { return enabled; } } boolBits;
        struct Loaded { void* data3D = reinterpret_cast<void*>(1); } loaded;
        struct LoadedData { Loaded* value; Loaded* const* LockRead() const { return &value; } } loadedData{&loaded};
        TESForm* parentCell = nullptr;
        Actor* combatTarget = nullptr;
        struct Position {
            float x{}, y{}, z{};
            Position operator-(Position other) const { return {x-other.x,y-other.y,z-other.z}; }
            float SqrLength() const { return x*x+y*y+z*z; }
        } position;
        Position GetPosition() const { return position; }
        std::set<BGSKeyword*> keywords;
        std::map<TESFaction*, int> factions;
        std::set<Actor*> hostile;
        bool IsDead() const { return dead; }
        bool IsPlayerRef() const { return id == 0x14; }
        void* GetCurrentScene() const { return inScene ? reinterpret_cast<void*>(1) : nullptr; }
        bool HasKeyword(BGSKeyword* keyword) const { return keywords.contains(keyword); }
        int GetFactionRank(TESFaction* faction, bool) const { auto it = factions.find(faction); return it == factions.end() ? -1 : it->second; }
        bool IsHostileToActor(Actor* target) const { return hostile.contains(target); }
        void SetAIEnabled(bool enabled) { boolBits.enabled = enabled; }
    };
    struct PlayerCharacter : Actor {
        PlayerCharacter() : Actor(0x14) {}
        static inline PlayerCharacter* instance = nullptr;
        static PlayerCharacter* GetSingleton() { return instance; }
    };
    struct Calendar {
        float day = 1.0f, scale = 20.0f;
        static inline Calendar* instance = nullptr;
        static Calendar* GetSingleton() { return instance; }
        float GetDaysPassedExact() const { return day; }
        float GetTimeScale() const { return scale; }
    };
    namespace BSService {
        struct TaskQueue {
            static TaskQueue* GetSingleton() { static TaskQueue queue; return &queue; }
            bool inlineTasks = false;
            std::vector<std::function<void()>> pending;
            template<class F> void AddTask(F f) { if (inlineTasks) f(); else pending.emplace_back(std::move(f)); }
            void Drain() { auto batch = std::move(pending); pending.clear(); for (auto& f : batch) f(); }
        };
    }
    namespace BSScript {
        class Object {};
        class IStackCallbackFunctor {};
        struct Variable { std::any value; };
        template<class T> void PackVariable(Variable& out, T value) { out.value = value; }
        template<class T> int GetVMTypeID() { return 1; }
        using Args = std::function<bool(BSScrapArray<Variable>&)>;
        struct IVirtualMachine {
            struct Handles {
                bool available = true;
                std::uint64_t GetHandleForObject(int, const void*) const { return available ? 1 : 0; }
                std::uint64_t EmptyHandle() const { return 0; }
            } handles;
            struct Call { std::string name; Args args; };
            bool accept = true;
            std::vector<Call> calls;
            Handles& GetObjectHandlePolicy() { return handles; }
            template<class C> bool DispatchMethodCall(const BSTSmartPointer<Object>&, const BSFixedString& name, Args args, const C&, int) {
                if (accept) calls.push_back({name, std::move(args)}); return accept;
            }
            template<class C> bool DispatchMethodCall(std::uint64_t, const char* type, const char* name, Args args, const C&, int) {
                if (accept) calls.push_back({std::string(type) + "." + name, std::move(args)}); return accept;
            }
        };
    }
    struct GameVM {
        BSScript::IVirtualMachine* vm = nullptr;
        static inline GameVM* instance = nullptr;
        static GameVM* GetSingleton() { return instance; }
        BSScript::IVirtualMachine* GetVM() const { return vm; }
    };
}
