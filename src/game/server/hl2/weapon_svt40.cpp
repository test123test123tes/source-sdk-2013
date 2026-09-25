//========= Copyright (c) Rebuild Team, 2026 ===================================//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "ammodef.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "gamestats.h"
#include "rumble_shared.h"
#include "tier0/memdbgon.h"

#define SVT40_FASTEST_REFIRE_TIME 0.21f

ConVar sk_plr_dmg_svt40("sk_plr_dmg_svt40", "40", FCVAR_REPLICATED);
ConVar sk_npc_dmg_svt40("sk_npc_dmg_svt40", "15", FCVAR_REPLICATED);
ConVar sk_max_svt40("sk_max_svt40", "60", FCVAR_REPLICATED);

class CWeaponSVT40 : public CHLSelectFireMachineGun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponSVT40, CHLSelectFireMachineGun);
	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponSVT40(void);

	void Precache(void);
	void PrimaryAttack(void);
	void SecondaryAttack(void);
	bool Reload(void);
	bool Deploy(void);
	bool Holster(CBaseCombatWeapon* pSwitchingTo);
	void ItemPostFrame(void);
	void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
	Activity GetPrimaryAttackActivity(void);
	void AddViewKick(void);

	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

	virtual float GetFireRate(void) { return 0.4f; }
	virtual int GetMinBurst() { return 1; }
	virtual int GetMaxBurst() { return 1; }
	virtual float GetMinRestTime() { return 0.4f; }
	virtual float GetMaxRestTime() { return 0.6f; }

	const WeaponProficiencyInfo_t* GetProficiencyValues();

private:
	float m_flSoonestPrimaryAttack;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponSVT40, DT_WeaponSVT40)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_svt40, CWeaponSVT40);
PRECACHE_WEAPON_REGISTER(weapon_svt40);

BEGIN_DATADESC(CWeaponSVT40)
DEFINE_FIELD(m_flSoonestPrimaryAttack, FIELD_TIME),
END_DATADESC()

acttable_t CWeaponSVT40::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_AR2, true },
	{ ACT_RELOAD, ACT_RELOAD_SMG1, true },
	{ ACT_IDLE, ACT_IDLE_SMG1, true },
	{ ACT_IDLE_ANGRY, ACT_IDLE_ANGRY_SMG1, true },
	{ ACT_WALK, ACT_WALK_RIFLE, true },
	{ ACT_WALK_AIM, ACT_WALK_AIM_RIFLE, true },
	{ ACT_WALK_CROUCH, ACT_WALK_CROUCH_RIFLE, true },
	{ ACT_WALK_CROUCH_AIM, ACT_WALK_CROUCH_AIM_RIFLE, true },
	{ ACT_RUN, ACT_RUN_RIFLE, true },
	{ ACT_RUN_AIM, ACT_RUN_AIM_RIFLE, true },
	{ ACT_RUN_CROUCH, ACT_RUN_CROUCH_RIFLE, true },
	{ ACT_RUN_CROUCH_AIM, ACT_RUN_CROUCH_AIM_RIFLE, true },
	{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_AR2, false },
	{ ACT_COVER_LOW, ACT_COVER_SMG1_LOW, false },
	{ ACT_RANGE_AIM_LOW, ACT_RANGE_AIM_AR2_LOW, false },
	{ ACT_RANGE_ATTACK1_LOW, ACT_RANGE_ATTACK_SMG1_LOW, true },
	{ ACT_RELOAD_LOW, ACT_RELOAD_SMG1_LOW, false },
	{ ACT_GESTURE_RELOAD, ACT_GESTURE_RELOAD_SMG1, true },
};

IMPLEMENT_ACTTABLE(CWeaponSVT40);

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CWeaponSVT40::CWeaponSVT40(void)
{
	m_bReloadsSingly = false;
	m_bFiresUnderwater = false;
	m_flSoonestPrimaryAttack = 0;
	m_fMinRange1 = 65;
	m_fMaxRange1 = 2048;
	m_fMinRange2 = 65;
	m_fMaxRange2 = 1024;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSVT40::Precache(void)
{
	BaseClass::Precache();

	int ammoType = GetAmmoDef()->Index("SVT40");
	if (ammoType >= 0)
	{
		m_iPrimaryAmmoType = ammoType;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeaponSVT40::Deploy(void)
{
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeaponSVT40::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSVT40::SecondaryAttack(void)
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSVT40::PrimaryAttack(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		}
		return;
	}

	if (gpGlobals->curtime < m_flSoonestPrimaryAttack)
		return;

	m_flSoonestPrimaryAttack = gpGlobals->curtime + SVT40_FASTEST_REFIRE_TIME;
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();
	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1--;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	pPlayer->FireBullets(1, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, sk_plr_dmg_svt40.GetInt());
	pPlayer->SetMuzzleFlashTime(gpGlobals->curtime + 0.5f);
	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2f, GetOwner());
	AddViewKick();

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSVT40::AddViewKick(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	QAngle viewPunch;
	viewPunch.x = random->RandomFloat(-1.0f, -1.5f);
	viewPunch.y = random->RandomFloat(-0.4f, 0.4f);
	viewPunch.z = 0.0f;
	pPlayer->ViewPunch(viewPunch);
	DoMachineGunKick(pPlayer, 0.5f, 2.0f, 0.2f, 2.0f);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeaponSVT40::Reload(void)
{
	bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		WeaponSound(RELOAD);
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSVT40::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();
	if (m_bInReload)
		return;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (pOwner == NULL)
		return;

	if ((pOwner->m_nButtons & IN_ATTACK) == false && m_flSoonestPrimaryAttack < gpGlobals->curtime)
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSVT40::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_AR2:
	{
		Vector vecShootOrigin;
		Vector vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		CAI_BaseNPC* npc = pOperator->MyNPCPointer();
		if (npc == NULL)
			break;
		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);
		WeaponSound(SINGLE_NPC);
		CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2f, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());
		pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2, -1, -1, sk_npc_dmg_svt40.GetInt());
		pOperator->DoMuzzleFlash();
		m_iClip1 = m_iClip1 - 1;
	}
	break;
	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CWeaponSVT40::GetPrimaryAttackActivity(void)
{
	return ACT_VM_PRIMARYATTACK;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t* CWeaponSVT40::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0, 0.75 },
		{ 5.00, 0.75 },
		{ 3.0, 0.85 },
		{ 5.0 / 3.0, 0.75 },
		{ 1.00, 1.0 },
	};
	COMPILE_TIME_ASSERT(ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);
	return proficiencyTable;
}
