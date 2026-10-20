#include "Meter.h"

namespace Meter
{
	using namespace Defs;

	std::string Def::Describe() const
	{
		std::string names;
		for (const auto& n : stageNames) {
			names += (names.empty() ? "" : ", ") + n;
		}
		for (const auto& b : bonuses) {
			if (!b.name.empty()) {
				names += " (bonus: " + b.name + ")";
			}
		}
		return names;
	}

	Def Load(const json& a_json, const std::string& a_plugin)
	{
		Def d;
		d.enabled = ParseConditions(a_json, "enabled", a_plugin);
		d.value = ParseValue(a_json.at("value"), a_plugin);
		if (a_json.contains("max")) {
			d.max = ParseValue(a_json["max"], a_plugin);
		}
		d.stages = ParseStages(a_json, a_plugin);
		for (const auto& name : a_json.value("stageNames", json::array())) {
			d.stageNames.push_back(StageName(name, a_plugin));
		}
		d.stageNames.resize(d.stages.size() + 1);
		d.comfortableStages = a_json.value("comfortableStages", 1);
		d.dangerStage = a_json.value("dangerStage", std::max(static_cast<int>(d.stages.size()) - 1, 1));
		d.bonusStages = a_json.value("bonusStages", 0);
		if (a_json.contains("bonus")) {
			const auto& b = a_json["bonus"];
			for (const auto& entry : b.is_array() ? b : json::array({ b })) {
				Bonus bonus;
				bonus.when = ParseValue(entry.at("when"), a_plugin);
				bonus.name = entry.contains("name") ? StageName(entry["name"], a_plugin) : std::string{};
				if (bonus.when.Valid()) {
					d.bonuses.push_back(std::move(bonus));
				}
			}
		}
		d.fillsUp = a_json.value("fillsUp", false);
		if (a_json.contains("detail")) {
			const auto& detail = a_json["detail"];
			d.detailValue = ParseValue(detail.at("value"), a_plugin);
			d.detailFormat = detail.value("format", "{:.0f}"s);
		}
		d.hidden.Parse(a_json, a_plugin);
		return d;
	}

	NeedReading Read(const Def& a_def)
	{
		const auto  stages = GetStages(a_def.stages);
		const auto  maxValue = a_def.max.Get();
		const float max = maxValue && *maxValue > 0.0f ? *maxValue : (stages.empty() ? 1000.0f : stages.back() * 1.25f);
		const float value = std::clamp(a_def.value.Get().value_or(0.0f), 0.0f, max);

		// the gauge: one even share per stage, from 0 to max (a last stage that starts at max is only its end)
		std::vector<float> bounds{ 0.0f };
		for (const float stage : stages) {
			if (stage > bounds.back() && stage < max) {
				bounds.push_back(stage);
			}
		}
		bounds.push_back(max);
		const int segments = static_cast<int>(bounds.size()) - 1;
		float     used = static_cast<float>(segments);
		for (int i = 0; i < segments; ++i) {
			if (value < bounds[i + 1]) {
				used = i + (value - bounds[i]) / std::max(bounds[i + 1] - bounds[i], 1e-3f);
				break;
			}
		}

		int stage = 0;
		while (stage < static_cast<int>(stages.size()) && value >= stages[stage]) {
			++stage;
		}
		const Bonus* bonus = nullptr;
		for (const auto& b : a_def.bonuses) {
			if (b.when.Holds()) {
				bonus = &b;
				break;
			}
		}

		NeedReading r;
		r.segments = std::min(segments, kMaxNeedSegments);
		for (int j = 0; j < r.segments; ++j) {
			// the stage the segment starts in
			int k = 0;
			while (k < static_cast<int>(stages.size()) && bounds[j] >= stages[k]) {
				++k;
			}
			r.zones[j] = k < a_def.bonusStages ? NeedZone::kBonus :
			             k < a_def.comfortableStages ? NeedZone::kFine :
			             k >= a_def.dangerStage ? NeedZone::kDanger :
			                                       NeedZone::kPenalty;
		}
		r.value = value;
		r.max = max;
		r.fill = bonus ? 1.0f : std::clamp(1.0f - used / segments, 0.0f, 1.0f);
		r.bonus = bonus || stage < a_def.bonusStages;
		r.comfortable = bonus || stage < a_def.comfortableStages;
		r.danger = !bonus && stage >= a_def.dangerStage;
		r.stage = bonus && !bonus->name.empty() ? bonus->name : a_def.stageNames[stage];
		r.fillsUp = a_def.fillsUp;
		if (const auto detail = a_def.detailValue.Get()) {
			try {
				r.detail = std::vformat(a_def.detailFormat, std::make_format_args(*detail));
			} catch (const std::format_error&) {
			}
		}
		return r;
	}
}
