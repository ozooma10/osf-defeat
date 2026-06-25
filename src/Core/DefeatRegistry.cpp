#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat
{
	std::shared_ptr<VictimData> DefeatRegistry::Add(FormID a_id, float a_registeredDay, FormID a_aggressor)
	{
		std::unique_lock lock{ _m };
		auto              data = std::make_shared<VictimData>(a_registeredDay, a_aggressor);
		const auto [it, inserted] = _victims.emplace(a_id, data);
		return inserted ? data : nullptr;
	}

	std::shared_ptr<VictimData> DefeatRegistry::Get(FormID a_id)
	{
		std::shared_lock lock{ _m };
		const auto       it = _victims.find(a_id);
		return it == _victims.end() ? nullptr : it->second;
	}

	bool DefeatRegistry::Contains(FormID a_id)
	{
		std::shared_lock lock{ _m };
		return _victims.contains(a_id);
	}

	bool DefeatRegistry::Remove(FormID a_id)
	{
		std::unique_lock lock{ _m };
		return _victims.erase(a_id) > 0;
	}

	std::vector<FormID> DefeatRegistry::Snapshot()
	{
		std::shared_lock    lock{ _m };
		std::vector<FormID> out;
		out.reserve(_victims.size());
		for (const auto& [id, _] : _victims) {
			out.push_back(id);
		}
		return out;
	}

	void DefeatRegistry::Clear()
	{
		std::unique_lock lock{ _m };
		_victims.clear();
	}
}
