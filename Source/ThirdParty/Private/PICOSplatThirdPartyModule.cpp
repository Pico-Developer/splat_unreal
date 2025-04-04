/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "Modules/ModuleManager.h"

namespace PICO::Splat
{

class FPICOSplatThirdPartyModule final : public IModuleInterface
{
};

} // namespace PICO::Splat

IMPLEMENT_MODULE(PICO::Splat::FPICOSplatThirdPartyModule, PICOSplatThirdParty);