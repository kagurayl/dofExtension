#pragma once

#include <cstdint>

namespace dofext::address {

// --------------------------------------------------------------------------
// Game-client absolute addresses encoded by the unpacked DLL.
// These are not RVAs. The original code calls/reads them directly.
// --------------------------------------------------------------------------
inline constexpr std::uintptr_t kScreenXGlobal       = 0x004B1258;
inline constexpr std::uintptr_t kDrawTips            = 0x004C4690;
inline constexpr std::uintptr_t kPushObjectByName    = 0x006847B0;
inline constexpr std::uintptr_t kDrawItemSlot        = 0x007AA800;
inline constexpr std::uintptr_t kResolveItemSlot     = 0x007AAB60;
inline constexpr std::uintptr_t kSkillCooldown       = 0x00904440;
inline constexpr std::uintptr_t kDrawSkillIcon       = 0x00905750;
inline constexpr std::uintptr_t kSendNotice          = 0x009536C0;
inline constexpr std::uintptr_t kPacketBegin         = 0x01127D60;
inline constexpr std::uintptr_t kPacketSend          = 0x01127EC0;
inline constexpr std::uintptr_t kPacketWriteByte     = 0x01128550;
inline constexpr std::uintptr_t kPacketWriteWord     = 0x01128580;
inline constexpr std::uintptr_t kPacketWriteDword    = 0x011285B0;
inline constexpr std::uintptr_t kPacketWriteQword    = 0x011285E0;
inline constexpr std::uintptr_t kSqCall              = 0x01359280;
inline constexpr std::uintptr_t kImageDraw           = 0x011A97E0;
inline constexpr std::uintptr_t kImageLookup         = 0x011AA190;
inline constexpr std::uintptr_t kImagePackageLookup  = 0x011C0410;
inline constexpr std::uintptr_t kTextBegin           = 0x01206550;
inline constexpr std::uintptr_t kTextEnd             = 0x01206570;
inline constexpr std::uintptr_t kTextDraw            = 0x01206BD0;

inline constexpr std::uintptr_t kScaleGlobal         = 0x01556210;
inline constexpr std::uintptr_t kEncryptWordTableA   = 0x01843F58;
inline constexpr std::uintptr_t kEncryptWordTableB   = 0x01844158;
inline constexpr std::uintptr_t kNoticeContextGlobal = 0x01A5FB20;
inline constexpr std::uintptr_t kGameStateGlobal     = 0x01A5FB4C;
inline constexpr std::uintptr_t kTextContextGlobal   = 0x01A74360;
inline constexpr std::uintptr_t kTownAreaRootGlobal  = 0x01A5E258;
inline constexpr std::uintptr_t kCharacterGlobal     = 0x01AB7CDC;
inline constexpr std::uintptr_t kPacketWriterGlobal  = 0x01AEB6E4;
inline constexpr std::uintptr_t kEncodedFieldTables  = 0x01AF8D78;
inline constexpr std::uintptr_t kEncodeCounter       = 0x01AF8DB8;
inline constexpr std::uintptr_t kSquirrelVmGlobal    = 0x01AF3544;
inline constexpr std::uintptr_t kRendererGlobal      = 0x01B45B94;
inline constexpr std::uintptr_t kImageManagerGlobal  = 0x01B4684C;

// --------------------------------------------------------------------------
// Original unpacked DLL runtime addresses for the observed load base 66490000.
// Runtime RVA = VA - 66490000. Preferred VA = 10000000 + RVA.
// --------------------------------------------------------------------------
inline constexpr std::uintptr_t kObservedDllBase = 0x66490000;
inline constexpr std::uintptr_t kPreferredDllBase = 0x10000000;
inline constexpr std::uintptr_t kRegisterExtensionsObserved = 0x664F7010;
inline constexpr std::uintptr_t kPacketWriteHelperObserved  = 0x664F06C0;
inline constexpr std::uintptr_t kEncodeFieldHelperObserved  = 0x664F3C30;
inline constexpr std::uintptr_t kParseOffsetPathObserved    = 0x664F3F90;
inline constexpr std::uintptr_t kDecodePathHelperObserved   = 0x664F3CE0;

// Squirrel operations inside the unpacked original DLL at the observed base.
inline constexpr std::uintptr_t kSqGetBoolObserved       = 0x666AC910;
inline constexpr std::uintptr_t kSqGetFloatObserved      = 0x666ACA10;
inline constexpr std::uintptr_t kSqGetObjectObserved     = 0x666ACA70;
inline constexpr std::uintptr_t kSqGetIntegerObserved    = 0x666ACB00;
inline constexpr std::uintptr_t kSqGetStringObserved     = 0x666ACE00;
inline constexpr std::uintptr_t kSqNewClosureObserved    = 0x666ACFA0;
inline constexpr std::uintptr_t kSqNewSlotObserved       = 0x666AD0C0;
inline constexpr std::uintptr_t kSqPushFloatObserved     = 0x666AD3D0;
inline constexpr std::uintptr_t kSqPushIntegerObserved   = 0x666AD450;
inline constexpr std::uintptr_t kSqPushStringObserved   = 0x666AD590;
inline constexpr std::uintptr_t kSqPushObjectObserved   = 0x666ACC10; // object-value helper seen in readValue
inline constexpr std::uintptr_t kSqRaiseErrorObserved    = 0x666AE270;

// Recovered host object-layout constants.
inline constexpr std::uint32_t kItemSlotRootOffset    = 0x52F4;
inline constexpr std::uint32_t kItemSlotTableOffset   = 56;
inline constexpr std::uint32_t kItemSlotValueOffset   = 28;
inline constexpr std::uint32_t kEncodedPayloadOffset  = 8;
inline constexpr std::uint32_t kEncodedStride         = 12;
inline constexpr std::uint32_t kEncodedLookupBias     = 8468;

// cqx_getInfoContent: exact game query and allocator-release addresses.
inline constexpr std::uintptr_t kInfoContentQuery     = 0x011A2150;
inline constexpr std::uintptr_t kInfoContentContext   = 0x01D17638;
inline constexpr std::uintptr_t kGameBufferFree       = 0x013CC64A;

constexpr std::uintptr_t OriginalRva(std::uintptr_t observed_va) {
    return observed_va - kObservedDllBase;
}

constexpr std::uintptr_t PreferredVa(std::uintptr_t observed_va) {
    return kPreferredDllBase + OriginalRva(observed_va);
}

} // namespace dofext::address
