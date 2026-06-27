#include "pch.h"

#include "Core/Events.h"

#include "Core/Defeat.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

namespace OSFDefeat::Events
{
    namespace
    {
        using ArgsBuilder = RE::BSTThreadScrapFunction<bool(RE::BSScrapArray<RE::BSScript::Variable>&)>;

        [[nodiscard]] RE::BSScript::IVirtualMachine* VM()
        {
            const auto* const gameVM = RE::GameVM::GetSingleton();
            return gameVM ? gameVM->GetVM() : nullptr;
        }

        // Token-based registry of Papyrus callbacks for one event channel. Consumers register a
        // script instance + a function name; on broadcast we DispatchMethodCall that function on
        // each. (Starfield can't deliver events to non-native scripts — see Events.h.)
        class CallbackList
        {
        public:
            Token Add(const Receiver& a_receiver, std::string_view a_fn)
            {
                if (!a_receiver.get() || a_fn.empty()) {
                    return 0;
                }
                std::unique_lock lock{ _lock };
                for (std::uint16_t slot = 0; slot < _slots.size(); ++slot) {
                    if (_slots[slot].generation == 0) {
                        _slots[slot] = Entry{ NextGen(), a_receiver, RE::BSFixedString{ a_fn } };
                        return MakeToken(_slots[slot].generation, slot);
                    }
                }
                if (_slots.size() >= 0xFFFF) {
                    return 0;
                }
                const auto slot = static_cast<std::uint16_t>(_slots.size());
                _slots.push_back(Entry{ NextGen(), a_receiver, RE::BSFixedString{ a_fn } });
                return MakeToken(_slots[slot].generation, slot);
            }

            bool Remove(Token a_token)
            {
                if (a_token == 0) {
                    return false;
                }
                const auto slot = static_cast<std::uint16_t>(a_token & 0xFFFF);
                const auto gen = static_cast<std::uint16_t>((a_token >> 16) & 0xFFFF);
                std::unique_lock lock{ _lock };
                if (slot >= _slots.size() || _slots[slot].generation != gen) {
                    return false;
                }
                _slots[slot] = Entry{};
                return true;
            }

            void Dispatch(const ArgsBuilder& a_args)
            {
                std::vector<Entry> targets;
                {
                    std::unique_lock lock{ _lock };
                    for (const auto& entry : _slots) {
                        if (entry.generation != 0 && entry.receiver.get()) {
                            targets.push_back(entry);
                        }
                    }
                }
                if (targets.empty()) {
                    return;
                }
                auto* const vm = VM();
                if (!vm) {
                    return;
                }
                const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> noCallback{};
                for (auto& target : targets) {
                    vm->DispatchMethodCall(target.receiver, target.fn, a_args, noCallback, 0);
                }
            }

            void Clear()
            {
                std::unique_lock lock{ _lock };
                _slots.clear();
                _nextGen = 1;
            }

        private:
            struct Entry
            {
                std::uint16_t     generation = 0;  // 0 = empty slot
                Receiver          receiver;
                RE::BSFixedString fn;
            };

            std::uint16_t NextGen()
            {
                _nextGen = (_nextGen == 0xFFFF) ? 1 : static_cast<std::uint16_t>(_nextGen + 1);
                return _nextGen;
            }

            static Token MakeToken(std::uint16_t a_gen, std::uint16_t a_slot)
            {
                return (static_cast<Token>(a_gen) << 16) | a_slot;
            }

            std::mutex         _lock;
            std::vector<Entry> _slots;
            std::uint16_t      _nextGen = 1;
        };

        CallbackList g_playerDefeated;
        CallbackList g_playerRecovered;
        CallbackList g_playerConsequenceStarted;
        CallbackList g_actorDefeated;
        CallbackList g_actorRecovered;

        // (Actor akVictim, Actor akAggressor) args, resolved when the call runs so a deferred
        // dispatch can't dangle on an actor that unloaded after the broadcast.
        [[nodiscard]] auto ActorArgs(FormID a_victim, FormID a_aggressor)
        {
            return [a_victim, a_aggressor](RE::BSScrapArray<RE::BSScript::Variable>& a_args) -> bool {
                a_args.resize(2);
                RE::BSScript::PackVariable(a_args[0], a_victim ? RE::TESForm::LookupByID<RE::Actor>(a_victim) : nullptr);
                RE::BSScript::PackVariable(a_args[1], a_aggressor ? RE::TESForm::LookupByID<RE::Actor>(a_aggressor) : nullptr);
                return true;
            };
        }

        bool NoArgs(RE::BSScrapArray<RE::BSScript::Variable>&)
        {
            return true;
        }
    }

    Token RegisterPlayerDefeated(const Receiver& a_receiver, std::string_view a_callback) { return g_playerDefeated.Add(a_receiver, a_callback); }
    bool  UnregisterPlayerDefeated(Token a_token) { return g_playerDefeated.Remove(a_token); }
    Token RegisterPlayerRecovered(const Receiver& a_receiver, std::string_view a_callback) { return g_playerRecovered.Add(a_receiver, a_callback); }
    bool  UnregisterPlayerRecovered(Token a_token) { return g_playerRecovered.Remove(a_token); }
    Token RegisterPlayerConsequenceStarted(const Receiver& a_receiver, std::string_view a_callback) { return g_playerConsequenceStarted.Add(a_receiver, a_callback); }
    bool  UnregisterPlayerConsequenceStarted(Token a_token) { return g_playerConsequenceStarted.Remove(a_token); }

    Token RegisterActorDefeated(const Receiver& a_receiver, std::string_view a_callback) { return g_actorDefeated.Add(a_receiver, a_callback); }
    bool  UnregisterActorDefeated(Token a_token) { return g_actorDefeated.Remove(a_token); }
    Token RegisterActorRecovered(const Receiver& a_receiver, std::string_view a_callback) { return g_actorRecovered.Add(a_receiver, a_callback); }
    bool  UnregisterActorRecovered(Token a_token) { return g_actorRecovered.Remove(a_token); }

    void BroadcastPlayerDefeated() { g_playerDefeated.Dispatch(&NoArgs); }
    void BroadcastPlayerRecovered() { g_playerRecovered.Dispatch(&NoArgs); }
    void BroadcastPlayerConsequenceStarted() { g_playerConsequenceStarted.Dispatch(&NoArgs); }
    void BroadcastActorDefeated(FormID a_victim, FormID a_aggressor) { g_actorDefeated.Dispatch(ActorArgs(a_victim, a_aggressor)); }
    void BroadcastActorRecovered(FormID a_victim, FormID a_aggressor) { g_actorRecovered.Dispatch(ActorArgs(a_victim, a_aggressor)); }

    void Install()
    {
        // Light up the dormant Defeat observers so every defeat/rescue (player AND NPC) fans out to
        // OnActorDefeated/OnActorRecovered callbacks. Fires on the game thread (DefeatActor/RescueActor).
        Defeat::SetOnDefeat([](FormID a_victim, FormID a_aggressor) { BroadcastActorDefeated(a_victim, a_aggressor); });
        Defeat::SetOnRescue([](FormID a_victim, FormID a_aggressor) { BroadcastActorRecovered(a_victim, a_aggressor); });
    }

    void Clear()
    {
        g_playerDefeated.Clear();
        g_playerRecovered.Clear();
        g_playerConsequenceStarted.Clear();
        g_actorDefeated.Clear();
        g_actorRecovered.Clear();
    }
}
