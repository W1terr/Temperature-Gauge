#include "SmfHuds.h"

#include "Framework.h"

namespace SmfHuds
{
	namespace
	{
		using Callback = void(__stdcall*)();
		using RegisterFn = std::int64_t (*)(Callback);
		using GetProcFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);

		constexpr std::array kFolders{ "Data/SKSE/Plugins/TemperatureGauge/Sources", "Data/SKSE/Plugins/TemperatureGauge/Needs",
			"Data/SKSE/Plugins/TemperatureGauge/Stress" };

		struct Slot
		{
			std::string           dll;   // lower case, no ".dll"
			std::array<float, 4>  keep{};  // left, top, right, bottom (fractions of the screen); all 0: nothing kept
			bool                  patched{ false };
			std::atomic<Callback> callback{ nullptr };  // the DLL's own draw function
			std::atomic<bool>     hidden{ false };
		};

		constexpr std::size_t    kSlots = 4;
		std::array<Slot, kSlots> slots;
		std::size_t              slotCount = 0;
		std::once_flag           scanned;

		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		bool HudUp()
		{
			const auto ui = RE::UI::GetSingleton();
			return ui && !ui->GameIsPaused() && !SKSEMenuFramework::IsAnyBlockingWindowOpened();
		}

		template <std::size_t I>
		void __stdcall Draw()
		{
			auto&      slot = slots[I];
			const auto callback = slot.callback.load();
			if (!callback) {
				return;
			}
			if (!slot.hidden || !HudUp()) {
				callback();
				return;
			}
			using namespace ImGuiMCP;
			const auto  list = GetForegroundDrawList();
			const auto  screen = GetIO()->DisplaySize;
			const auto& k = slot.keep;
			ImDrawListManager::PushClipRect(list, { k[0] * screen.x, k[1] * screen.y }, { k[2] * screen.x, k[3] * screen.y }, false);
			callback();
			ImDrawListManager::PopClipRect(list);
		}

		template <std::size_t I>
		std::int64_t RegisterHud(Callback a_callback)
		{
			const auto real = reinterpret_cast<RegisterFn>(::GetProcAddress(GetMenuFrameworkModule(), "RegisterHudElement"));
			if (!real) {
				return 0;
			}
			Callback none = nullptr;
			if (a_callback && slots[I].callback.compare_exchange_strong(none, a_callback)) {
				logs::info("{}.dll registered its HUD with SKSE Menu Framework: wrapped, hidden while our gauges stand in for it", slots[I].dll);
				return real(&Draw<I>);
			}
			return real(a_callback);  // a second HUD element of the same DLL stays as it is
		}

		template <std::size_t I>
		FARPROC WINAPI GetProc(HMODULE a_module, LPCSTR a_name)
		{
			if (reinterpret_cast<std::uintptr_t>(a_name) > 0xFFFF && a_module && a_module == GetMenuFrameworkModule() &&
				std::strcmp(a_name, "RegisterHudElement") == 0) {
				return reinterpret_cast<FARPROC>(&RegisterHud<I>);
			}
			return ::GetProcAddress(a_module, a_name);
		}

		template <std::size_t... I>
		constexpr std::array<GetProcFn, sizeof...(I)> MakeHooks(std::index_sequence<I...>)
		{
			return { &GetProc<I>... };
		}
		constexpr auto kHooks = MakeHooks(std::make_index_sequence<kSlots>{});

		// points the module's imports of GetProcAddress (by name) at a_hook
		bool PatchImport(HMODULE a_module, GetProcFn a_hook)
		{
			const auto  base = reinterpret_cast<std::uint8_t*>(a_module);
			const auto  dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const auto  nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
			const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE || !dir.VirtualAddress) {
				return false;
			}
			bool done = false;
			for (auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp) {
				if (!imp->OriginalFirstThunk) {
					continue;  // no names to go by
				}
				auto thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + imp->FirstThunk);
				auto names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + imp->OriginalFirstThunk);
				for (; thunk->u1.Function; ++thunk, ++names) {
					if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) {
						continue;
					}
					const auto byName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
					if (std::strcmp(reinterpret_cast<const char*>(byName->Name), "GetProcAddress") != 0) {
						continue;
					}
					DWORD old = 0;
					if (VirtualProtect(&thunk->u1.Function, sizeof(ULONGLONG), PAGE_READWRITE, &old)) {
						thunk->u1.Function = reinterpret_cast<ULONGLONG>(a_hook);
						VirtualProtect(&thunk->u1.Function, sizeof(ULONGLONG), old, &old);
						done = true;
					}
				}
			}
			return done;
		}

		void Collect(const json& a_node)
		{
			if (a_node.is_array()) {
				for (const auto& item : a_node) {
					Collect(item);
				}
				return;
			}
			if (!a_node.is_object()) {
				return;
			}
			for (const auto& [key, value] : a_node.items()) {
				if (key != "smfHud") {
					Collect(value);
					continue;
				}
				for (const auto& hud : value.is_array() ? value : json::array({ value })) {
					const auto dll = Lower(hud.value("dll", ""s));
					if (dll.empty() || std::ranges::any_of(std::span{ slots.data(), slotCount }, [&](const Slot& s) { return s.dll == dll; })) {
						continue;
					}
					if (slotCount == kSlots) {
						logs::warn("SKSE Menu Framework HUDs: more than {} to hide, {}.dll left out", kSlots, dll);
						continue;
					}
					auto& slot = slots[slotCount++];
					slot.dll = dll;
					const auto keep = hud.value("keep", std::vector<float>{});
					for (std::size_t i = 0; i < 4 && i < keep.size(); ++i) {
						slot.keep[i] = keep[i];
					}
				}
			}
		}

		void Scan()
		{
			for (const auto* folder : kFolders) {
				std::error_code ec;
				for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
					if (entry.path().extension() != ".json") {
						continue;
					}
					try {
						std::ifstream file{ entry.path() };
						Collect(json::parse(file));
					} catch (const std::exception&) {
						// reported when the definitions are read
					}
				}
			}
		}
	}

	void Install()
	{
		std::call_once(scanned, Scan);
		for (std::size_t i = 0; i < slotCount; ++i) {
			auto& slot = slots[i];
			if (slot.patched) {
				continue;
			}
			const auto module = GetModuleHandleA((slot.dll + ".dll").c_str());
			if (!module) {
				continue;
			}
			slot.patched = PatchImport(module, kHooks[i]);
			if (slot.patched) {
				logs::info("{}.dll: its SKSE Menu Framework HUD can be hidden", slot.dll);
			} else {
				logs::warn("{}.dll: no GetProcAddress import found, its HUD can't be hidden", slot.dll);
			}
		}
	}

	void Hide(const std::vector<std::string>& a_dlls)
	{
		for (std::size_t i = 0; i < slotCount; ++i) {
			auto&      slot = slots[i];
			const bool hide = std::ranges::any_of(a_dlls, [&](const std::string& d) { return _stricmp(d.c_str(), slot.dll.c_str()) == 0; });
			if (slot.hidden.exchange(hide) != hide && slot.callback.load()) {
				logs::info("{}.dll's HUD: {}", slot.dll, hide ? "hidden (our gauges stand in for it)" : "shown again");
			}
		}
	}
}
