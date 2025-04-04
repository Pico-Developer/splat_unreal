/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "SplatSettings.h"
#include "import/splat_logging.h"

void splat_log_recv(Level level, const char* message)
{
	switch (level)
	{
	case Level::ERROR:
	{
		UE_LOG(LogPICOSplat, Error, TEXT("%hs"), message);
		break;
	}
	case Level::WARNING:
	{
		UE_LOG(LogPICOSplat, Warning, TEXT("%hs"), message);
		break;
	}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Required by LOCTEXT.
#define LOCTEXT_NAMESPACE "PICOSplatEditor"

namespace PICO::Splat
{

/**
 * Unreal requires a class definition for a module.
 * All Editor-only logic (e.g. `ply` import) lives within this module.
 */
class FPICOSplatEditorModule final : public IModuleInterface
{
	virtual void StartupModule() override
	{
		// Register splat log handler.
		set_log_recv(splat_log_recv);

		// Register settings page.
		ISettingsModule* SettingsModule =
			FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->RegisterSettings(
				"Project",
				"Plugins",
				"PICO Splat",
				LOCTEXT("RuntimeSettingsName", "PICO Splat"),
				LOCTEXT(
					"RuntimeSettingsDescription", "PICO Splat configuration."),
				GetMutableDefault<USplatSettings>());
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule =
			FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->UnregisterSettings(
				"Project", "Plugins", "PICO Splat");
		}
	}
};

} // namespace PICO::Splat

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(PICO::Splat::FPICOSplatEditorModule, PICOSplatEditor);