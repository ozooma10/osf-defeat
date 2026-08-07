#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat
{
	std::shared_ptr<ActiveDefeat> DefeatRegistry::Add(
		FormID a_id, float a_registeredDay, FormID a_aggressor, float a_autoRecoverAtDay)
	{
		std::unique_lock lock{ _m };
		auto              data = std::make_shared<ActiveDefeat>(a_registeredDay, a_aggressor, a_autoRecoverAtDay);
		const auto [it, inserted] = _activeDefeats.emplace(a_id, data);
		return inserted ? data : nullptr;
	}

	std::shared_ptr<ActiveDefeat> DefeatRegistry::Get(FormID a_id)
	{
		std::shared_lock lock{ _m };
		const auto       it = _activeDefeats.find(a_id);
		return it == _activeDefeats.end() ? nullptr : it->second;
	}

	bool DefeatRegistry::Contains(FormID a_id)
	{
		std::shared_lock lock{ _m };
		return _activeDefeats.contains(a_id);
	}

	bool DefeatRegistry::Remove(FormID a_id)
	{
		std::unique_lock lock{ _m };
		return _activeDefeats.erase(a_id) > 0;
	}

	std::vector<FormID> DefeatRegistry::Snapshot()
	{
		std::shared_lock    lock{ _m };
		std::vector<FormID> out;
		out.reserve(_activeDefeats.size());
		for (const auto& [id, _] : _activeDefeats) {
			out.push_back(id);
		}
		return out;
	}

	void DefeatRegistry::Clear()
	{
		std::unique_lock lock{ _m };
		_activeDefeats.clear();
	}
}
