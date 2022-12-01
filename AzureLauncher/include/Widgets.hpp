#pragma once
#include <imgui.h>
#include <string>
#include <format>
#include <map>

namespace ImGui
{
	void HiddenText(std::string const& txt, bool hidden = true)
	{
		if (hidden)
			ImGui::Text(std::string(txt.size(), '*').data());
		else
			ImGui::Text(txt.data());
	}

	bool Toggle(const char* name, bool* toggled)
	{
		if (name == nullptr || toggled == nullptr)
			return false;

		static std::map<std::string, float> transitions;

		auto wndPos = ImGui::GetWindowPos();
		auto scrollY = ImGui::GetScrollY();
		auto knobPos = ImVec2{ wndPos.x + ImGui::GetCursorPosX() + 8 + (transitions[name] * 15.f), wndPos.y - scrollY + ImGui::GetCursorPosY() + 8 };

		if (!transitions.contains(name))
		{
			transitions[name] = *toggled;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.153f, 0.271f, 0.788f, transitions[name]));
		ImGui::PushStyleColor(ImGuiCol_Border, 0x50ffffff);

		ImGui::BeginChild(name, { 32, 16 }, true);

		if (ImGui::InvisibleButton(std::format("#ib_{}", name).data(), { 32, 16 }))
		{
			*toggled = !*toggled;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}
		if (*toggled && transitions[name] < 1.0f)
		{
			transitions[name] += 0.03f;
		}
		else if (!*toggled && transitions[name] > 0.0f)
		{
			transitions[name] -= 0.03f;
		}

		ImGui::GetWindowDrawList()->AddCircleFilled(knobPos, 6, 0xffffffff);

		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);

		return *toggled;
	}
}