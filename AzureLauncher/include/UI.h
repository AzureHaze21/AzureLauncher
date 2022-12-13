#pragma once

#include <D3dx9tex.h>
#include <string>
#include <windows.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>
#pragma comment(lib, "D3dx9")
#include <vector>
#include <fstream>
#include <format>
#include <filesystem>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#include "Widgets.hpp"
#include "../../Blackbone/src/BlackBone/Process/Process.h"
#include "../../Blackbone/src/BlackBone/Process/RPC/RemoteFunction.hpp"
#include "globals.h"
#include "imspinner.h"
#include "Utils.h"
#include "images.h"
#include "AES.h"
#include "xorstr.h"

static bool CreateAndHook(std::wstring const& imgPath, const char* ip, const char* login, const char* password, bool bUseMod, bool bSpoofId)
{
	blackbone::Process proc;

	if (!std::filesystem::exists(imgPath))
	{
		MessageBoxA(0, "File not found", "Error", 0);
		return false;
	}
	
	const auto moduleAbsolutePath = std::filesystem::current_path().append("AzureHook.dll");

	if (!std::filesystem::exists(moduleAbsolutePath))
	{
		MessageBoxA(0, "Dll not found", "Error", 0);
		return false;
	}

	auto t = imgPath.substr(imgPath.size() - std::size(L"Dofus.exe") + 1, std::size(L"Dofus.exe"));
	int clientArch = !imgPath.substr(imgPath.size() - std::size(L"Dofus.exe") + 1, std::size(L"Dofus.exe")).compare(L"Dofus.exe") ? 1 : 0;

	if (!NT_SUCCESS(proc.CreateAndAttach(imgPath, true)))
	{
		MessageBoxA(0, "Failed to create process", "Error", 0);
		return false;
	}

	if (auto [mmapSuccess, mod] = proc.mmap().MapImage(moduleAbsolutePath.wstring()); NT_SUCCESS(mmapSuccess) && mod)
	{
		if (auto [success, fn] = proc.modules().GetExport(*mod, "HookProcess"); NT_SUCCESS(success) || !fn)
		{
			static int launcherPort = 26120;
			blackbone::RemoteFunction<void(__cdecl*)(const wchar_t, const char*, uint16_t, const char*, const char*, bool, int, bool)> hkFn(proc, blackbone::ptr_t(fn->procAddress));
			hkFn.Call({ moduleAbsolutePath.wstring().data(), ip, launcherPort++, login, password, bUseMod, clientArch, bSpoofId });
		}
		else
		{
			proc.Terminate();
			MessageBoxA(0, "Failed to call module function", "Error", 0);
			return false;
		}
	}
	else
	{
		proc.Terminate();
		MessageBoxA(0, "Failed to inject", "Error", 0);
		return false;
	}

	proc.Resume();
	proc.Detach();

	return true;
}

struct AdapterInfo
{
	std::string id;
	std::string desc;
	std::string ip;
	UINT type;
};

enum ProxyType
{
	UNKNOWN,
	HTTP,
	SOCKS5
};

inline std::map<ProxyType, const std::string> proxyTypeStrings
{
	{ ProxyType::UNKNOWN, "N/A" },
	{ ProxyType::HTTP, "HTTP" },
	{ ProxyType::SOCKS5, "SOCKS5" }
};

struct ProxyInfo
{
	std::string addr;
	int port;
	std::string login;
	std::string password;
	ProxyType type{ ProxyType::UNKNOWN };
};

struct AccountInfo
{
	int type{ 0 };
	std::string name;
	std::string login;
	std::string password;
	std::optional<AdapterInfo> adapterInfo;
	std::optional<ProxyInfo> proxy;
	bool bUseProxy = false;
	bool bUseMod = false;
	bool bSpoofId = false;
};

namespace LauncherUI
{
	static LPDIRECT3D9              g_pD3D = NULL;
	static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
	static D3DPRESENT_PARAMETERS    g_d3dpp = {};

	std::vector<AccountInfo>		accounts{};
	inline std::wstring				gamePath;
	std::string						pathStr;

	inline int						windowAlpha = 255;

	const unsigned char iv[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	};

	static auto LoadTextureFromFile = [&](const void* buffer, std::size_t size, PDIRECT3DTEXTURE9* out_texture, int* out_width, int* out_height)
	{
		// Load texture from disk
		PDIRECT3DTEXTURE9 texture;
		HRESULT hr = D3DXCreateTextureFromFileInMemory(LauncherUI::g_pd3dDevice, buffer, size, &texture);
		if (hr != S_OK)
			return false;

		// Retrieve description of the texture surface so we can access its size
		D3DSURFACE_DESC my_image_desc;
		texture->GetLevelDesc(0, &my_image_desc);
		*out_texture = texture;
		*out_width = (int)my_image_desc.Width;
		*out_height = (int)my_image_desc.Height;
		return true;
	};

	static std::optional<std::vector<AdapterInfo>> GetAllAdapters()
	{
		std::vector<AdapterInfo> adapterInfos;
		auto pAdapterInfos = std::make_unique<IP_ADAPTER_INFO[]>(1);
		ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
		DWORD dwRetVal = 0;

		if (GetAdaptersInfo(pAdapterInfos.get(), &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
			pAdapterInfos.release();
			pAdapterInfos = std::make_unique<IP_ADAPTER_INFO[]>(ulOutBufLen);
			if (pAdapterInfos == NULL) {
				printf("Error allocating memory needed to call GetAdaptersinfo\n");
				return {};
			}
		}
		if ((dwRetVal = GetAdaptersInfo(pAdapterInfos.get(), &ulOutBufLen)) == NO_ERROR) {
			auto pAdapter = pAdapterInfos.get();
			while (pAdapter) {
				adapterInfos.push_back({ pAdapter->AdapterName, pAdapter->Description, pAdapter->IpAddressList.IpAddress.String, pAdapter->Type });
				pAdapter = pAdapter->Next;
			}
		}

		return adapterInfos;
	}

	void Theme()
	{
		auto& style = ImGui::GetStyle();
		auto& io = ImGui::GetIO();

		style.WindowRounding = 3.0f;
		style.ChildRounding = 3.0f;
		style.FrameRounding = 3.0f;
		style.Colors[ImGuiCol_ChildBg] = ImColor(14, 19, 46, 255);
		style.Colors[ImGuiCol_WindowBg] = ImColor(4, 8, 31, 255);
		style.Colors[ImGuiCol_Button] = ImColor(39, 69, 201, 255);
		style.Colors[ImGuiCol_ButtonHovered] = ImColor(93, 117, 224, 255);
		style.Colors[ImGuiCol_ButtonActive] = ImColor(39, 69, 201, 150);
		style.Colors[ImGuiCol_TableHeaderBg] = ImColor(93, 117, 224, 255);
		style.Colors[ImGuiCol_Header] = ImColor(39, 69, 201, 50);
		style.Colors[ImGuiCol_HeaderActive] = ImColor(39, 69, 201, 50);
		style.Colors[ImGuiCol_HeaderHovered] = ImColor(39, 69, 201, 50);
		style.Colors[ImGuiCol_TitleBgActive] = ImColor(93, 117, 224, 255);
	}

	void RenderMenu()
	{
		static ImColor color(93, 117, 224, 255);
		static AccountInfo accountInfo;
		static int selectedClass = -1;
		static int selectedAccount = -1;
		static bool bIsOpen = true;
		static bool editAccount = false;
		static std::vector<std::pair<std::string, std::string>> classes{
			{ "", "" },
			{ ICON_FA_SKULL, "Sram" },
			{ ICON_FA_SWORD, " Iop"},
			{ ICON_FA_FLASK_POTION, " Eniripsa"},
			{ ICON_FA_DICE, " Ecaflip" },
			{ ICON_FA_SHOVEL, " Enutrof" },
			{ ICON_FA_BOW_ARROW, " Cra" },
			{ ICON_FA_HOURGLASS, " Xelor" },
			{ ICON_FA_STAFF, " Sadida" },
			{ ICON_FA_SHIELD_ALT, " Feca" }
		};

		auto addAccount = [&](AccountInfo const& account) {
			accounts.push_back(account);
		};

		auto clearAccounts = [&]() {
			accounts.clear();
		};

		static auto loadConfig = [&]() {
			if (std::filesystem::exists("config"))
			{
				std::ifstream in("config", std::ios::binary | std::ios::ate);
				std::size_t fileSize = in.tellg();
				std::vector<unsigned char> encryptedFile(fileSize);
				in.seekg(0);
				in.read((char*)encryptedFile.data(), encryptedFile.size());

				unsigned long padded_size = 0;
				std::vector<unsigned char> decrypted(fileSize);

				plusaes::decrypt_cbc(&encryptedFile[0], fileSize, (unsigned char*)xorstr_("AnkamaGames2022!"), xorstr("AnkamaGames2022!").size(), &LauncherUI::iv, &decrypted[0], decrypted.size(), &padded_size);

				auto json = nlohmann::json::parse(std::string((char*)decrypted.data()));

				auto path = json.contains("path") ? json["path"].get<std::string>() : "";

				pathStr = path;
				gamePath = std::wstring(path.begin(), path.end());

				if (json.contains("accounts"))
				{
					for (auto& acc : json["accounts"].get<nlohmann::json::array_t>())
					{
						std::string name = acc.contains("name") ? acc["name"].get<std::string>() : "";
						std::string login = acc.contains("login") ? acc["login"].get<std::string>() : "";
						std::string password = acc.contains("password") ? acc["password"].get<std::string>() : "";
						int type = acc.contains("type") ? acc["type"].get<int>() : 0;

						bool useProxy = acc.contains("useProxy") ? acc["useProxy"].get<bool>() : false;
						bool useMod = acc.contains("useMod") ? acc["useMod"].get<bool>() : false;
						bool spoofId = acc.contains("spoofId") ? acc["spoofId"].get<bool>() : false;

						auto adapterConfig = acc.contains("adapter") ? acc["adapter"].get<nlohmann::json::object_t>() : nlohmann::json::object_t{};
						std::string desc = adapterConfig.contains("desc") ? adapterConfig["desc"].get<std::string>() : "";
						std::string adapterIp = adapterConfig.contains("ip") ? adapterConfig["ip"].get<std::string>() : "";

						auto proxyConfig = acc.contains("proxy") ? acc["proxy"].get<nlohmann::json::object_t>() : nlohmann::json::object_t{};
						std::string proxyAddr = proxyConfig.contains("ip") ? proxyConfig["ip"].get<std::string>() : "";
						std::string proxyLogin = proxyConfig.contains("login") ? proxyConfig["login"].get<std::string>() : "";
						std::string proxyPassword = proxyConfig.contains("password") ? proxyConfig["password"].get<std::string>() : "";
						int proxyPort = proxyConfig.contains("port") ? proxyConfig["port"].get<int>() : 0;

						AccountInfo accountInfo;
						accountInfo.type = type;
						accountInfo.name = name;
						accountInfo.login = login;
						accountInfo.password = password;
						accountInfo.bUseProxy = useProxy;
						accountInfo.bUseMod = useMod;
						accountInfo.bSpoofId = spoofId;

						if (adapterIp.size() > 0)
						{
							accountInfo.adapterInfo = AdapterInfo{ .desc = desc, .ip = adapterIp, };
						}
						if (proxyAddr.size() > 0)
						{
							accountInfo.proxy = ProxyInfo{ .addr = proxyAddr, .port = proxyPort, .login = proxyLogin, .password = proxyPassword };
						}

						accounts.emplace_back(std::move(accountInfo));
					}
				}
			}
			return 0;
		}();

		auto openEditPopup = [&](const char* title) {
			static bool needRefresh = true;
			if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Class");

				if (ImGui::BeginCombo(
					"##ClassSelector",
					selectedClass > -1 ? std::format("{} {}", classes[accountInfo.type].first, classes[accountInfo.type].second.data()).data() : "Select a class (optional)"))
				{
					for (int i = 1; i < classes.size(); i++)
					{
						ImGui::TextColored(color, classes[i].first.data()); ImGui::SameLine();
						if (ImGui::Selectable(classes[i].second.data(), selectedClass == i, ImGuiSelectableFlags_SpanAllColumns))
						{
							selectedClass = i;
							accountInfo.type = i;
							/*std::memcpy(adapter.data(), adapterInfos.value()[i].ip.data(), std::size(adapter));*/
						}
					}
					ImGui::EndCombo();
				}

				if (accountInfo.name.size() < 256)
					accountInfo.name.resize(256);
				if (accountInfo.login.size() < 256)
					accountInfo.login.resize(256);
				if (accountInfo.password.size() < 256)
					accountInfo.password.resize(256);

				ImGui::Text("Alias"); ImGui::InputText("##Alias", accountInfo.name.data(), accountInfo.name.size());
				ImGui::Text("Login"); ImGui::InputText("##Login", accountInfo.login.data(), accountInfo.login.size());
				ImGui::Text("Password"); ImGui::InputText("##Password", accountInfo.password.data(), accountInfo.password.size(), ImGuiInputTextFlags_Password);

				static int selectedAdapter = -1;
				static auto adapterInfos = GetAllAdapters();

				if (adapterInfos && adapterInfos->size() > 0)
				{
					if (!accountInfo.adapterInfo)
						accountInfo.adapterInfo = AdapterInfo{ .ip = (*adapterInfos)[0].ip };
				}

				ImGui::Text("Adapter");

				if (ImGui::BeginCombo(
					"##Adapter",
					accountInfo.adapterInfo ? accountInfo.adapterInfo->ip.data() : "Select adapter"))
				{
					if (needRefresh)
					{
						adapterInfos = GetAllAdapters();
					}

					if (adapterInfos)
					{
						for (int i = 0; i < adapterInfos->size(); i++)
						{
							auto adapterTxt = std::format("{} ({})", adapterInfos.value()[i].desc.data(), adapterInfos.value()[i].ip.data());
							if (!adapterInfos.value()[i].ip.compare("127.0.0.1") || !adapterInfos.value()[i].ip.compare("0.0.0.0"))
							{
								ImGui::TextDisabled(adapterTxt.data());
							}
							else if (ImGui::Selectable(adapterTxt.data(), selectedAdapter == i))
							{
								selectedAdapter = i;
								accountInfo.adapterInfo = adapterInfos.value()[i];
							}
							else if (selectedAdapter == -1)
							{
								selectedAdapter == i;
							}
						}
					}

					needRefresh = false;

					ImGui::EndCombo();
				}
				else
					needRefresh = true;

				ImGui::Spacing();

				if (ImGui::Button("Ok", ImVec2(120, 0)))
				{
					if (!editAccount)
					{
						addAccount(accountInfo);
						accountInfo = {};
					}
					else
						accounts[selectedAccount] = accountInfo;
					editAccount = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					accountInfo = {};
					editAccount = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		};

		{
			ImGui::SetNextWindowSize({ 1280, 600 });

			if (ImGui::Begin("MainWindow", &bIsOpen, ImGuiWindowFlags_NoDecoration))
			{
				auto* dl = ImGui::GetForegroundDrawList();

				static PDIRECT3DTEXTURE9 img = nullptr;
				static int width, height;

				const auto [wX, wY] = ImGui::GetWindowPos();
				const auto [wW, wH] = ImGui::GetWindowSize();


				auto wndPos = ImGui::GetWindowPos();
				auto wndSize = ImGui::GetWindowSize();

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + wndSize.x - 42);

				if (ImGui::Button(ICON_FA_TIMES))
					exit(0);

				ImGui::Spacing();

				static PDIRECT3DTEXTURE9 logo1 = nullptr, logo2 = nullptr, logo3 = nullptr;
				static int image_width, image_height;

				if (logo1 == nullptr)
					LoadTextureFromFile(Images::dofus2_logo.data(), Images::dofus2_logo.size(), &logo1, &image_width, &image_height);

				if (logo2 == nullptr)
					LoadTextureFromFile(Images::dofus_logo.data(), Images::dofus_logo.size(), &logo2, &image_width, &image_height);

				if (logo3 == nullptr)
					LoadTextureFromFile(Images::brand_logo, std::size(Images::brand_logo), &logo3, &image_width, &image_height);

				ImGui::BeginChild("sideMenu", { 200, 0 }, false);

				ImGui::PushFont(ImGui::GetFont()->ContainerAtlas->Fonts[1]);
				auto pos = ImVec2{ ImGui::GetContentRegionAvail().x * .5f - ImGui::CalcTextSize(ICON_FA_BOLT "AZURE.VIP").x * .5f - 10, 12.0f };
				ImGui::SetCursorPos({ pos.x, pos.y + 4 });

				static float hue = 0;

				ImGui::GetWindowDrawList()->AddImage((void*)logo3, ImVec2(wX + 20, wY + 150), ImVec2(wX + 300 + 20, wY + 300 + 150), ImVec2(0, 0), ImVec2(1, 1), 0x20ffffff);

				ImGui::SameLine();
				auto txtPosX = ImVec2{ ImGui::GetContentRegionAvail().x * 0.5f - ImGui::CalcTextSize("AZURE.VIP").x * 0.5f, pos.y + 2 };
				ImGui::SetCursorPos({ txtPosX.x, txtPosX.y });  ImGui::Text("AZURE");
				ImGui::SameLine();
				ImGui::SetCursorPos({ txtPosX.x + ImGui::CalcTextSize("AZURE").x + 1, txtPosX.y }); ImGui::TextColored(color, ".VIP");
				ImGui::PopFont();
				ImGui::SetCursorPosX(12);
				ImGui::BeginGroup();
				ImGui::TextColored(ImColor(255, 255, 255, 170), "\n GAMES\n\n");
				ImGui::Image((void*)logo2, ImVec2(18, 18), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1)); ImGui::SameLine();
				ImGui::Selectable("Dofus Retro", true, ImGuiSelectableFlags_SpanAllColumns);

				const auto [cX, cY] = ImGui::GetCursorPos();

				ImGui::Spacing();
				ImGui::Image((void*)logo1, ImVec2(18, 18), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 0.5f)); ImGui::SameLine();
				ImGui::TextDisabled("Dofus 2.0");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Coming soon");
				ImGui::EndGroup();

				hue += 0.1f;

				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("container", { 0, 0 }, false, ImGuiWindowFlags_NoBackground);

				if (ImGui::BeginChild("TableContainer", { 0, ImGui::GetContentRegionAvail().y * .6f }, true, ImGuiWindowFlags_NoScrollbar))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 6, 8 });
					if (ImGui::BeginTable("AccountsList", 7, ImGuiTableFlags_NoSavedSettings))
					{
						//ImGui::TableNextColumn();
						//ImGui::TableNextColumn(); ImGui::TableHeader("Name");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Login");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Password");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Adapter");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Proxy");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Proxy Port");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Proxy Login");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Proxy Password");
						//ImGui::TableNextColumn(); ImGui::TableHeader("Proxy Type");

						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4, 14 });

						ImGui::TableSetupColumn("##icon_header", ImGuiTableColumnFlags_WidthFixed, 60.0f);
						ImGui::TableSetupColumn("Alias");
						//ImGui::TableSetupColumn("Login");
						//ImGui::TableSetupColumn("Password");
						ImGui::TableSetupColumn("Adapter ip");
						ImGui::TableSetupColumn("Proxy ip");
						ImGui::TableSetupColumn("Use Proxy", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Use Proxy").x + 8);
						ImGui::TableSetupColumn("Spoof ID", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Spoof ID").x + 8);
						ImGui::TableSetupColumn("Use Mod", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Use Mod").x + 8);
						//ImGui::TableSetupColumn("Proxy Address");
						//ImGui::TableSetupColumn("Proxy Port");
						//ImGui::TableSetupColumn("Proxy Login");
						//ImGui::TableSetupColumn("Proxy Password");
						/*ImGui::TableSetupColumn("Proxy Type");*/
						ImGui::TableHeadersRow();

						static std::vector<std::string> popupIds;

						if (accounts.size() > popupIds.size())
						{
							for (int i = popupIds.size(); i < accounts.size(); i++)
								popupIds.push_back(std::format("list_popup#{}", i));
						}

						static std::optional<std::function<void()>> actionToExecute;

						for (int i = 0; i < accounts.size(); i++)
						{
							auto& account = accounts[i];

							ImGui::TableNextRow();
							ImGui::TableNextColumn();

							ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 24);
							ImGui::Text(account.type < classes.size() ? classes[account.type].first.data() : "");

							ImGui::TableNextColumn();

							if (ImGui::Selectable(std::format("{}##{}", account.name.data(), i + 1).data(), selectedAccount == i, ImGuiSelectableFlags_SpanAllColumns))
							{
								selectedAccount = i;
							}
							if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
							{
								selectedAccount = i;
								ImGui::OpenPopup(popupIds[i].data());
							}

							static std::vector<std::pair<std::string, std::function<void(int)>>> popupActions{
								{ "Edit", [&,account](int id) {
									accountInfo = accounts[id];
									editAccount = true;
									ImGui::OpenPopup("Edit Account");
								}},
								{ "Duplicate", [](int id) { accounts.push_back(accounts[id]); }},
								{ "Remove", [](int id) { accounts.erase(accounts.begin() + id); }},
								{ "Test Connection", [](int id) { MessageBoxA(0, "OK", "Info", 0);  }}
							};


							static bool editAccount = false;

							if (ImGui::BeginPopup(popupIds[i].data()))
							{
								if (gamePath.size() > 0)
								{
									if (ImGui::Selectable("Launch"))
									{
										actionToExecute = [&account]() { CreateAndHook(gamePath, account.adapterInfo->ip.data(), account.login.data(), account.password.data(), account.bUseMod, account.bSpoofId); };
									}
								}
								else
								{
									ImGui::TextDisabled("Launch");
									if (ImGui::IsItemHovered())
									{
										ImGui::SetTooltip("Game path must be specified!");
									}
								}

								ImGui::Separator();

								for (auto& [menu, action] : popupActions)
								{
									if (ImGui::Selectable(menu.data()))
									{
										actionToExecute = [i, action]() { action(i); };
									}
								}

								ImGui::EndPopup();
							}

							if (editAccount)
							{
								ImGui::OpenPopup("Edit Account");
							}

							ImGui::TableNextColumn();
							if (account.adapterInfo)
							{
								ImGui::Text(account.adapterInfo->ip.data());
							}

							ImGui::TableNextColumn();
							if (account.proxy && account.proxy->addr.size() > 0)
								ImGui::Text(account.proxy->addr.data());
							else
								ImGui::TextDisabled("n/a");

							ImGui::TableNextColumn();

							ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x / 2.0f) - 16.0f);
							if (account.proxy && account.proxy->addr.size() > 0)
								ImGui::Toggle(std::format("#toggle_proxy{}", i).data(), &account.bUseProxy);
							else
								ImGui::TextDisabled("n/a");

							ImGui::TableNextColumn();
							ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x / 2.0f) - 16.0f);
							ImGui::Toggle(std::format("#spoof_id{}", i).data(), &account.bSpoofId);

							ImGui::TableNextColumn();
							ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x / 2.0f) - 16.0f);
							ImGui::Toggle(std::format("#toggle_mod{}", i).data(), &account.bUseMod);
						}

						if (actionToExecute)
						{
							(*actionToExecute)();
							actionToExecute.reset();
						}

						ImGui::PopStyleVar();

						ImGui::EndTable();
					}

					ImGui::PopStyleVar();
					ImGui::EndChild();
				}

				if (editAccount)
					ImGui::OpenPopup("Edit Account");

				static float alpha = 0.0f;
				static int d = 0;

				if (alpha > 0.0f)
					alpha -= 0.002f;

				openEditPopup("Edit Account");

				{
					ImGui::SetCursorPos({ ImGui::GetContentRegionAvail().x - 520, ImGui::GetCursorPosY() - 4 });

					openEditPopup("Add Account");

					ImGui::BeginGroup();

					ImVec2 center = ImGui::GetWindowViewport()->GetCenter();
					// Always center this window when appearing
					ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

					if (ImGui::Button("  " ICON_FA_USER_PLUS " Add Account  ", ImVec2(120, 30)))
						ImGui::OpenPopup("Add Account");

					ImGui::SameLine();
					if (ImGui::Button("  " ICON_FA_SAVE " Save config  ", ImVec2(120, 30)))
					{
						std::string json = "{";

						if (gamePath.size() > 0)
							json += std::format(R"("path":"{}")", Utils::utf8_encode(gamePath).data());

						if (accounts.size() > 0)
						{
							if (gamePath.size() > 0)
								json += ',';

							json += "\"accounts\": [";

							for (int i = 0; i < accounts.size(); i++)
							{
								json += std::format(R"({{"name":"{}","login":"{}","password":"{}","type":{},"useProxy":{},"useMod":{},"spoofId":{})",
									accounts[i].name.data(),
									accounts[i].login.data(),
									accounts[i].password.data(),
									accounts[i].type,
									accounts[i].bUseProxy,
									accounts[i].bUseMod,
									accounts[i].bSpoofId);

								if (accounts[i].adapterInfo)
								{
									json += std::format(R"(,"adapter":{{"ip":"{}","desc":"{}","type":{}}})",
										accounts[i].adapterInfo->ip.data(),
										accounts[i].adapterInfo->desc.data(),
										accounts[i].adapterInfo->type);
								}
								json += "}";
								if (i != accounts.size() - 1)
									json += ',';
							}
							json += "]";
						}
						json += "}";

						std::replace_if(json.begin(), json.end(), [](const char& c) { return c == '\\'; }, '/');

						// encrypt config
						const unsigned long encrypted_size = plusaes::get_padded_encrypted_size(json.size());
						std::vector<unsigned char> encrypted(encrypted_size);

						plusaes::encrypt_cbc((unsigned char*)json.data(), json.size(), (unsigned char*)xorstr_("AnkamaGames2022!"), xorstr("AnkamaGames2022!").size(), &LauncherUI::iv, &encrypted[0], encrypted.size(), true);

						std::ofstream out("config", std::ios::binary);

						out.write((char*)encrypted.data(), encrypted.size());
						out.close();

						alpha = 1.0f;
					}

					ImGui::GetForegroundDrawList()->AddText({ wndPos.x + wndSize.x - 180, wndPos.y + wndSize.y - 24 }, ImGui::ColorConvertFloat4ToU32({ 1.0, 1.0, 1.0, alpha }), "Config saved sucessfully");

					static bool dont_ask_me_next_time = false;

					ImGui::SameLine();
					if (ImGui::Button("  " ICON_FA_TRASH_ALT " Remove All  ", ImVec2(120, 30)))
						ImGui::OpenPopup("Confirm");

					if (ImGui::BeginPopupModal("Confirm", NULL, ImGuiWindowFlags_AlwaysAutoResize))
					{
						ImGui::Text("\n    Are you sure you want to remove all accounts?    \n\n");
						ImGui::Separator();

						if (ImGui::Button("Yes", ImVec2(120, 0)))
						{
							clearAccounts();
							ImGui::CloseCurrentPopup();
						}
						ImGui::SetItemDefaultFocus();
						ImGui::SameLine();
						if (ImGui::Button("Cancel", ImVec2(120, 0)))
						{
							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}

					ImGui::SameLine();
					if (ImGui::Button("  " ICON_FA_PLAY " Launch All  ", ImVec2(120, 30)))
					{
						for (auto& account : accounts)
						{
							CreateAndHook(gamePath, account.adapterInfo->ip.data(), account.login.data(), account.password.data(), account.bUseMod, account.bSpoofId);
						}
					}

					ImGui::EndGroup();
				}

				{
					ImGui::Text("\n");
					ImGui::BeginChild("controls", { 0, 75 }, true);

					ImGui::Text(" " ICON_FA_FOLDER " Game path");
					ImGui::InputText("##path_input", pathStr.data(), pathStr.size(), ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					if (ImGui::Button("  " ICON_FA_HDD " Select  "))
					{
						std::wstring path = Utils::OpenFileDialog();
						if (path.size() > 0)
						{
							gamePath = path;
							pathStr = Utils::utf8_encode(gamePath);

						}
					}

					ImGui::EndChild();
				}

				ImGui::EndChild();

				ImGui::GetWindowDrawList()->AddText(ImVec2(wX + wW - ImGui::CalcTextSize(ICON_FA_COPYRIGHT " 2022 - AzureProd").x - 10, wY + wH - 25), 0x40ffffff, ICON_FA_COPYRIGHT " 2022 - AzureProd");

				ImGui::End();
			}
		}
	}

	void RenderLoginPage(const float width, const float height)
	{
		static PDIRECT3DTEXTURE9 img = nullptr;
		static int image_width, image_height;

		if (img == nullptr)
			LoadTextureFromFile(Images::login_screen.data(), Images::login_screen.size(), &img, &image_width, &image_height);

		const auto& io = ImGui::GetIO();

		ImGui::SetNextWindowSize(ImVec2(width, height));
		ImGui::SetNextWindowPos(ImVec2(0.5f * GetSystemMetrics(SM_CXSCREEN), 0.5f * GetSystemMetrics(SM_CYSCREEN)), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
		ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoDecoration);

		const auto [wX, wY] = ImGui::GetWindowPos();
		const auto [wW, wH] = ImGui::GetWindowSize();

		ImGui::GetWindowDrawList()->AddImage((void*)img, ImVec2(wX, wY), ImVec2(wX + wW, wY + wH), ImVec2(0, 0), ImVec2(1, 1), 0x20ffffff);
		ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(wX, wY), ImVec2(wX + wW, wY + wH), 0xff000000, 0x10000000, 0x10000000, 0xff000000);
		ImGui::TextDisabled("Welcome");
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 25); ImGui::TextDisabled(ICON_FA_TIMES);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}
		if (ImGui::IsItemClicked())
		{
			exit(0);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, 0x10000000);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 12.0f));
		ImGui::Text("\n\n\n");
		ImGui::SetCursorPosX(50);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
		ImGui::BeginGroup();
		ImGui::Text("License Key");
		ImGui::SetNextItemWidth(wW - 100.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
		ImGui::InputText("##login_input", globals::login.data(), globals::login.size());
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		ImGui::EndGroup();
		ImGui::Text("\n");
		ImGui::SetCursorPosX(50 /*50.0f + (wW - 200) * .5f*/);

		static int loginStatus = 0;

		if (ImGui::Button("Sign In", ImVec2(wW - 100.0f, 40)))
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			loginStatus = 1;

			CreateThread(nullptr, 0, [](LPVOID lpData) -> DWORD {
				Sleep(2000);
				loginStatus = 3;
				return 0;
				}, nullptr, 0, nullptr);
		}

		if (loginStatus == 1)
		{
			static float spinnerSize = 20.0f;
			ImGui::SetCursorPosX(250 - spinnerSize);
			ImSpinner::SpinnerLemniscate("##logo", spinnerSize, 2, ImColor(0.153f, 0.271f, 0.788f, .900f), 6, 7);
		}
		else if (loginStatus == 2)
		{
			//globals::bAuthenticated = true;
			static float spinnerSize = 20.0f;
			ImGui::SetCursorPosX(250 - spinnerSize);
			ImSpinner::SpinnerLemniscate("##logo", spinnerSize, 2, ImColor(0.153f, 0.271f, 0.788f, .900f), 6, 7);
			ImGui::SetCursorPosX(250 - ImGui::CalcTextSize("Checking for update...").x * .5f);
			ImGui::TextColored(ImColor(0.153f, 0.271f, 0.788f, 1.00f), "Checking for update...");
		}
		else if (loginStatus == 3)
		{
			globals::bAuthenticated = true;
		}
		else if (loginStatus == -1)
		{
			ImGui::SetCursorPosX(250 - ImGui::CalcTextSize("Login failed").x * .5f);
			ImGui::TextColored(ImColor(0.153f, 0.271f, 0.788f, 1.00f), "\nLogin failed");
		}

		ImGui::GetWindowDrawList()->AddText(ImVec2(wX + wW - ImGui::CalcTextSize(ICON_FA_COPYRIGHT " 2022 - AzureProd").x - 20, wY + wH - 30), 0x30ffffff, ICON_FA_COPYRIGHT " 2022 - AzureProd");
		ImGui::End();
	}

	void Render()
	{
		if (globals::bAuthenticated)
		{
			RenderMenu();
		}
		else
		{
			RenderLoginPage(500, 500);
		}
	}
}