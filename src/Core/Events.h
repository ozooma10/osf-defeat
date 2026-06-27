#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace RE::BSScript
{
    class Object;
}

namespace OSFDefeat::Events
{
    using Token = std::int32_t;
    using Receiver = RE::BSTSmartPointer<RE::BSScript::Object>;

    // Callback registration. A consumer passes its own script instance (ScriptObject) plus the name
    // of a regular Function to invoke, and gets back a token to Unregister with. Starfield cannot
    // deliver Papyrus *events* to non-native scripts, so we DispatchMethodCall a function instead
    // (the proven OSF Animation pattern). A token of 0 means registration failed.
    Token RegisterPlayerDefeated(const Receiver& a_receiver, std::string_view a_callback);
    bool  UnregisterPlayerDefeated(Token a_token);
    Token RegisterPlayerRecovered(const Receiver& a_receiver, std::string_view a_callback);
    bool  UnregisterPlayerRecovered(Token a_token);
    Token RegisterPlayerConsequenceStarted(const Receiver& a_receiver, std::string_view a_callback);
    bool  UnregisterPlayerConsequenceStarted(Token a_token);

    Token RegisterActorDefeated(const Receiver& a_receiver, std::string_view a_callback);
    bool  UnregisterActorDefeated(Token a_token);
    Token RegisterActorRecovered(const Receiver& a_receiver, std::string_view a_callback);
    bool  UnregisterActorRecovered(Token a_token);

    void BroadcastPlayerDefeated();
    void BroadcastPlayerRecovered();
    void BroadcastPlayerConsequenceStarted();
    void BroadcastActorDefeated(FormID a_victim, FormID a_aggressor);
    void BroadcastActorRecovered(FormID a_victim, FormID a_aggressor);

    // Wires the dormant Defeat observers to the actor broadcasts. Call once at init.
    void Install();
    void Clear();
}
