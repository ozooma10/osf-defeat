#include "Test.h"
using namespace Test;

TEST(registry_duplicate_preserves_original_and_shared_handles) {
    auto first = DefeatRegistry::Add(42, 3, 7, 9);
    CHECK(first); CHECK(!DefeatRegistry::Add(42, 4, 8, 10));
    CHECK(DefeatRegistry::Get(42) == first); CHECK(first->registeredDay == 3 && first->aggressor == 7);
    CHECK(first->autoRecoverAtDay == 9); CHECK(DefeatRegistry::Remove(42)); CHECK(!DefeatRegistry::Remove(42));
    CHECK(first->aggressor == 7); CHECK(!DefeatRegistry::Get(42));
}
TEST(registry_snapshot_is_independent_and_clear_releases_membership) {
    DefeatRegistry::Add(9, 1, 0); DefeatRegistry::Add(3, 1, 0);
    auto snapshot = DefeatRegistry::Snapshot(); DefeatRegistry::Clear();
    CHECK(snapshot == std::vector<FormID>({3, 9})); CHECK(DefeatRegistry::Snapshot().empty());
}
TEST(concurrent_registration_has_one_winner) {
    std::atomic<int> winners{}; std::vector<std::thread> workers;
    for (int i = 0; i < 16; ++i) workers.emplace_back([&] { if (DefeatRegistry::Add(77, 1, 0)) ++winners; });
    for (auto& worker : workers) worker.join();
    CHECK(winners == 1); CHECK(DefeatRegistry::Snapshot().size() == 1);
}
TEST(concurrent_lease_reads_never_observe_torn_strings) {
    auto entry = DefeatRegistry::Add(1, 1, 0); const std::string a(250, 'a'), b(250, 'b'); entry->SetLeaseHolderId(a);
    std::atomic<bool> valid{true};
    std::thread writer([&] { for (int i = 0; i < 3000; ++i) entry->SetLeaseHolderId(i % 2 ? a : b); });
    for (int i = 0; i < 3000; ++i) { const auto value = entry->CopyLeaseHolderId(); if (value != a && value != b) valid = false; }
    writer.join(); CHECK(valid);
}
TEST(epoch_invalidates_prior_save_work_and_queue_rejects_early_boot) {
    const auto epoch = RuntimeMutation::CurrentEpoch(); CHECK(RuntimeMutation::IsCurrent(epoch));
    CHECK(RuntimeMutation::AdvanceEpoch() == epoch + 1); CHECK(!RuntimeMutation::IsCurrent(epoch));
    TestEngine::queueEnabled = 0; CHECK(!RuntimeMutation::GetTaskQueue());
    TestEngine::queueEnabled = 2; CHECK(!RuntimeMutation::GetTaskQueue());
    TestEngine::queueEnabled = 1; CHECK(RuntimeMutation::GetTaskQueue());
    std::scoped_lock outer(RuntimeMutation::Mutex()); std::scoped_lock nested(RuntimeMutation::Mutex());
}
