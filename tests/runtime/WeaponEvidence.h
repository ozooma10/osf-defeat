#pragma once
#include "REX/W32/KERNEL32.h"

// Read-only acceptance instrumentation. Layout/gate provenance:
// OSF RE gameplay.actor_equipment, "Equipped OMOD stack layout", .244;
// OSF Multiplayer EquipmentReplication.cpp. Never mutate instance storage.
namespace OSFDefeat::RuntimeTests::Acceptance {
using WeaponMods = std::vector<std::array<std::uint32_t, 4>>;
inline std::optional<WeaponMods> ReadWeaponMods(RE::Actor& owner, RE::TESForm& weapon) {
    const auto inventory = owner.inventoryList.LockRead();
    if (!*inventory) return std::nullopt;
    WeaponMods result;
    std::uint32_t copies = 0;
    for (const auto& item : (*inventory)->data) {
        if (item.object != &weapon) continue;
        for (const auto& stack : item.stacks) {
            if (!stack.count) continue;
            if (stack.count != 1 || ++copies != 1 || !stack.extra) return std::nullopt;
            auto* bytes = reinterpret_cast<std::byte*>(stack.extra.get());
            const RE::BSAutoReadLock lock{*reinterpret_cast<RE::BSReadWriteLock*>(bytes + 0x20)};
            const auto* list = reinterpret_cast<const RE::BaseExtraList*>(bytes + 8);
            const auto* instance = list->GetByType(RE::ExtraDataType::kObjectInstance);
            if (!instance) return std::nullopt;
            const auto image = reinterpret_cast<std::uintptr_t>(REX::W32::GetModuleHandleW(nullptr));
            if (REL::ID(395585).address() != image + 0x4b0a998 ||
                *reinterpret_cast<const std::uintptr_t*>(instance) != image + 0x4b0a998) return std::nullopt;
            const auto* data = reinterpret_cast<const std::byte*>(instance);
            const auto count = *reinterpret_cast<const std::uint32_t*>(data + 0x18);
            const auto capacity = *reinterpret_cast<const std::uint32_t*>(data + 0x1c);
            const auto* entries = *reinterpret_cast<const std::byte* const*>(data + 0x20);
            if (count > 128 || count > capacity || (count && !entries)) return std::nullopt;
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto* entry = entries + std::size_t(i) * 0x10;
                const auto* mod = *reinterpret_cast<RE::TESForm* const*>(entry);
                if (!mod || mod->GetFormType() != RE::FormType::kOMOD) return std::nullopt;
                result.push_back({mod->GetFormID(), std::to_integer<std::uint32_t>(entry[8]),
                    std::to_integer<std::uint32_t>(entry[9]), std::to_integer<std::uint32_t>(entry[10])});
            }
        }
    }
    if (copies != 1) return std::nullopt;
    std::ranges::sort(result); // Preserve duplicates and attachment metadata.
    return result;
}
}
