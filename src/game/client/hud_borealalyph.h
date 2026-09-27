//========= Copyright (c) Rebuild Team, 2026 ===================================//
//
// Purpose:
//
//=============================================================================//

#ifndef HUD_BOREALALYPH_H
#define HUD_BOREALALYPH_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include "weapon_selection.h"

class C_BasePlayer;

class CHudBorealAlyph : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudBorealAlyph, vgui::Panel);

public:
	CHudBorealAlyph(const char* pElementName);
	virtual ~CHudBorealAlyph();

	virtual void Init(void);
	virtual void VidInit(void);
	virtual void Reset(void);
	virtual bool ShouldDraw(void);
	virtual void OnThink(void);
	virtual void Paint(void);
	virtual void ApplySchemeSettings(vgui::IScheme* pScheme);

	void MsgFunc_Damage(bf_read& msg);
	void MsgFunc_Battery(bf_read& msg);

	static void UpdateDefaultHudElements(void);

private:
	void PaintHealth(C_BasePlayer* pPlayer, int x, int y);
	int PaintArmor(C_BasePlayer* pPlayer, int x, int y);

	void PaintLimitedHEV(C_BasePlayer* pPlayer, int x, int y);
	void PaintLimitedFlashlight(C_BasePlayer* pPlayer, int x, int y);

	void PaintAmmo(C_BasePlayer* pPlayer, int x, int y);
	void DrawPrimaryAmmo(C_BasePlayer* pPlayer, C_BaseCombatWeapon* pWeapon, int x, int y);
	int DrawSecondaryAmmo(C_BasePlayer* pPlayer, C_BaseCombatWeapon* pWeapon, int x, int y);

	void PaintCrosshair(C_BasePlayer* pPlayer);
	void DrawCrosshairGeneric(int x, int y, float accuracy, float flBarLength);
	void DrawCrosshairRifle(int x, int y, float accuracy);
	void DrawCrosshairRevolver(int x, int y, float accuracy);
	void DrawCrosshairShotgun(int x, int y, float accuracy);
	void CalculateWorldCrosshair(float* pX, float* pY, bool* pbBehindCamera);
	void CalculateStaticCrosshair(float* pX, float* pY, bool* pbBehindCamera);
	bool IsPlayerZooming(void);

	void OnPlayerDamaged(int damageTaken, const Vector& vecFrom);
	void UpdatePlayerFeedback(C_BasePlayer* pPlayer);
	void UpdateWeaponFeedback(C_BasePlayer* pPlayer);
	void CheckHitMarker(C_BasePlayer* pPlayer);
	void PaintDamageDirection(void);
	void PaintCrosshairFeedback(int x, int y);
	void PaintWeaponName(C_BaseCombatWeapon* pWeapon, int x, int y);
	vgui::Color GetAmmoFeedbackColor(void);

	int m_iHealth;
	int m_iArmor;
	int m_iNewArmor;
	float m_flSuitPower;

	float m_flLastFullSuit;
	float m_flLastFullSuitFade;
	float m_flLastNonFullSuit;
	float m_flLastNonFullSuitFade;
	bool m_bLastNonFullSuitStatus;

	float m_flLastSuitAltLabel;
	char m_szLastSuitAltLabelLast[32];
	float m_flLastSuitAltLabelLastDec;
	float m_flLastSuitAltLabelLastDecT;

	Vector m_vecPrevCrossPos;
	bool m_bHavePrevCrossPos;
	C_BaseCombatWeapon* m_pPrevCrossWeapon;
	int m_iPrevCrossClip;
	float m_flCrossBloom;

	float m_flHudFade;

	int m_iPrevHealth;
	int m_iPrevArmor;
	float m_flPrevSuitPower;

	float m_flHealthPulse;
	float m_flHealthHeal;
	float m_flArmorPulse;
	float m_flArmorHeal;
	float m_flAmmoPulse;
	float m_flAmmoReload;
	float m_flHEVPulse;
	float m_flFlashlightPulse;
	bool m_bLastFlashlightOn;

	float m_flDamageShake;
	Vector m_vecDamageFrom;
	float m_flDamageDirTime;
	float m_flDamageDirFade;
	float m_flDamageDirAmount;
	bool m_bDamageDirValid;

	CHandle<C_BaseCombatWeapon> m_hPrevAmmoWeapon;
	int m_iPrevClip1;
	int m_iPrevClip2;
	int m_iPrevAmmo1;
	int m_iPrevAmmo2;
	float m_flWeaponSwitchTime;
	float m_flWeaponSwitchFade;

	bool m_bPrevAttackDown;
	float m_flHitMarker;

	vgui::HFont m_hFontCounter;
	vgui::HFont m_hFontCounterStored;
	vgui::HFont m_hFontHEVCounter;
	vgui::HFont m_hFontCounterText;
	vgui::HFont m_hFontIcon;
	vgui::HFont m_hFontHEVIcon;
	vgui::HFont m_hFontFlashlightIcon;
};

class CHudBAWeaponSelection : public CBaseHudWeaponSelection, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudBAWeaponSelection, vgui::Panel);

public:
	CHudBAWeaponSelection(const char* pElementName);
	virtual ~CHudBAWeaponSelection();

	virtual bool ShouldDraw();
	virtual void OnWeaponPickup(C_BaseCombatWeapon* pWeapon);
	virtual void CycleToNextWeapon(void);
	virtual void CycleToPrevWeapon(void);
	virtual C_BaseCombatWeapon* GetWeaponInSlot(int iSlot, int iSlotPos);
	virtual void SelectWeaponSlot(int iSlot);
	virtual C_BaseCombatWeapon* GetSelectedWeapon(void) { return m_hSelectedWeapon; }
	virtual void OpenSelection(void);
	virtual void HideSelection(void);
	virtual void LevelInit();

	virtual void SelectWeapon(void);
	virtual void CancelWeaponSelection(void);
	static void YieldInstanceToStock(void);

protected:
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme* pScheme);

	virtual bool IsWeaponSelectable()
	{
		if (IsInSelectionMode())
			return true;

		return false;
	}

private:
	C_BaseCombatWeapon* FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	C_BaseCombatWeapon* FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	int GetLastPosInSlot(int iSlot) const;
	int GetWeaponsCountInSlot(int iSlot);
	void FastWeaponSwitch(int iWeaponSlot);
	void CallWeaponSelectorDeny(void);
	void ResetSelectorFade(void);
	float GetSelectorAlpha(void);

	void DrawWeaponSelector(C_BasePlayer* pPlayer);

	virtual void SetSelectedWeapon(C_BaseCombatWeapon* pWeapon) { m_hSelectedWeapon = pWeapon; }
	virtual void SetSelectedSlot(int slot) { m_iSelectedSlot = slot; }

	int m_iSelectedSlot;
	int m_iSelectedPos;
	float m_flSelectorStartFade;
	float m_flSelectorEndFade;
	float m_flDenyFadeout;
	float m_flDenyFadeouts;

	float m_flSelectorOpenTime;
	float m_flDenyShake;
	float m_flPickupFlash;
	int m_iPickupSlot;

	vgui::HFont m_hFontSmallNumbers;
	vgui::HFont m_hFontWeaponName;
	vgui::HFont m_hFontNames;
};

#endif // HUD_BOREALALYPH_H
