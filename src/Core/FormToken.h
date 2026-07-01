#pragma once

#include "pch.h"

namespace OSFDefeat
{
    // Shared "form token" resolution for JSON-authored references: editorID first
    // (runtime-verified to work for quests and keywords), then a 0x-prefixed / bare
    // hex FormID. Returns nullptr when the token names nothing of type T. Forms are
    // resolved at USE time (game thread), not JSON-load time, so load order and
    // form availability are never a parsing concern.
    template <class T>
    [[nodiscard]] T* ResolveFormToken(std::string_view a_token)
    {
        if (a_token.empty()) {
            return nullptr;
        }
        if (auto* const form = RE::TESForm::LookupByEditorID<T>(RE::BSFixedString{ a_token })) {
            return form;
        }

        std::string_view hex = a_token;
        if (hex.starts_with("0x") || hex.starts_with("0X")) {
            hex.remove_prefix(2);
        }
        if (hex.empty() || hex.size() > 8) {
            return nullptr;
        }
        std::uint32_t parsed = 0;
        const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), parsed, 16);
        if (ec != std::errc{} || ptr != hex.data() + hex.size()) {
            return nullptr;
        }
        return RE::TESForm::LookupByID<T>(parsed);
    }
}
