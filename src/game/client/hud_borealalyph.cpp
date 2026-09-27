//========= Copyright (c) Rebuild Team, 2026 ===================================//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "c_basehlplayer.h"
#include "c_basecombatweapon.h"
#include "weapon_selection.h"
#include "history_resource.h"
#include "input.h"
#include "view.h"
#include "ivrenderview.h"
#include "hud_element_helper.h"
#include "hud_borealalyph.h"
#include "in_buttons.h"
#include "ammodef.h"

#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/VGUI.h>
#include <vgui_controls/AnimationController.h>
#include "filesystem.h" 

#include "tier0/memdbgon.h"

using namespace vgui;

static void BAHudVisibilityChanged(IConVar* pVar, const char* pOldValue, float flOldValue)
{
	CHudBorealAlyph::UpdateDefaultHudElements();
}

ConVar bahud_enable("bahud_enable", "1", FCVAR_ARCHIVE, "Enable Boreal Alyph HUD", BAHudVisibilityChanged);
ConVar bahud_health("bahud_health", "1", FCVAR_ARCHIVE, "Enable health counter", BAHudVisibilityChanged);
ConVar bahud_armor("bahud_armor", "1", FCVAR_ARCHIVE, "Enable armor counter", BAHudVisibilityChanged);
ConVar bahud_hev("bahud_hev", "1", FCVAR_ARCHIVE, "Enable HEV power counter", BAHudVisibilityChanged);
ConVar bahud_ammo("bahud_ammo", "1", FCVAR_ARCHIVE, "Enable Ammo counter", BAHudVisibilityChanged);
ConVar bahud_crosshair("bahud_crosshair", "1", FCVAR_ARCHIVE, "Enable crosshair", BAHudVisibilityChanged);
ConVar bahud_wepselect("bahud_wepselect", "1", FCVAR_ARCHIVE, "Enable HUD weapon selection", BAHudVisibilityChanged);
ConVar bahud_alt_aux("bahud_altcaux", "1", FCVAR_ARCHIVE, "Alternative AUX counter");
ConVar bahud_always_draw_hev("bahud_always_draw_hev", "1", FCVAR_ARCHIVE, "Always draw HEV power icon");
ConVar bahud_alt_aux_hide("bahud_alt_aux_hide", "1", FCVAR_ARCHIVE, "Hide alt aux when it is full");
ConVar bahud_def_line_width("bahud_def_line_width", "65", FCVAR_ARCHIVE, "Default line width on HUD");
ConVar bahud_def_ammo_width("bahud_def_ammo_width", "90", FCVAR_ARCHIVE, "Default line width of ammo HUD");
ConVar bahud_depth_size("bahud_depth_size", "12", FCVAR_ARCHIVE, "Depth/tilt size on HUD bars and text");
ConVar bahud_crosshair_thickness("bahud_crosshair_thickness", "0.2", FCVAR_ARCHIVE);
ConVar bahud_crosshair_outline("bahud_crosshair_outline", "0.1", FCVAR_ARCHIVE);
ConVar bahud_low_health("bahud_low_health", "25", FCVAR_ARCHIVE, "Health value when HUD starts to blink");
ConVar bahud_low_aux("bahud_low_aux", "25", FCVAR_ARCHIVE, "HEV power value when HUD starts to blink");

extern ConVar cl_crosshair_world;
extern ConVar cl_crosshair_trace_dist;
extern ConVar crosshair;

static const Color BAHUD_COLOR_HEALTH(214, 183, 52, 160);
static const Color BAHUD_COLOR_CRIT_HEALTH(214, 183, 52, 255);
static const Color BAHUD_COLOR_ARMOR(214, 183, 52, 160);
static const Color BAHUD_COLOR_AMMO(214, 183, 52, 160);
static const Color BAHUD_COLOR_CROSSHAIR(214, 183, 52, 255);
static const Color BAHUD_COLOR_FLASH(255, 255, 255, 255);
static const Color BAHUD_COLOR_DAMAGE(214, 76, 52, 255);

static const Color BAHUD_COLOR_SELECT_ACTIVE(214, 183, 52, 160);
static const Color BAHUD_COLOR_SELECT_ACTIVE2(214, 183, 52, 160);
static const Color BAHUD_COLOR_SELECT_EMPTY(214, 183, 52, 160);
static const Color BAHUD_COLOR_SELECT_BAR(214, 183, 52, 200);
static const Color BAHUD_COLOR_SELECT_INACTIVE(214, 183, 52, 40);
static const Color BAHUD_COLOR_SELECT_INACTIVE2(214, 183, 52, 120);

#define BAHUD_FONT_DIN "Alte DIN 1451 Mittelschrift"
#define BAHUD_FONT_MONO "Roboto Mono"
#define BAHUD_FONT_HL2 "HalfLife2"

int ScreenTransform(const Vector& point, Vector& screen);

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline float ScreenScaleF(float val)
{
	return val * ((float)ScreenHeight() / 480.0f);
}

static inline int ScreenScale(float val)
{
	return (int)(ScreenScaleF(val) + 0.5f);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static HFont CreateBAHudFont(const char* pszFamily, float flLuaSize, int iWeight)
{
	int iTall = (int)floorf(ScreenScaleF(flLuaSize * 0.8f) + 0.5f);
	iTall = MAX(iTall, 4);

	HFont hFont = surface()->CreateFont();
	surface()->SetFontGlyphSet(hFont, pszFamily, iTall, iWeight, 0, 0, ISurface::FONTFLAG_ANTIALIAS, ISurface::FONTFLAG_CUSTOM);
	return hFont;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void RegisterBAHudFontFiles(void)
{
	static bool s_bRegistered = false;
	if (s_bRegistered)
		return;

	s_bRegistered = true;
	surface()->AddCustomFontFile(BAHUD_FONT_DIN, "resource/fonts/alte_din_1451_mittelschrift.ttf");
	surface()->AddCustomFontFile(BAHUD_FONT_MONO, "resource/fonts/RobotoMono-Regular.ttf");
	surface()->AddCustomFontFile(BAHUD_FONT_HL2, "resource/HALFLIFE2.ttf");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline Color ModifyAlpha(const Color& col, int alpha)
{
	return Color(col.r(), col.g(), col.b(), clamp(alpha, 0, 255));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline Color MultColor(const Color& col, int mult255)
{
	return Color(
		clamp((col.r() * mult255) / 255, 0, 255),
		clamp((col.g() * mult255) / 255, 0, 255),
		clamp((col.b() * mult255) / 255, 0, 255),
		col.a()
	);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline Color LerpBAHudColor(const Color& from, const Color& to, float t)
{
	t = clamp(t, 0.0f, 1.0f);

	return Color(
		(int)(from.r() + (to.r() - from.r()) * t),
		(int)(from.g() + (to.g() - from.g()) * t),
		(int)(from.b() + (to.b() - from.b()) * t),
		(int)(from.a() + (to.a() - from.a()) * t)
	);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline float BAHudBlink(float flSpeed)
{
	return 0.5f + 0.5f * sin(gpGlobals->realtime * flSpeed);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawFilledRect(int x, int y, int w, int h, const Color& col)
{
	surface()->DrawSetColor(col);
	surface()->DrawFilledRect(x, y, x + w, y + h);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawOutlinedFilledRect(int x, int y, int w, int h, int outline, const Color& outlineCol, const Color& fillCol)
{
	DrawFilledRect(x - outline, y - outline, w + outline * 2, h + outline * 2, outlineCol);
	DrawFilledRect(x, y, w, h, fillCol);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline int ComputeTiltOffset(float t, int tilt, bool bevelLeft)
{
	bool tiltRightUp = !bevelLeft;
	float dir = tiltRightUp ? -1.0f : 1.0f;
	float centered = t - 0.5f;
	return (int)floor(centered * tilt * dir + 0.5f);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static int GetDepthOffset(int px, int originX, int totalW, bool bevelLeft)
{
	int tilt = ScreenScale(bahud_depth_size.GetFloat());
	if (tilt <= 1 || totalW <= 1)
		return 0;

	float t = clamp((float)(px - originX) / (float)(totalW - 1), 0.0f, 1.0f);
	return ComputeTiltOffset(t, tilt, bevelLeft);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static int GetWhiteTextureId()
{
	static int textureId = -1;
	if (textureId == -1)
	{
		textureId = surface()->CreateNewTextureID(true);
		byte whitePixel[4] = { 255, 255, 255, 255 };
		surface()->DrawSetTextureRGBA(textureId, whitePixel, 1, 1, 1, false);
	}
	return textureId;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawFilledParallelogram(int x, int y, int w, int h, int offsetLeft, int offsetRight, const Color& col)
{
	Vertex_t verts[4];
	verts[0].Init(Vector2D(x, y + offsetLeft));
	verts[1].Init(Vector2D(x + w, y + offsetRight));
	verts[2].Init(Vector2D(x + w, y + offsetRight + h));
	verts[3].Init(Vector2D(x, y + offsetLeft + h));

	surface()->DrawSetColor(col);
	surface()->DrawSetTexture(GetWhiteTextureId());
	surface()->DrawTexturedPolygon(4, verts);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawLine2D(float x1, float y1, float x2, float y2, float flThickness, const Color& col)
{
	float dx = x2 - x1;
	float dy = y2 - y1;
	float flLen = sqrt(dx * dx + dy * dy);
	if (flLen < 0.001f)
		return;

	float nx = -dy / flLen * flThickness * 0.5f;
	float ny = dx / flLen * flThickness * 0.5f;

	Vertex_t verts[4];
	verts[0].Init(Vector2D(x1 + nx, y1 + ny));
	verts[1].Init(Vector2D(x2 + nx, y2 + ny));
	verts[2].Init(Vector2D(x2 - nx, y2 - ny));
	verts[3].Init(Vector2D(x1 - nx, y1 - ny));

	surface()->DrawSetColor(col);
	surface()->DrawSetTexture(GetWhiteTextureId());
	surface()->DrawTexturedPolygon(4, verts);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawHudArrow(float x, float y, float dx, float dy, float flInner, float flOuter, float flWidth, const Color& col)
{
	float tx = -dy;
	float ty = dx;

	Vertex_t verts[4];
	verts[0].Init(Vector2D(x + dx * flInner + tx * flWidth, y + dy * flInner + ty * flWidth));
	verts[1].Init(Vector2D(x + dx * flInner - tx * flWidth, y + dy * flInner - ty * flWidth));
	verts[2].Init(Vector2D(x + dx * flOuter - tx * flWidth * 0.25f, y + dy * flOuter - ty * flWidth * 0.25f));
	verts[3].Init(Vector2D(x + dx * flOuter + tx * flWidth * 0.25f, y + dy * flOuter + ty * flWidth * 0.25f));

	surface()->DrawSetColor(col);
	surface()->DrawSetTexture(GetWhiteTextureId());
	surface()->DrawTexturedPolygon(4, verts);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawSlotHighlight(int x, int y, int w, int h, int thickness, const Color& col)
{
	DrawFilledRect(x - thickness, y - thickness, w + thickness * 2, thickness, col);
	DrawFilledRect(x - thickness, y + h, w + thickness * 2, thickness, col);
	DrawFilledRect(x - thickness, y, thickness, h, col);
	DrawFilledRect(x + w, y, thickness, h, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawFilledRectDepth(int x, int y, int totalW, int h, int fillW, const Color& col, bool bevelLeft)
{
	if (totalW <= 0 || h <= 0)
		return;

	fillW = clamp(fillW, 0, totalW);
	if (fillW <= 0)
		return;

	int tilt = MIN(ScreenScale(bahud_depth_size.GetFloat()), h * 4);

	if (tilt <= 1)
	{
		DrawFilledRect(x, y, fillW, h, col);
		return;
	}

	int offsetLeft = GetDepthOffset(x, x, totalW, bevelLeft);
	int offsetRight = GetDepthOffset(x + fillW, x, totalW, bevelLeft);

	DrawFilledParallelogram(x, y, fillW, h, offsetLeft, offsetRight, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawFilledRectDepthPart(int segX, int segW, int y, int h,
	const Color& col,
	int originX, int totalW, bool bevelLeft)
{
	if (segW <= 0 || h <= 0)
		return;

	int tilt = MIN(ScreenScale(bahud_depth_size.GetFloat()), h * 4);
	if (tilt <= 1 || totalW <= 1)
	{
		DrawFilledRect(segX, y, segW, h, col);
		return;
	}

	int offsetLeft = GetDepthOffset(segX, originX, totalW, bevelLeft);
	int offsetRight = GetDepthOffset(segX + segW, originX, totalW, bevelLeft);

	DrawFilledParallelogram(segX, y, segW, h, offsetLeft, offsetRight, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawTextString(HFont font, int x, int y, const wchar_t* text, const Color& col)
{
	surface()->DrawSetTextFont(font);
	surface()->DrawSetTextColor(col);
	surface()->DrawSetTextPos(x, y);
	surface()->DrawUnicodeString(text);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawTextStringA(HFont font, int x, int y, const char* text, const Color& col)
{
	wchar_t wsz[256];
	g_pVGuiLocalize->ConvertANSIToUnicode(text, wsz, sizeof(wsz));
	DrawTextString(font, x, y, wsz, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawTextStringDepth(HFont font, int x, int y, const wchar_t* text, const Color& col, int totalW, int originX, bool bevelLeft)
{
	if (!text || !text[0])
		return;

	int tilt = ScreenScale(bahud_depth_size.GetFloat());

	if (tilt <= 1 || totalW <= 1)
	{
		DrawTextString(font, x, y, text, col);
		return;
	}

	surface()->DrawSetTextFont(font);
	surface()->DrawSetTextColor(col);

	int curX = x;
	for (int i = 0; text[i] != L'\0'; ++i)
	{
		int charWide = surface()->GetCharacterWidth(font, text[i]);
		int offset = GetDepthOffset(curX + charWide / 2, originX, totalW, bevelLeft);

		surface()->DrawSetTextPos(curX, y + offset);
		surface()->DrawUnicodeChar(text[i]);

		curX += charWide;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawTextStringADepth(HFont font, int x, int y, const char* text, const Color& col, int totalW, int originX, bool bevelLeft)
{
	wchar_t wsz[256];
	g_pVGuiLocalize->ConvertANSIToUnicode(text, wsz, sizeof(wsz));
	DrawTextStringDepth(font, x, y, wsz, col, totalW, originX, bevelLeft);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void GetTextSize(HFont font, const wchar_t* text, int& w, int& h)
{
	surface()->GetTextSize(font, text, w, h);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawCircleHollow(int x, int y, int radius, int thickness, const Color& col)
{
	surface()->DrawSetColor(col);
	for (int t = 0; t < thickness; ++t)
	{
		surface()->DrawOutlinedCircle(x, y, radius - t, 48);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static inline void DrawFilledCircle(int x, int y, int radius, const Color& col)
{
	surface()->DrawSetColor(col);
	for (int r = 0; r <= radius; ++r)
	{
		surface()->DrawOutlinedCircle(x, y, r, 24);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
struct BAIconFont_t
{
	char szFamily[64];
	int iTall;
	int iFlags;
	HFont hFont;
};

static CUtlVector<BAIconFont_t> g_BAIconFonts;

static HFont GetScaledIconFont(const char* pszFamily, int iTall, int iFlags)
{
	for (int i = 0; i < g_BAIconFonts.Count(); ++i)
	{
		BAIconFont_t& entry = g_BAIconFonts[i];
		if (entry.iTall == iTall && entry.iFlags == iFlags && Q_stricmp(entry.szFamily, pszFamily) == 0)
			return entry.hFont;
	}

	BAIconFont_t entry;
	Q_strncpy(entry.szFamily, pszFamily, sizeof(entry.szFamily));
	entry.iTall = iTall;
	entry.iFlags = iFlags;
	entry.hFont = surface()->CreateFont();
	surface()->SetFontGlyphSet(entry.hFont, pszFamily, iTall, 0, 0, 0, iFlags);
	g_BAIconFonts.AddToTail(entry);

	return entry.hFont;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void DrawWeaponIcon(const CHudTexture* icon, int x, int y, int w, int h, const Color& col)
{
	if (!icon || w <= 0 || h <= 0)
		return;

	if (!icon->bRenderUsingFont)
	{
		int iw = icon->Width();
		int ih = icon->Height();
		if (iw <= 0 || ih <= 0)
			return;

		float scale = MIN((float)w / (float)iw, (float)h / (float)ih);
		int dw = MAX(1, (int)(iw * scale + 0.5f));
		int dh = MAX(1, (int)(ih * scale + 0.5f));
		icon->DrawSelf(x + (w - dw) / 2, y + (h - dh) / 2, dw, dh, col);
		return;
	}

	int fontTall = surface()->GetFontTall(icon->hFont);
	int charWide = surface()->GetCharacterWidth(icon->hFont, icon->cCharacterInFont);
	if (fontTall <= 0 || charWide <= 0)
		return;

	int tall = h;
	if (charWide * h > w * fontTall)
		tall = w * fontTall / charWide;
	tall = MAX(tall, 4);

	const char* pszFamily = surface()->GetFontName(icon->hFont);
	if (!pszFamily || !pszFamily[0])
		pszFamily = BAHUD_FONT_HL2;

	int flags = ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_CUSTOM;
	if (surface()->IsFontAdditive(icon->hFont))
		flags |= ISurface::FONTFLAG_ADDITIVE;

	HFont hFont = GetScaledIconFont(pszFamily, tall, flags);
	int dw = surface()->GetCharacterWidth(hFont, icon->cCharacterInFont);
	int dh = surface()->GetFontTall(hFont);

	surface()->DrawSetTextFont(hFont);
	surface()->DrawSetTextColor(col);
	surface()->DrawSetTextPos(x + (w - dw) / 2, y + (h - dh) / 2);
	surface()->DrawUnicodeChar(icon->cCharacterInFont);
}

DECLARE_HUDELEMENT(CHudBorealAlyph);
DECLARE_HUD_MESSAGE(CHudBorealAlyph, Damage);
DECLARE_HUD_MESSAGE(CHudBorealAlyph, Battery);

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CHudBorealAlyph::CHudBorealAlyph(const char* pElementName) :
	CHudElement(pElementName), BaseClass(NULL, "HudBorealAlyph")
{
	Panel* pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);

	m_iHealth = -1;
	m_iArmor = -1;
	m_iNewArmor = 0;
	m_flSuitPower = 100.0f;

	m_flLastFullSuit = 0;
	m_flLastFullSuitFade = 0;
	m_flLastNonFullSuit = 0;
	m_flLastNonFullSuitFade = 0;
	m_bLastNonFullSuitStatus = false;

	m_flLastSuitAltLabel = 0;
	m_szLastSuitAltLabelLast[0] = '\0';
	m_flLastSuitAltLabelLastDec = 0;
	m_flLastSuitAltLabelLastDecT = 100.0f;

	m_hFontCounter = INVALID_FONT;
	m_hFontCounterStored = INVALID_FONT;
	m_hFontHEVCounter = INVALID_FONT;
	m_hFontCounterText = INVALID_FONT;
	m_hFontIcon = INVALID_FONT;
	m_hFontHEVIcon = INVALID_FONT;
	m_hFontFlashlightIcon = INVALID_FONT;

	m_bHavePrevCrossPos = false;
	m_pPrevCrossWeapon = NULL;
	m_iPrevCrossClip = 0;
	m_flCrossBloom = 0.0f;

	m_flHudFade = 1.0f;

	m_iPrevHealth = -1;
	m_iPrevArmor = -1;
	m_flPrevSuitPower = -1.0f;

	m_flHealthPulse = 0.0f;
	m_flHealthHeal = 0.0f;
	m_flArmorPulse = 0.0f;
	m_flArmorHeal = 0.0f;
	m_flAmmoPulse = 0.0f;
	m_flAmmoReload = 0.0f;
	m_flHEVPulse = 0.0f;
	m_flFlashlightPulse = 0.0f;
	m_bLastFlashlightOn = false;

	m_flDamageShake = 0.0f;
	m_vecDamageFrom = vec3_origin;
	m_flDamageDirTime = 0.0f;
	m_flDamageDirFade = 0.0f;
	m_flDamageDirAmount = 0.0f;
	m_bDamageDirValid = false;

	m_iPrevClip1 = -1;
	m_iPrevClip2 = -1;
	m_iPrevAmmo1 = -1;
	m_iPrevAmmo2 = -1;
	m_flWeaponSwitchTime = 0.0f;
	m_flWeaponSwitchFade = 0.0f;

	m_bPrevAttackDown = false;
	m_flHitMarker = 0.0f;
}

CHudBorealAlyph::~CHudBorealAlyph()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::Init(void)
{
	HOOK_HUD_MESSAGE(CHudBorealAlyph, Damage);
	HOOK_HUD_MESSAGE(CHudBorealAlyph, Battery);
	Reset();
}

void CHudBorealAlyph::VidInit(void)
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::Reset(void)
{
	m_iHealth = -1;
	m_iArmor = -1;
	m_iNewArmor = 0;
	m_flSuitPower = 100.0f;

	m_flLastFullSuit = 0;
	m_flLastFullSuitFade = 0;
	m_flLastNonFullSuit = 0;
	m_flLastNonFullSuitFade = 0;
	m_bLastNonFullSuitStatus = false;

	m_flLastSuitAltLabel = 0;
	m_szLastSuitAltLabelLast[0] = '\0';
	m_flLastSuitAltLabelLastDec = 0;
	m_flLastSuitAltLabelLastDecT = 100.0f;

	m_bHavePrevCrossPos = false;
	m_pPrevCrossWeapon = NULL;
	m_iPrevCrossClip = 0;
	m_flCrossBloom = 0.0f;

	m_flHudFade = 0.0f;

	m_iPrevHealth = -1;
	m_iPrevArmor = -1;
	m_flPrevSuitPower = -1.0f;

	m_flHealthPulse = 0.0f;
	m_flHealthHeal = 0.0f;
	m_flArmorPulse = 0.0f;
	m_flArmorHeal = 0.0f;
	m_flAmmoPulse = 0.0f;
	m_flAmmoReload = 0.0f;
	m_flHEVPulse = 0.0f;
	m_flFlashlightPulse = 0.0f;
	m_bLastFlashlightOn = false;

	m_flDamageShake = 0.0f;
	m_vecDamageFrom = vec3_origin;
	m_flDamageDirTime = 0.0f;
	m_flDamageDirFade = 0.0f;
	m_flDamageDirAmount = 0.0f;
	m_bDamageDirValid = false;

	m_hPrevAmmoWeapon = NULL;
	m_iPrevClip1 = -1;
	m_iPrevClip2 = -1;
	m_iPrevAmmo1 = -1;
	m_iPrevAmmo2 = -1;
	m_flWeaponSwitchTime = 0.0f;
	m_flWeaponSwitchFade = 0.0f;

	m_bPrevAttackDown = false;
	m_flHitMarker = 0.0f;

	UpdateDefaultHudElements();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
struct BAStockHudEntry_t
{
	const char* szElement;
	ConVar* pGate;
	int iHiddenBits;
	bool bStored;
	CHudElement* pCached;
};

static BAStockHudEntry_t g_BAStockHud[] =
{
	{ "CHudHealth",          &bahud_health,    0, false, NULL },
	{ "CHudBattery",         &bahud_armor,     0, false, NULL },
	{ "CHudSuitPower",       &bahud_hev,       0, false, NULL },
	{ "CHudFlashlight",      &bahud_hev,       0, false, NULL },
	{ "CHudAmmo",            &bahud_ammo,      0, false, NULL },
	{ "CHudSecondaryAmmo",   &bahud_ammo,      0, false, NULL },
	{ "CHudCrosshair",       &bahud_crosshair, 0, false, NULL },
	{ "CHudWeaponSelection", &bahud_wepselect, 0, false, NULL },
};

static CHudElement* BAGetStockHudElement(BAStockHudEntry_t& entry)
{
	if (!entry.pCached)
	{
		entry.pCached = gHUD.FindElement(entry.szElement);
	}
	return entry.pCached;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void BAHideStockHudEntry(BAStockHudEntry_t& entry)
{
	CHudElement* pElem = BAGetStockHudElement(entry);
	if (!pElem)
		return;

	if (!entry.bStored)
	{
		entry.iHiddenBits = pElem->GetHiddenBits();
		entry.bStored = true;
	}

	pElem->SetHiddenBits(HIDEHUD_ALL);
	Panel* pPanel = dynamic_cast<Panel*>(pElem);
	if (pPanel)
	{
		pPanel->SetVisible(false);
		pPanel->SetPaintEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void BARestoreStockHudEntry(BAStockHudEntry_t& entry)
{
	if (!entry.bStored)
		return;

	entry.bStored = false;

	CHudElement* pElem = BAGetStockHudElement(entry);
	if (!pElem)
		return;

	pElem->SetHiddenBits(entry.iHiddenBits);
	Panel* pPanel = dynamic_cast<Panel*>(pElem);
	if (pPanel)
	{
		pPanel->SetPaintEnabled(true);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::UpdateDefaultHudElements(void)
{
	bool bEnabled = bahud_enable.GetBool();

	for (int i = 0; i < (int)ARRAYSIZE(g_BAStockHud); ++i)
	{
		BAStockHudEntry_t& entry = g_BAStockHud[i];
		bool bHide = bEnabled && (!entry.pGate || entry.pGate->GetBool());

		if (bHide)
			BAHideStockHudEntry(entry);
		else
			BARestoreStockHudEntry(entry);
	}

	if (!bEnabled || !bahud_wepselect.GetBool())
	{
		CHudElement* pElem = gHUD.FindElement("CHudBAWeaponSelection");
		CBaseHudWeaponSelection* pBA = pElem ? dynamic_cast<CBaseHudWeaponSelection*>(pElem) : NULL;
		if (pBA)
			pBA->CancelWeaponSelection();
	}

	if (!bEnabled)
		CHudBAWeaponSelection::YieldInstanceToStock();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled(false);
	SetSize(ScreenWidth(), ScreenHeight());
	SetPos(0, 0);

	RegisterBAHudFontFiles();

	m_hFontCounter = CreateBAHudFont(BAHUD_FONT_DIN, 40.0f, 500);
	m_hFontCounterStored = CreateBAHudFont(BAHUD_FONT_DIN, 28.0f, 500);
	m_hFontHEVCounter = CreateBAHudFont(BAHUD_FONT_HL2, 32.0f, 400);
	m_hFontCounterText = CreateBAHudFont(BAHUD_FONT_MONO, 15.0f, 500);
	m_hFontIcon = CreateBAHudFont(BAHUD_FONT_HL2, 27.0f, 500);
	m_hFontHEVIcon = CreateBAHudFont(BAHUD_FONT_HL2, 37.0f, 400);
	m_hFontFlashlightIcon = CreateBAHudFont(BAHUD_FONT_HL2, 54.0f, 400);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHudBorealAlyph::ShouldDraw(void)
{
	if (!bahud_enable.GetBool())
		return false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer || !pPlayer->IsAlive())
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::OnThink(void)
{
	UpdateDefaultHudElements();

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	m_iHealth = MAX(pPlayer->GetHealth(), 0);
	m_iArmor = m_iNewArmor;

	C_BaseHLPlayer* pHLPlayer = dynamic_cast<C_BaseHLPlayer*>(pPlayer);
	if (pHLPlayer)
	{
		m_flSuitPower = pHLPlayer->m_HL2Local.m_flSuitPower;
	}

	UpdatePlayerFeedback(pPlayer);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::Paint(void)
{
	if (!ShouldDraw())
		return;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	int healthX = (int)(0.04f * ScreenWidth());
	int healthY = (int)(0.87f * ScreenHeight());
	PaintHealth(pPlayer, healthX, healthY);

	int ammoX = (int)(0.96f * ScreenWidth());
	int ammoY = (int)(0.87f * ScreenHeight());
	PaintAmmo(pPlayer, ammoX, ammoY);

	if (bahud_ammo.GetBool())
	{
		PaintWeaponName(pPlayer->GetActiveWeapon(), ammoX, ammoY);
	}

	PaintDamageDirection();

	if (bahud_crosshair.GetBool())
	{
		PaintCrosshair(pPlayer);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintHealth(C_BasePlayer* pPlayer, int x, int y)
{
	int barWidthArmor = 0;

	if (m_flDamageShake > 0.0f)
	{
		x += (int)(sin(gpGlobals->realtime * 62.0f) * m_flDamageShake * ScreenScaleF(5.0f));
		y += (int)(cos(gpGlobals->realtime * 47.0f) * m_flDamageShake * ScreenScaleF(3.0f));
	}

	if (bahud_health.GetBool())
	{
		int iHealth = MAX(pPlayer->GetHealth(), 0);
		wchar_t wszPlus[] = L"+";
		int w = 0, h = 0;
		GetTextSize(m_hFontIcon, wszPlus, w, h);

		int padding = ScreenScale(4.0f);
		int barHeight = ScreenScale(4.0f);

		float healthFillage = clamp((float)iHealth / 100.0f, 0.0f, 1.0f);
		Color col = (healthFillage > 0.3f) ? BAHUD_COLOR_HEALTH : BAHUD_COLOR_CRIT_HEALTH;

		col = LerpBAHudColor(col, BAHUD_COLOR_FLASH, MAX(m_flHealthPulse, m_flHealthHeal) * 0.7f);

		if (iHealth > 0 && iHealth <= bahud_low_health.GetInt())
		{
			col = ModifyAlpha(col, (int)(col.a() * (0.4f + 0.6f * BAHudBlink(5.5f))));
		}

		col = ModifyAlpha(col, (int)(col.a() * m_flHudFade));

		wchar_t* pwszText = g_pVGuiLocalize->Find("#Valve_Hud_HEALTH");
		const wchar_t* wszHealthText = pwszText ? pwszText : L"HEALTH";

		int w2 = 0, h2 = 0;
		GetTextSize(m_hFontCounterText, wszHealthText, w2, h2);

		wchar_t wszHealthVal[16];
		V_snwprintf(wszHealthVal, ARRAYSIZE(wszHealthVal), L"%d", iHealth);

		int wVal = 0, hVal = 0;
		GetTextSize(m_hFontCounter, wszHealthVal, wVal, hVal);

		int barWidth = MAX(ScreenScale(bahud_def_line_width.GetFloat()), wVal + padding * 2 + ScreenScale(23.0f) + w2);

		DrawTextStringDepth(m_hFontIcon, x, y + h - ScreenScale(18.0f), wszPlus, col, barWidth, x, false);
		DrawTextStringDepth(m_hFontCounterText, x + padding + w, y + ScreenScale(25.0f) - h2, wszHealthText, col, barWidth, x, false);
		DrawTextStringDepth(m_hFontCounter, x + padding + w + w2 + ScreenScale(17.0f), y - ScreenScale(4.0f), wszHealthVal, col, barWidth, x, false);

		h = h + ScreenScale(3.0f);

		DrawFilledRectDepth(x, y + h + padding, barWidth, barHeight, barWidth, MultColor(col, 50), false);
		DrawFilledRectDepth(x, y + h + padding, barWidth, barHeight, (int)(barWidth * healthFillage), col, false);

		if (bahud_armor.GetBool() && m_iArmor > 0)
		{
			barWidthArmor = PaintArmor(pPlayer, x + barWidth + ScreenScale(14.0f), y) + ScreenScale(14.0f);
		}

		if (bahud_hev.GetBool())
		{
			int wW = 0, hW = 0;
			GetTextSize(m_hFontHEVCounter, L"W", wW, hW);
			PaintLimitedHEV(pPlayer, x, y - ScreenScale(14.0f) - hW);
			PaintLimitedFlashlight(pPlayer, x + barWidth + ScreenScale(14.0f) + barWidthArmor, y);
		}
	}
	else if (bahud_armor.GetBool())
	{
		if (m_iArmor > 0)
		{
			barWidthArmor = PaintArmor(pPlayer, x, y) + ScreenScale(14.0f);
		}

		if (bahud_hev.GetBool())
		{
			int wW = 0, hW = 0;
			GetTextSize(m_hFontHEVCounter, L"W", wW, hW);
			PaintLimitedHEV(pPlayer, x, y - ScreenScale(14.0f) - hW);
			PaintLimitedFlashlight(pPlayer, x + barWidthArmor, y);
		}
	}
	else if (bahud_hev.GetBool())
	{
		int wW = 0, hW = 0;
		GetTextSize(m_hFontHEVCounter, L"W", wW, hW);
		PaintLimitedHEV(pPlayer, x, y - ScreenScale(14.0f) - hW);
		PaintLimitedFlashlight(pPlayer, x, y);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHudBorealAlyph::PaintArmor(C_BasePlayer* pPlayer, int x, int y)
{
	wchar_t wszStar[] = L"*";
	int w = 0, h = 0;
	GetTextSize(m_hFontIcon, wszStar, w, h);

	int padding = ScreenScale(4.0f);
	int barHeight = ScreenScale(4.0f);
	Color col = BAHUD_COLOR_ARMOR;

	col = LerpBAHudColor(col, BAHUD_COLOR_FLASH, MAX(m_flArmorPulse, m_flArmorHeal) * 0.8f);
	col = ModifyAlpha(col, (int)(col.a() * m_flHudFade));

	wchar_t* pwszText = g_pVGuiLocalize->Find("#Valve_Hud_SUIT");
	const wchar_t* wszArmorText = pwszText ? pwszText : L"ARMOR";

	int w2 = 0, h2 = 0;
	GetTextSize(m_hFontCounterText, wszArmorText, w2, h2);

	wchar_t wszArmorVal[16];
	V_snwprintf(wszArmorVal, ARRAYSIZE(wszArmorVal), L"%d", m_iArmor);

	int wVal = 0, hVal = 0;
	GetTextSize(m_hFontCounter, wszArmorVal, wVal, hVal);

	int barWidth = MAX(ScreenScale(bahud_def_line_width.GetFloat()), wVal + padding * 2 + w2 + ScreenScale(23.0f));

	DrawTextStringDepth(m_hFontIcon, x, y + h - ScreenScale(16.0f), wszStar, col, barWidth, x, false);
	DrawTextStringDepth(m_hFontCounterText, x + padding + w, y + ScreenScale(25.0f) - h2, wszArmorText, col, barWidth, x, false);
	DrawTextStringDepth(m_hFontCounter, x + padding + w + w2 + ScreenScale(17.0f), y - ScreenScale(4.0f), wszArmorVal, col, barWidth, x, false);

	h = h + ScreenScale(3.0f);

	float armorFillage = clamp((float)m_iArmor / 100.0f, 0.0f, 1.0f);
	DrawFilledRectDepth(x, y + h + padding, barWidth, barHeight, barWidth, MultColor(col, 50), false);
	DrawFilledRectDepth(x, y + h + padding, barWidth, barHeight, (int)(barWidth * armorFillage), col, false);

	return barWidth;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintLimitedHEV(C_BasePlayer* pPlayer, int x, int y)
{
	float suit = floor(m_flSuitPower);
	if (suit < 0)
		return;

	float alpha = 1.0f;
	bool alt = bahud_alt_aux.GetBool();
	bool hide = !bahud_always_draw_hev.GetBool();

	if (alt)
	{
		hide = bahud_alt_aux_hide.GetBool();
	}

	float flTime = gpGlobals->realtime;

	if (hide)
	{
		if (suit >= 100.0f)
		{
			if (m_flLastFullSuitFade < flTime && !m_bLastNonFullSuitStatus)
				return;

			if (m_bLastNonFullSuitStatus)
			{
				m_flLastFullSuit = flTime + 0.5f;
				m_flLastFullSuitFade = flTime + 0.9f;
				m_bLastNonFullSuitStatus = false;
			}

			if (m_flLastFullSuitFade > m_flLastFullSuit)
			{
				float prog = clamp((flTime - m_flLastFullSuit) / (m_flLastFullSuitFade - m_flLastFullSuit), 0.0f, 1.0f);
				alpha = 1.0f - prog;
			}
			else
			{
				alpha = 0.0f;
			}
		}
		else
		{
			if (!m_bLastNonFullSuitStatus)
			{
				m_flLastNonFullSuit = flTime;
				m_flLastNonFullSuitFade = flTime + 0.4f;
				m_bLastNonFullSuitStatus = true;
			}

			if (m_flLastNonFullSuitFade > m_flLastNonFullSuit)
			{
				alpha = clamp((flTime - m_flLastNonFullSuit) / (m_flLastNonFullSuitFade - m_flLastNonFullSuit), 0.0f, 1.0f);
			}
			else
			{
				alpha = 1.0f;
			}
		}
	}

	int padding = ScreenScale(4.0f);
	Color col = ModifyAlpha(BAHUD_COLOR_ARMOR, (int)(BAHUD_COLOR_ARMOR.a() * alpha));

	col = LerpBAHudColor(col, BAHUD_COLOR_FLASH, m_flHEVPulse * 0.6f);

	if (suit <= bahud_low_aux.GetFloat())
	{
		col = ModifyAlpha(col, (int)(col.a() * (0.4f + 0.6f * BAHudBlink(6.5f))));
	}

	col = ModifyAlpha(col, (int)(col.a() * m_flHudFade));

	if (alt)
	{
		wchar_t wszD[] = L"D";
		int wD = 0, hD = 0;
		GetTextSize(m_hFontHEVIcon, wszD, wD, hD);

		int auxBars = 10;
		float auxBarW = ScreenScaleF(7.7f);
		float auxBarPad = ScreenScaleF(2.5f);
		int auxBarH = ScreenScale(4.0f);
		int h = ScreenScale(14.0f);

		Color colI = MultColor(col, 140);
		float step = 100.0f / (float)auxBars;
		float w = 0.0f;

		int barsX = x + wD + padding;
		int totalBarsW = (int)ceil(auxBars * auxBarW + (auxBars - 1) * auxBarPad);
		int totalW = wD + padding + totalBarsW;

		DrawTextStringDepth(m_hFontHEVIcon, x, (int)(y - hD * 0.15f), wszD, col, totalW, x, false);

		wchar_t wszSuitVal[16];
		V_snwprintf(wszSuitVal, ARRAYSIZE(wszSuitVal), L"%d", (int)clamp(suit, 0.0f, 100.0f));

		const char* szText2 = "";
		C_BaseHLPlayer* pHLPlayer = dynamic_cast<C_BaseHLPlayer*>(pPlayer);

		if (suit < 100.0f)
		{
			if (pPlayer->GetWaterLevel() >= 3)
			{
				szText2 = "O2";
			}
			else if ((pHLPlayer && pHLPlayer->IsSprinting()) || (pPlayer->m_nButtons & IN_SPEED))
			{
				szText2 = "SPRINT";
			}
		}

		if (m_flLastSuitAltLabelLastDecT > suit)
		{
			m_flLastSuitAltLabelLastDec = flTime + 1.0f;
		}
		else if (m_flLastSuitAltLabelLastDec < flTime)
		{
			szText2 = "";
		}

		m_flLastSuitAltLabelLastDecT = suit;

		if (szText2[0] == '\0')
		{
			m_flLastSuitAltLabel = Approach(0.0f, m_flLastSuitAltLabel, gpGlobals->frametime * 22.0f);
		}
		else
		{
			Q_strncpy(m_szLastSuitAltLabelLast, szText2, sizeof(m_szLastSuitAltLabelLast));
			m_flLastSuitAltLabel = Approach(1.0f, m_flLastSuitAltLabel, gpGlobals->frametime * 22.0f);
		}

		Color textCol = ModifyAlpha(col, (int)(col.a() * m_flLastSuitAltLabel));
		DrawTextStringADepth(m_hFontCounterText, barsX - ScreenScale(1.0f), y - auxBarH, m_szLastSuitAltLabelLast, textCol, totalW, x, false);

		for (int i = 1; i <= auxBars; ++i)
		{
			Color barCol = (suit >= (i * step)) ? col : colI;
			DrawFilledRectDepthPart(barsX + (int)w, (int)auxBarW, y + h, auxBarH, barCol, x, totalW, false);
			w += auxBarW + auxBarPad;
		}

		int w2 = 0, h2 = 0;
		GetTextSize(m_hFontHEVCounter, wszSuitVal, w2, h2);
		DrawTextStringDepth(m_hFontHEVCounter, barsX + (int)w - w2 - (int)auxBarPad, y - h2 - auxBarH + ScreenScale(16.0f), wszSuitVal, col, totalW, x, false);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintLimitedFlashlight(C_BasePlayer* pPlayer, int x, int y)
{
	C_BaseHLPlayer* pHLPlayer = dynamic_cast<C_BaseHLPlayer*>(pPlayer);
	bool bFlashlightOn = pHLPlayer ? pHLPlayer->IsFlashlightActive() : false;

	Color col = BAHUD_COLOR_ARMOR;
	wchar_t wszSymbol[2] = { bFlashlightOn ? (wchar_t)169 : (wchar_t)174, 0 };

	col = LerpBAHudColor(col, BAHUD_COLOR_FLASH, m_flFlashlightPulse * 0.8f);

	bool bLowBattery = (pHLPlayer && pHLPlayer->m_HL2Local.m_flFlashBattery >= 0.0f && pHLPlayer->m_HL2Local.m_flFlashBattery <= bahud_low_aux.GetFloat());
	if (bFlashlightOn && bLowBattery)
	{
		col = ModifyAlpha(col, (int)(col.a() * (0.35f + 0.65f * BAHudBlink(7.0f))));
	}

	col = ModifyAlpha(col, (int)(col.a() * m_flHudFade));

	int dummyW = 0, h = 0;
	GetTextSize(m_hFontIcon, L"*", dummyW, h);

	int sW = 0, sh = 0;
	GetTextSize(m_hFontFlashlightIcon, wszSymbol, sW, sh);

	int padding = ScreenScale(4.0f);
	int iconh = (int)(y + h + padding - sh * 0.7f);

	h += ScreenScale(3.0f);

	bool limited = (pHLPlayer && pHLPlayer->m_HL2Local.m_flFlashBattery >= 0.0f);
	if (!limited)
	{
		iconh = (int)(y + h + padding + ScreenScale(4.0f) * 0.5f - sh * 0.55f);
	}

	int flashBars = 12;
	float flashBarW = ScreenScaleF(2.0f);
	float flashBarPad = ScreenScaleF(1.5f);
	int flashBarH = ScreenScale(2.0f);

	int totalFlashW = (int)ceil(flashBars * flashBarW + (flashBars - 1) * flashBarPad);
	int totalW = MAX(sW, totalFlashW);

	DrawTextStringDepth(m_hFontFlashlightIcon, x, iconh, wszSymbol, col, totalW, x, false);

	if (!limited || !pHLPlayer)
		return;

	float charge = floor(pHLPlayer->m_HL2Local.m_flFlashBattery);
	if (charge < 0)
		return;

	Color colI = MultColor(col, 50);
	float step = 100.0f / (float)flashBars;
	float w = 0.0f;

	for (int i = 1; i <= flashBars; ++i)
	{
		Color barCol = (charge >= (i * step)) ? col : colI;
		int segW = (int)MAX(flashBarW, 1.0f);
		DrawFilledRectDepthPart(x + (int)w, segW, y + h + padding, flashBarH, barCol, x, totalW, false);
		w = floor(w + flashBarW + flashBarPad);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
struct BAmmoIconOffset_t
{
	const char* szName;
	int x;
	int y;
};

static const BAmmoIconOffset_t g_BAmmoIconOffsets[] =
{
	{ "smg1",         15, -5 },
	{ "pistol",       19, -5 },
	{ "357",          17, -6 },
	{ "smg1_grenade", 14, -8 },
	{ "ar2",          20, -7 },
	{ "ar2altfire",    0,  0 },
	{ "buckshot",     19, -6 },
	{ "xbowbolt",     10, -5 },
	{ "grenade",      17, -6 },
	{ "rpg_round",    10, -5 },
};

static void GetAmmoIconOffset(int iAmmoType, int& iOffX, int& iOffY)
{
	iOffX = 0;
	iOffY = 0;

	if (iAmmoType < 0)
		return;

	CAmmoDef* pAmmoDef = GetAmmoDef();
	if (!pAmmoDef)
		return;

	Ammo_t* pAmmo = pAmmoDef->GetAmmoOfIndex(iAmmoType);
	if (!pAmmo || !pAmmo->pName)
		return;

	for (int i = 0; i < ARRAYSIZE(g_BAmmoIconOffsets); ++i)
	{
		if (Q_stricmp(g_BAmmoIconOffsets[i].szName, pAmmo->pName) == 0)
		{
			iOffX = ScreenScale((float)g_BAmmoIconOffsets[i].x);
			iOffY = ScreenScale((float)g_BAmmoIconOffsets[i].y);
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintAmmo(C_BasePlayer* pPlayer, int x, int y)
{
	if (!bahud_ammo.GetBool())
		return;

	C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (!pWeapon || !pWeapon->UsesPrimaryAmmo())
		return;

	bool bSecondary = pWeapon->UsesSecondaryAmmo() && (pWeapon->Clip2() >= 0 || pPlayer->GetAmmoCount(pWeapon->GetSecondaryAmmoType()) > 0);

	if (bSecondary)
	{
		int x2 = DrawSecondaryAmmo(pPlayer, pWeapon, x, y);
		DrawPrimaryAmmo(pPlayer, pWeapon, x2, y);
	}
	else
	{
		DrawPrimaryAmmo(pPlayer, pWeapon, x, y);
	}

	if (pWeapon->UsesClipsForAmmo1() && pWeapon->Clip1() <= 0 && pPlayer->GetAmmoCount(pWeapon->GetPrimaryAmmoType()) > 0)
	{
		float flAlpha = m_flHudFade * (0.35f + 0.65f * BAHudBlink(5.0f));
		int wHint = 0, hHint = 0;
		GetTextSize(m_hFontCounterText, L"RELOAD", wHint, hHint);
		DrawTextString(m_hFontCounterText, x - wHint, y - ScreenScale(20.0f), L"RELOAD", ModifyAlpha(BAHUD_COLOR_AMMO, (int)(255 * flAlpha)));
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::DrawPrimaryAmmo(C_BasePlayer* pPlayer, C_BaseCombatWeapon* pWeapon, int x, int y)
{
	int totalWidth = 0;
	int barWidth = ScreenScale(bahud_def_ammo_width.GetFloat());
	int clip1 = pWeapon->Clip1();
	int ammo1 = pPlayer->GetAmmoCount(pWeapon->GetPrimaryAmmoType());

	Color col = BAHUD_COLOR_AMMO;
	int w2 = 0, h2 = 0;
	bool bUsesClips = pWeapon->UsesClipsForAmmo1() && (clip1 >= 0);

	col = GetAmmoFeedbackColor();

	if (clip1 <= 0 && (!bUsesClips || ammo1 <= 0))
	{
		col = ModifyAlpha(col, (int)(col.a() * (0.35f + 0.65f * BAHudBlink(6.0f))));
	}

	wchar_t wszAmmo1[32];
	if (bUsesClips)
	{
		V_snwprintf(wszAmmo1, ARRAYSIZE(wszAmmo1), L"/%d", ammo1);
		GetTextSize(m_hFontCounterStored, wszAmmo1, w2, h2);
		totalWidth += w2;
	}
	else
	{
		clip1 = ammo1;
	}

	wchar_t wszClip1[32];
	V_snwprintf(wszClip1, ARRAYSIZE(wszClip1), L"%d", clip1);

	int w1 = 0, h1 = 0;
	GetTextSize(m_hFontCounter, wszClip1, w1, h1);
	totalWidth += w1;

	wchar_t* pwszText = g_pVGuiLocalize->Find("#Valve_Hud_AMMO");
	const wchar_t* wszText = pwszText ? pwszText : L"AMMO";

	int w3 = 0, h3 = 0;
	GetTextSize(m_hFontCounterText, wszText, w3, h3);

	const CHudTexture* icon = gWR.GetAmmoIconFromWeapon(pWeapon->GetPrimaryAmmoType());
	int iconW = icon ? ScreenScale(12.0f) : 0;

	barWidth = MAX(barWidth, totalWidth + iconW + ScreenScale(8.0f) + w3 + ScreenScale(6.0f));

	int originX = x - barWidth;

	DrawTextStringDepth(m_hFontCounter, x - totalWidth, y, wszClip1, col, barWidth, originX, true);

	if (bUsesClips)
	{
		DrawTextStringDepth(m_hFontCounterStored, x - totalWidth + w1, y + h1 - h2, wszAmmo1, col, barWidth, originX, true);
	}

	if (icon)
	{
		int isize = ScreenScale(12.0f);
		int icx = 0, icy = 0;
		GetAmmoIconOffset(pWeapon->GetPrimaryAmmoType(), icx, icy);
		int iconOffset = GetDepthOffset(x - barWidth + icx + isize / 2, originX, barWidth, true);
		icon->DrawSelf(x - barWidth + icx, y + ScreenScale(2.0f) + icy + iconOffset, isize, isize * 3, col);
	}

	DrawTextStringDepth(m_hFontCounterText, x - barWidth + iconW + ScreenScale(4.0f), y + ScreenScale(29.0f) - h3, wszText, col, barWidth, originX, true);

	float fillage = 1.0f;
	if (bUsesClips && pWeapon->GetMaxClip1() > 0)
	{
		fillage = clamp((float)clip1 / (float)pWeapon->GetMaxClip1(), 0.0f, 1.0f);
	}

	int barHeight = ScreenScale(4.0f);
	DrawFilledRectDepth(x - barWidth, y + h1, barWidth, barHeight, barWidth, MultColor(col, 50), true);
	DrawFilledRectDepth(x - barWidth, y + h1, barWidth, barHeight, (int)(barWidth * fillage), col, true);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHudBorealAlyph::DrawSecondaryAmmo(C_BasePlayer* pPlayer, C_BaseCombatWeapon* pWeapon, int x, int y)
{
	int clip2 = pWeapon->Clip2();
	int ammo2 = pPlayer->GetAmmoCount(pWeapon->GetSecondaryAmmoType());
	Color col = BAHUD_COLOR_AMMO;

	col = GetAmmoFeedbackColor();

	if (clip2 <= 0 && ammo2 <= 0)
	{
		col = ModifyAlpha(col, (int)(col.a() * (0.35f + 0.65f * BAHudBlink(6.0f))));
	}

	bool bUsesClips = (clip2 >= 0 && pWeapon->GetMaxClip2() > 0);
	int barWidth;

	if (!bUsesClips)
	{
		barWidth = ScreenScale(40.0f);

		wchar_t wszAmmo2[32];
		V_snwprintf(wszAmmo2, ARRAYSIZE(wszAmmo2), L"%d", ammo2);

		int w1 = 0, h1 = 0;
		GetTextSize(m_hFontCounter, wszAmmo2, w1, h1);
		int totalWidth = w1;

		wchar_t* pwszText = g_pVGuiLocalize->Find("#Valve_Hud_AMMO_ALT");
		const wchar_t* wszText = pwszText ? pwszText : L"ALT";

		int w3 = 0, h3 = 0;
		GetTextSize(m_hFontCounterText, wszText, w3, h3);

		const CHudTexture* icon = gWR.GetAmmoIconFromWeapon(pWeapon->GetSecondaryAmmoType());
		int iconW = icon ? ScreenScale(12.0f) : 0;

		barWidth = MAX(barWidth, totalWidth + iconW + ScreenScale(8.0f) + w3 + ScreenScale(6.0f));

		int originX = x - barWidth;

		DrawTextStringDepth(m_hFontCounter, x - w1, y, wszAmmo2, col, barWidth, originX, true);

		if (icon)
		{
			int isize = ScreenScale(12.0f);
			int icx = 0, icy = 0;
			GetAmmoIconOffset(pWeapon->GetSecondaryAmmoType(), icx, icy);
			int iconOffset = GetDepthOffset(x - barWidth + icx + isize / 2, originX, barWidth, true);
			icon->DrawSelf(x - barWidth + icx, y + ScreenScale(4.0f) + icy + iconOffset, isize, isize * 3, col);
		}

		DrawTextStringDepth(m_hFontCounterText, x - barWidth + iconW + ScreenScale(4.0f), y + ScreenScale(29.0f) - h3, wszText, col, barWidth, originX, true);

		Color barDrawCol = (ammo2 > 0) ? col : MultColor(col, 50);
		DrawFilledRectDepth(x - barWidth, y + h1, barWidth, ScreenScale(4.0f), barWidth, barDrawCol, true);

		return x - barWidth - ScreenScale(14.0f);
	}

	barWidth = ScreenScale(pWeapon->GetMaxClip2() > 10 ? 80.0f : 40.0f);

	wchar_t wszAmmo2[32];
	V_snwprintf(wszAmmo2, ARRAYSIZE(wszAmmo2), L"/%d", ammo2);

	wchar_t wszClip2[32];
	V_snwprintf(wszClip2, ARRAYSIZE(wszClip2), L"%d", clip2);

	int w1 = 0, h1 = 0;
	GetTextSize(m_hFontCounter, wszClip2, w1, h1);
	int totalWidth = w1;

	int w2 = 0, h2 = 0;
	GetTextSize(m_hFontCounterStored, wszAmmo2, w2, h2);
	totalWidth += w2;

	wchar_t* pwszText = g_pVGuiLocalize->Find("#Valve_Hud_AMMO_ALT");
	const wchar_t* wszText = pwszText ? pwszText : L"ALT";

	int w3 = 0, h3 = 0;
	GetTextSize(m_hFontCounterText, wszText, w3, h3);

	const CHudTexture* icon = gWR.GetAmmoIconFromWeapon(pWeapon->GetSecondaryAmmoType());
	int iconW = icon ? ScreenScale(12.0f) : 0;

	barWidth = MAX(barWidth, totalWidth + iconW + ScreenScale(8.0f) + w3 + ScreenScale(6.0f));

	int originX = x - barWidth;

	DrawTextStringDepth(m_hFontCounterStored, x - totalWidth + w1, y + h1 - h2, wszAmmo2, col, barWidth, originX, true);
	DrawTextStringDepth(m_hFontCounter, x - totalWidth, y, wszClip2, col, barWidth, originX, true);

	if (icon)
	{
		int isize = ScreenScale(12.0f);
		int icx = 0, icy = 0;
		GetAmmoIconOffset(pWeapon->GetSecondaryAmmoType(), icx, icy);
		int iconOffset = GetDepthOffset(x - barWidth + icx + isize / 2, originX, barWidth, true);
		icon->DrawSelf(x - barWidth + icx, y + ScreenScale(4.0f) + icy + iconOffset, isize, isize * 3, col);
	}

	DrawTextStringDepth(m_hFontCounterText, x - barWidth + iconW + ScreenScale(4.0f), y + ScreenScale(29.0f) - h3, wszText, col, barWidth, originX, true);

	float fillage = clamp((float)clip2 / (float)pWeapon->GetMaxClip2(), 0.0f, 1.0f);
	DrawFilledRectDepth(x - barWidth, y + h1, barWidth, ScreenScale(4.0f), barWidth, MultColor(col, 50), true);
	DrawFilledRectDepth(x - barWidth, y + h1, barWidth, ScreenScale(4.0f), (int)(barWidth * fillage), col, true);

	return x - barWidth - ScreenScale(14.0f);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintCrosshair(C_BasePlayer* pPlayer)
{
	if (!pPlayer || !crosshair.GetInt() || pPlayer->IsInAVehicle())
		return;

	C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (!pWeapon || !pWeapon->ShouldDrawCrosshair())
		return;

	float x = ScreenWidth() / 2.0f;
	float y = ScreenHeight() / 2.0f;
	bool bBehindCamera = false;

	if (cl_crosshair_world.GetBool() && !IsPlayerZooming())
	{
		CalculateWorldCrosshair(&x, &y, &bBehindCamera);
	}
	else
	{
		CalculateStaticCrosshair(&x, &y, &bBehindCamera);
	}

	if (bBehindCamera)
		return;

	float baseSpread = 0.2f;
	float maxSpread = 0.9f;
	const char* szClass = pWeapon->GetClassname();

	if (Q_stristr(szClass, "shotgun"))
	{
		baseSpread = 0.75f;
		maxSpread = 1.0f;
	}
	else if (Q_stristr(szClass, "rifle") || Q_stristr(szClass, "smg"))
	{
		baseSpread = 0.25f;
		maxSpread = 1.0f;
	}
	else if (Q_stristr(szClass, "357") || Q_stristr(szClass, "revolver") || Q_stristr(szClass, "pistol"))
	{
		baseSpread = 0.12f;
		maxSpread = 0.85f;
	}

	float speed = 0.0f;
	Vector vecPos = pPlayer->GetAbsOrigin();
	if (m_bHavePrevCrossPos && gpGlobals->frametime > 0.0f)
	{
		Vector vecDelta = vecPos - m_vecPrevCrossPos;
		if (vecDelta.Length() < 100.0f)
		{
			vecDelta.y = 0.0f;
			speed = MIN(vecDelta.Length() / gpGlobals->frametime, 1000.0f);
		}
	}
	m_vecPrevCrossPos = vecPos;
	m_bHavePrevCrossPos = true;

	float accuracy = baseSpread + (maxSpread - baseSpread) * MIN(speed / 300.0f, 1.0f);
	if (!(pPlayer->GetFlags() & FL_ONGROUND))
		accuracy += (1.0f - accuracy) * 0.3f;

	QAngle vecPunch = pPlayer->m_Local.m_vecPunchAngle.m_Value;
	if (vecPunch.Length() > 0.01f)
		accuracy += (1.0f - accuracy) * MIN(vecPunch.Length() / 2.0f, 0.6f);

	int iClip = pWeapon->Clip1();
	if (pWeapon != m_pPrevCrossWeapon)
	{
		m_pPrevCrossWeapon = pWeapon;
		m_iPrevCrossClip = iClip;
		m_flCrossBloom = 0.0f;
	}
	else if (iClip >= 0 && iClip < m_iPrevCrossClip)
	{
		m_flCrossBloom = MIN(m_flCrossBloom + 0.2f, 1.0f);
		m_iPrevCrossClip = iClip;
	}
	else if (iClip != m_iPrevCrossClip)
	{
		m_iPrevCrossClip = iClip;
	}
	m_flCrossBloom = MAX(m_flCrossBloom - gpGlobals->frametime * 1.5f, 0.0f);
	accuracy += (1.0f - accuracy) * m_flCrossBloom;

	if (IsPlayerZooming())
		accuracy *= 0.3f;

	if (accuracy > 1.0f)
		accuracy = 1.0f;

	if (Q_stristr(szClass, "shotgun"))
	{
		DrawCrosshairShotgun((int)x, (int)y, accuracy);
	}
	else if (Q_stristr(szClass, "rifle") || Q_stristr(szClass, "smg"))
	{
		DrawCrosshairRifle((int)x, (int)y, accuracy);
	}
	else if (Q_stristr(szClass, "357") || Q_stristr(szClass, "revolver") || Q_stristr(szClass, "pistol"))
	{
		DrawCrosshairRevolver((int)x, (int)y, accuracy);
	}
	else
	{
		DrawCrosshairGeneric((int)x, (int)y, accuracy, 6.0f);
	}

	PaintCrosshairFeedback((int)x, (int)y);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::DrawCrosshairGeneric(int x, int y, float accuracy, float flBarLength)
{
	int width = MAX(1, (int)ceil(ScreenScaleF(bahud_crosshair_thickness.GetFloat())));
	int height = MAX(width, RoundFloatToInt(ScreenScaleF(flBarLength)));

	int padding = RoundFloatToInt(ScreenScaleF(1.0f + 5.0f * accuracy));
	int outline = MAX(1, RoundFloatToInt(ScreenScaleF(bahud_crosshair_outline.GetFloat())));

	int accurateW = width % 2 + (width / 3) - 1;
	int centerX = x - accurateW;
	int centerY = y - accurateW;

	Color black(0, 0, 0, 255);
	Color col = BAHUD_COLOR_CROSSHAIR;

	DrawOutlinedFilledRect(centerX, centerY, width, width, outline, black, col);
	DrawOutlinedFilledRect(centerX, centerY - height - padding, width, height, outline, black, col);
	DrawOutlinedFilledRect(centerX, centerY + padding + width, width, height, outline, black, col);
	DrawOutlinedFilledRect(centerX - height - padding, centerY, height, width, outline, black, col);
	DrawOutlinedFilledRect(centerX + padding + width, centerY, height, width, outline, black, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::DrawCrosshairRifle(int x, int y, float accuracy)
{
	DrawCrosshairGeneric(x, y, accuracy, 6.0f);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::DrawCrosshairRevolver(int x, int y, float accuracy)
{
	DrawCrosshairGeneric(x, y, accuracy, 3.0f);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::DrawCrosshairShotgun(int x, int y, float accuracy)
{
	int size = (int)(accuracy * ScreenScaleF(38.0f));
	int radius = size / 2;

	int outline = MAX(1, RoundFloatToInt(ScreenScaleF(bahud_crosshair_outline.GetFloat())));
	int width = MAX(1, (int)ceil(ScreenScaleF(bahud_crosshair_thickness.GetFloat())));

	Color black(0, 0, 0, 255);
	Color col = BAHUD_COLOR_CROSSHAIR;

	DrawCircleHollow(x, y, radius + outline, width + outline * 2, black);
	DrawCircleHollow(x, y, radius, width, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::CalculateWorldCrosshair(float* pX, float* pY, bool* pbBehindCamera)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
	{
		CalculateStaticCrosshair(pX, pY, pbBehindCamera);
		return;
	}

	Vector vecEyePos = pPlayer->EyePosition();
	QAngle angEyeAngles = pPlayer->EyeAngles();

	Vector vecForward;
	AngleVectors(angEyeAngles, &vecForward);

	float flDist = cl_crosshair_trace_dist.GetFloat();
	Vector vecEnd = vecEyePos + vecForward * flDist;

	trace_t tr;
	UTIL_TraceLine(vecEyePos, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr);

	Vector vecAimPoint = (tr.fraction < 1.0f) ? tr.endpos : vecEnd;

	Vector vecScreen;
	if (ScreenTransform(vecAimPoint, vecScreen))
	{
		*pbBehindCamera = true;
		return;
	}

	*pX = (ScreenWidth() / 2.0f) + (vecScreen.x * ScreenWidth() / 2.0f);
	*pY = (ScreenHeight() / 2.0f) - (vecScreen.y * ScreenHeight() / 2.0f);
	*pbBehindCamera = false;
}

void CHudBorealAlyph::CalculateStaticCrosshair(float* pX, float* pY, bool* pbBehindCamera)
{
	*pX = ScreenWidth() / 2.0f;
	*pY = ScreenHeight() / 2.0f;
	*pbBehindCamera = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHudBorealAlyph::IsPlayerZooming(void)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return false;

	C_BaseHLPlayer* pHLPlayer = dynamic_cast<C_BaseHLPlayer*>(pPlayer);
	return (pHLPlayer && pHLPlayer->m_HL2Local.m_bZooming);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::MsgFunc_Damage(bf_read& msg)
{
	int armor = msg.ReadByte();
	int damageTaken = msg.ReadByte();
	msg.ReadLong();

	Vector vecFrom;
	vecFrom.x = msg.ReadBitCoord();
	vecFrom.y = msg.ReadBitCoord();
	vecFrom.z = msg.ReadBitCoord();

	if (damageTaken > 0 || armor > 0)
	{
		if (damageTaken > 0)
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthDamageTaken");
		}
	}

	if (damageTaken > 0)
	{
		OnPlayerDamaged(damageTaken, vecFrom);
	}
	else if (armor > 0)
	{
		m_flArmorPulse = 1.0f;
	}
}

void CHudBorealAlyph::MsgFunc_Battery(bf_read& msg)
{
	m_iNewArmor = msg.ReadShort();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::OnPlayerDamaged(int damageTaken, const Vector& vecFrom)
{
	float flAmount = clamp((float)damageTaken / 40.0f, 0.25f, 1.0f);
	float flTime = gpGlobals->realtime;

	m_flHealthPulse = MAX(m_flHealthPulse, flAmount);
	m_flDamageShake = MAX(m_flDamageShake, flAmount);
	m_flCrossBloom = MIN(m_flCrossBloom + flAmount * 0.5f, 1.0f);

	if (vecFrom.IsZero())
		return;

	m_vecDamageFrom = vecFrom;
	m_flDamageDirTime = flTime;
	m_flDamageDirFade = flTime + 1.4f;
	m_flDamageDirAmount = flAmount;
	m_bDamageDirValid = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::UpdatePlayerFeedback(C_BasePlayer* pPlayer)
{
	float flTime = gpGlobals->realtime;
	float flFrameTime = gpGlobals->frametime;

	m_flHudFade = MIN(m_flHudFade + flFrameTime * 2.5f, 1.0f);

	int iHealth = MAX(pPlayer->GetHealth(), 0);
	if (m_iPrevHealth >= 0)
	{
		if (iHealth < m_iPrevHealth)
		{
			float flAmount = clamp((float)(m_iPrevHealth - iHealth) / 30.0f, 0.25f, 1.0f);
			m_flHealthPulse = MAX(m_flHealthPulse, flAmount);
			m_flDamageShake = MAX(m_flDamageShake, flAmount);
		}
		else if (iHealth > m_iPrevHealth)
		{
			m_flHealthHeal = 1.0f;
		}
	}
	m_iPrevHealth = iHealth;

	if (m_iPrevArmor >= 0)
	{
		if (m_iArmor < m_iPrevArmor)
		{
			m_flArmorPulse = 1.0f;
		}
		else if (m_iArmor > m_iPrevArmor)
		{
			m_flArmorHeal = 1.0f;
		}
	}
	m_iPrevArmor = m_iArmor;

	if (m_flPrevSuitPower >= 0.0f)
	{
		if (m_flSuitPower < m_flPrevSuitPower - 0.25f)
		{
			m_flHEVPulse = MAX(m_flHEVPulse, 0.6f);
		}
		else if (m_flSuitPower > m_flPrevSuitPower + 0.25f)
		{
			m_flHEVPulse = 1.0f;
		}
	}
	m_flPrevSuitPower = m_flSuitPower;

	C_BaseHLPlayer* pHLPlayer = dynamic_cast<C_BaseHLPlayer*>(pPlayer);
	bool bFlashlight = pHLPlayer ? pHLPlayer->IsFlashlightActive() : false;
	if (bFlashlight != m_bLastFlashlightOn)
	{
		m_flFlashlightPulse = 1.0f;
		m_bLastFlashlightOn = bFlashlight;
	}

	bool bAttack = (pPlayer->m_nButtons & IN_ATTACK) != 0;
	if (bAttack && !m_bPrevAttackDown)
	{
		m_flCrossBloom = MIN(m_flCrossBloom + 0.35f, 1.0f);

		C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
		if (!pWeapon || pWeapon->Clip1() < 0)
		{
			CheckHitMarker(pPlayer);
		}
	}
	m_bPrevAttackDown = bAttack;

	UpdateWeaponFeedback(pPlayer);

	float flDecay = flFrameTime * 2.5f;
	m_flHealthPulse = MAX(m_flHealthPulse - flDecay, 0.0f);
	m_flHealthHeal = MAX(m_flHealthHeal - flDecay, 0.0f);
	m_flArmorPulse = MAX(m_flArmorPulse - flDecay, 0.0f);
	m_flArmorHeal = MAX(m_flArmorHeal - flDecay, 0.0f);
	m_flAmmoPulse = MAX(m_flAmmoPulse - flDecay, 0.0f);
	m_flAmmoReload = MAX(m_flAmmoReload - flDecay, 0.0f);
	m_flHEVPulse = MAX(m_flHEVPulse - flDecay, 0.0f);
	m_flFlashlightPulse = MAX(m_flFlashlightPulse - flFrameTime * 3.0f, 0.0f);
	m_flDamageShake = MAX(m_flDamageShake - flFrameTime * 4.0f, 0.0f);
	m_flHitMarker = MAX(m_flHitMarker - flFrameTime * 4.0f, 0.0f);

	if (m_flDamageDirFade < flTime)
	{
		m_bDamageDirValid = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::UpdateWeaponFeedback(C_BasePlayer* pPlayer)
{
	C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (!pWeapon)
	{
		m_hPrevAmmoWeapon = NULL;
		m_flWeaponSwitchTime = 0.0f;
		m_flWeaponSwitchFade = 0.0f;
		return;
	}

	int clip1 = pWeapon->Clip1();
	int clip2 = pWeapon->Clip2();
	int ammo1 = (pWeapon->GetPrimaryAmmoType() >= 0) ? pPlayer->GetAmmoCount(pWeapon->GetPrimaryAmmoType()) : 0;
	int ammo2 = (pWeapon->GetSecondaryAmmoType() >= 0) ? pPlayer->GetAmmoCount(pWeapon->GetSecondaryAmmoType()) : 0;

	if (pWeapon != m_hPrevAmmoWeapon.Get())
	{
		m_hPrevAmmoWeapon = pWeapon;
		m_flWeaponSwitchTime = gpGlobals->realtime;
		m_flWeaponSwitchFade = gpGlobals->realtime + 1.5f;
	}
	else
	{
		if (clip1 >= 0 && clip1 < m_iPrevClip1)
		{
			m_flAmmoPulse = 1.0f;
			CheckHitMarker(pPlayer);
		}
		else if (clip1 > m_iPrevClip1)
		{
			m_flAmmoReload = 1.0f;
		}

		if (ammo1 > m_iPrevAmmo1 && clip1 <= m_iPrevClip1)
		{
			m_flAmmoReload = 1.0f;
		}
	}

	m_iPrevClip1 = clip1;
	m_iPrevClip2 = clip2;
	m_iPrevAmmo1 = ammo1;
	m_iPrevAmmo2 = ammo2;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::CheckHitMarker(C_BasePlayer* pPlayer)
{
	if (!bahud_crosshair.GetBool())
		return;

	Vector vecStart = pPlayer->EyePosition();
	Vector vecForward;
	AngleVectors(pPlayer->EyeAngles(), &vecForward);
	Vector vecEnd = vecStart + vecForward * cl_crosshair_trace_dist.GetFloat();

	trace_t tr;
	UTIL_TraceLine(vecStart, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr);

	C_BaseEntity* pTarget = tr.m_pEnt;
	if (pTarget && pTarget != pPlayer && (pTarget->IsNPC() || pTarget->IsPlayer()) && pTarget->IsAlive())
	{
		m_flHitMarker = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintCrosshairFeedback(int x, int y)
{
	if (m_flHitMarker <= 0.0f)
		return;

	float alpha = m_flHitMarker * m_flHudFade;
	int offset = RoundFloatToInt(ScreenScaleF(4.0f + (1.0f - m_flHitMarker) * 4.0f));
	int lineLen = MAX(2, RoundFloatToInt(ScreenScaleF(5.0f)));
	float thickness = MAX(ScreenScaleF(1.5f), 1.0f);

	Color col = ModifyAlpha(BAHUD_COLOR_CROSSHAIR, (int)(255 * alpha));

	for (int i = 0; i < 4; ++i)
	{
		float dx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float dy = (i < 2) ? -1.0f : 1.0f;
		float sx = x + dx * offset;
		float sy = y + dy * offset;

		DrawLine2D(sx, sy, sx + dx * lineLen, sy + dy * lineLen, thickness, col);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintDamageDirection(void)
{
	if (!m_bDamageDirValid)
		return;

	float flTime = gpGlobals->realtime;
	if (flTime >= m_flDamageDirFade)
		return;

	float flProg = clamp((flTime - m_flDamageDirTime) / (m_flDamageDirFade - m_flDamageDirTime), 0.0f, 1.0f);
	float flAlpha = (1.0f - flProg) * m_flDamageDirAmount * m_flHudFade;
	if (flAlpha <= 0.01f)
		return;

	Vector vecDelta = m_vecDamageFrom - MainViewOrigin();
	if (VectorNormalize(vecDelta) <= 0.0f)
		return;

	Vector forward, right;
	AngleVectors(MainViewAngles(), &forward, &right, NULL);

	float flFront = DotProduct(vecDelta, forward);
	float flSide = DotProduct(vecDelta, right);

	float dx = flSide;
	float dy = -flFront;
	float flLen = sqrt(dx * dx + dy * dy);
	if (flLen < 0.001f)
	{
		dx = 0.0f;
		dy = -1.0f;
		flLen = 1.0f;
	}
	dx /= flLen;
	dy /= flLen;

	float flCenterX = ScreenWidth() / 2.0f;
	float flCenterY = ScreenHeight() / 2.0f;
	float flInner = ScreenHeight() * 0.16f;
	float flOuter = flInner + ScreenScaleF(14.0f);
	float flWidth = MAX(ScreenScaleF(3.0f), 1.0f);

	DrawHudArrow(flCenterX, flCenterY, dx, dy, flInner, flOuter, flWidth, ModifyAlpha(BAHUD_COLOR_DAMAGE, (int)(255 * flAlpha)));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBorealAlyph::PaintWeaponName(C_BaseCombatWeapon* pWeapon, int x, int y)
{
	if (!pWeapon || m_flWeaponSwitchFade <= m_flWeaponSwitchTime)
		return;

	float flTime = gpGlobals->realtime;
	if (flTime >= m_flWeaponSwitchFade)
		return;

	float flProg = clamp((flTime - m_flWeaponSwitchTime) / (m_flWeaponSwitchFade - m_flWeaponSwitchTime), 0.0f, 1.0f);
	float flAlpha = MIN((1.0f - flProg) * 4.0f, 1.0f) * m_flHudFade;
	if (flAlpha <= 0.01f)
		return;

	const FileWeaponInfo_t& wpnInfo = pWeapon->GetWpnData();
	wchar_t* pwszName = g_pVGuiLocalize->Find(wpnInfo.szPrintName);
	wchar_t wszName[64];

	if (pwszName)
	{
		wcsncpy(wszName, pwszName, ARRAYSIZE(wszName));
		wszName[ARRAYSIZE(wszName) - 1] = 0;
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode(wpnInfo.szPrintName, wszName, sizeof(wszName));
	}

	int w = 0, h = 0;
	GetTextSize(m_hFontCounterText, wszName, w, h);

	int textX = x - w;
	int textY = y - ScreenScale(18.0f) + GetDepthOffset(textX, textX, w, true);
	Color col = ModifyAlpha(BAHUD_COLOR_AMMO, (int)(255 * flAlpha));

	DrawTextString(m_hFontCounterText, textX, textY, wszName, col);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Color CHudBorealAlyph::GetAmmoFeedbackColor(void)
{
	Color col = BAHUD_COLOR_AMMO;

	col = LerpBAHudColor(col, BAHUD_COLOR_FLASH, MAX(m_flAmmoPulse, m_flAmmoReload) * 0.75f);
	col = ModifyAlpha(col, (int)(col.a() * m_flHudFade));

	return col;
}

DECLARE_HUDELEMENT(CHudBAWeaponSelection);

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CHudBAWeaponSelection::CHudBAWeaponSelection(const char* pElementName) :
	CBaseHudWeaponSelection(pElementName), BaseClass(NULL, "HudBAWeaponSelection")
{
	Panel* pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_WEAPONSELECTION | HIDEHUD_NEEDSUIT | HIDEHUD_PLAYERDEAD);

	m_iSelectedSlot = -1;
	m_iSelectedPos = 0;
	m_flSelectorStartFade = 0;
	m_flSelectorEndFade = 0;
	m_flDenyFadeout = 0;
	m_flDenyFadeouts = 0;

	m_hFontSmallNumbers = INVALID_FONT;
	m_hFontWeaponName = INVALID_FONT;
	m_hFontNames = INVALID_FONT;

	m_flSelectorOpenTime = 0.0f;
	m_flDenyShake = 0.0f;
	m_flPickupFlash = 0.0f;
	m_iPickupSlot = -1;
}

CHudBAWeaponSelection::~CHudBAWeaponSelection()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::LevelInit()
{
	CHudElement::LevelInit();

	m_iSelectedSlot = -1;
	m_iSelectedPos = 0;
	m_flSelectorStartFade = 0;
	m_flSelectorEndFade = 0;
	m_flDenyFadeout = 0;
	m_flDenyFadeouts = 0;

	m_flSelectorOpenTime = 0.0f;
	m_flDenyShake = 0.0f;
	m_flPickupFlash = 0.0f;
	m_iPickupSlot = -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled(false);
	SetSize(ScreenWidth(), ScreenHeight());
	SetPos(0, 0);

	RegisterBAHudFontFiles();

	m_hFontSmallNumbers = CreateBAHudFont(BAHUD_FONT_DIN, 14.0f, 500);
	m_hFontWeaponName = CreateBAHudFont(BAHUD_FONT_DIN, 14.0f, 500);
	m_hFontNames = CreateBAHudFont(BAHUD_FONT_DIN, 18.0f, 500);

	g_BAIconFonts.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::OnWeaponPickup(C_BaseCombatWeapon* pWeapon)
{
	CHudHistoryResource* pHudHR = GET_HUDELEMENT(CHudHistoryResource);
	if (pHudHR)
	{
		pHudHR->AddToHistory(pWeapon);
	}

	m_flPickupFlash = gpGlobals->realtime + 1.2f;
	m_iPickupSlot = pWeapon ? pWeapon->GetSlot() : -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::ResetSelectorFade(void)
{
	m_flSelectorStartFade = gpGlobals->realtime + 2.0f;
	m_flSelectorEndFade = gpGlobals->realtime + 2.5f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CHudBAWeaponSelection::GetSelectorAlpha(void)
{
	float flTime = gpGlobals->realtime;
	float alpha = 255.0f;

	if (IsInSelectionMode() && m_flSelectorEndFade > m_flSelectorStartFade)
	{
		float prog = clamp((flTime - m_flSelectorStartFade) / (m_flSelectorEndFade - m_flSelectorStartFade), 0.0f, 1.0f);
		alpha = (1.0f - prog) * 255.0f;
	}
	else if (!IsInSelectionMode())
	{
		alpha = 0.0f;
	}

	if (m_flDenyFadeout > flTime && m_flDenyFadeout > m_flDenyFadeouts)
	{
		float prog = clamp((flTime - m_flDenyFadeouts) / (m_flDenyFadeout - m_flDenyFadeouts), 0.0f, 1.0f);
		alpha = MAX(alpha, (1.0f - prog) * 255.0f);
	}

	return alpha;
}

void CHudBAWeaponSelection::OpenSelection(void)
{
	CBaseHudWeaponSelection::OpenSelection();
	ResetSelectorFade();

	m_flSelectorOpenTime = gpGlobals->realtime;
}

void CHudBAWeaponSelection::HideSelection(void)
{
	CBaseHudWeaponSelection::HideSelection();
	m_flSelectorStartFade = 0;
	m_flSelectorEndFade = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::SelectWeapon(void)
{
	C_BaseCombatWeapon* pWeapon = GetSelectedWeapon();
	if (!pWeapon)
	{
		CancelWeaponSelection();
		return;
	}

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	if (!pWeapon->CanBeSelected())
	{
		pPlayer->EmitSound("Player.DenyWeaponSelection");
		return;
	}

	SetWeaponSelected();

	m_hSelectedWeapon = NULL;
	m_iSelectedSlot = -1;
	m_iSelectedPos = 0;

	HideSelection();

	pPlayer->EmitSound("Player.WeaponSelected");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::CancelWeaponSelection(void)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	if (IsInSelectionMode())
	{
		HideSelection();

		m_hSelectedWeapon = NULL;
		m_iSelectedSlot = -1;
		m_iSelectedPos = 0;

		pPlayer->EmitSound("Player.WeaponSelectionClose");
	}
}

void CHudBAWeaponSelection::YieldInstanceToStock(void)
{
	CHudElement* pElem = gHUD.FindElement("CHudWeaponSelection");
	CBaseHudWeaponSelection* pStock = pElem ? dynamic_cast<CBaseHudWeaponSelection*>(pElem) : NULL;
	if (pStock)
	{
		s_pInstance = pStock;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::CallWeaponSelectorDeny(void)
{
	m_flDenyFadeouts = gpGlobals->realtime;
	m_flDenyFadeout = gpGlobals->realtime + 1.2f;
	m_flDenyShake = gpGlobals->realtime + 0.3f;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (pPlayer)
	{
		pPlayer->EmitSound("Player.DenyWeaponSelection");
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::OnThink(void)
{
	if (bahud_enable.GetBool() && bahud_wepselect.GetBool())
	{
		s_pInstance = this;
	}
	ProcessInput();

	if (!IsInSelectionMode())
		return;

	if (gpGlobals->realtime >= m_flSelectorEndFade)
	{
		SelectWeapon();
		if (IsInSelectionMode())
		{
			HideSelection();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHudBAWeaponSelection::ShouldDraw(void)
{
	if (!bahud_enable.GetBool() || !bahud_wepselect.GetBool())
		return false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer || !pPlayer->IsAlive())
	{
		if (IsInSelectionMode())
		{
			HideSelection();
		}
		return false;
	}

	if (m_flDenyFadeout > gpGlobals->realtime)
		return true;

	if (!CBaseHudWeaponSelection::ShouldDraw())
		return false;

	return IsInSelectionMode();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHudBAWeaponSelection::GetWeaponsCountInSlot(int iSlot)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return 0;

	int count = 0;
	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
		if (pWeapon && pWeapon->GetSlot() == iSlot)
		{
			count++;
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHudBAWeaponSelection::GetLastPosInSlot(int iSlot) const
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return -1;

	int iMaxSlotPos = -1;
	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
		if (pWeapon && pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() > iMaxSlotPos)
		{
			iMaxSlotPos = pWeapon->GetPosition();
		}
	}

	return iMaxSlotPos;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_BaseCombatWeapon* CHudBAWeaponSelection::GetWeaponInSlot(int iSlot, int iSlotPos)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return NULL;

	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
		if (pWeapon && pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos)
		{
			return pWeapon;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_BaseCombatWeapon* CHudBAWeaponSelection::FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return NULL;

	C_BaseCombatWeapon* pNextWeapon = NULL;
	int iLowestNextSlot = MAX_WEAPON_SLOTS;
	int iLowestNextPosition = MAX_WEAPON_POSITIONS;

	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
		if (!pWeapon || !CanBeSelectedInHUD(pWeapon))
			continue;

		int weaponSlot = pWeapon->GetSlot();
		int weaponPosition = pWeapon->GetPosition();

		if (weaponSlot > iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition > iCurrentPosition))
		{
			if (weaponSlot < iLowestNextSlot || (weaponSlot == iLowestNextSlot && weaponPosition < iLowestNextPosition))
			{
				iLowestNextSlot = weaponSlot;
				iLowestNextPosition = weaponPosition;
				pNextWeapon = pWeapon;
			}
		}
	}

	return pNextWeapon;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_BaseCombatWeapon* CHudBAWeaponSelection::FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return NULL;

	C_BaseCombatWeapon* pPrevWeapon = NULL;
	int iLowestPrevSlot = -1;
	int iLowestPrevPosition = -1;

	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
		if (!pWeapon || !CanBeSelectedInHUD(pWeapon))
			continue;

		int weaponSlot = pWeapon->GetSlot();
		int weaponPosition = pWeapon->GetPosition();

		if (weaponSlot < iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition < iCurrentPosition))
		{
			if (weaponSlot > iLowestPrevSlot || (weaponSlot == iLowestPrevSlot && weaponPosition > iLowestPrevPosition))
			{
				iLowestPrevSlot = weaponSlot;
				iLowestPrevPosition = weaponPosition;
				pPrevWeapon = pWeapon;
			}
		}
	}

	return pPrevWeapon;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::CycleToNextWeapon(void)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	C_BaseCombatWeapon* pNextWeapon = NULL;
	if (IsInSelectionMode())
	{
		C_BaseCombatWeapon* pWeapon = GetSelectedWeapon();
		if (!pWeapon)
			return;

		pNextWeapon = FindNextWeaponInWeaponSelection(pWeapon->GetSlot(), pWeapon->GetPosition());
	}
	else
	{
		pNextWeapon = pPlayer->GetActiveWeapon();
		if (pNextWeapon)
		{
			pNextWeapon = FindNextWeaponInWeaponSelection(pNextWeapon->GetSlot(), pNextWeapon->GetPosition());
		}
	}

	if (!pNextWeapon)
	{
		pNextWeapon = FindNextWeaponInWeaponSelection(-1, -1);
	}

	if (pNextWeapon)
	{
		SetSelectedWeapon(pNextWeapon);
		m_iSelectedSlot = pNextWeapon->GetSlot();
		m_iSelectedPos = pNextWeapon->GetPosition();

		if (hud_fastswitch.GetInt() > 0)
		{
			SelectWeapon();
			return;
		}

		if (!IsInSelectionMode())
		{
			OpenSelection();
		}

		m_flSelectionTime = gpGlobals->curtime;
		ResetSelectorFade();
		pPlayer->EmitSound("Player.WeaponSelectionMoveSlot");
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::CycleToPrevWeapon(void)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	C_BaseCombatWeapon* pNextWeapon = NULL;
	if (IsInSelectionMode())
	{
		C_BaseCombatWeapon* pWeapon = GetSelectedWeapon();
		if (!pWeapon)
			return;

		pNextWeapon = FindPrevWeaponInWeaponSelection(pWeapon->GetSlot(), pWeapon->GetPosition());
	}
	else
	{
		pNextWeapon = pPlayer->GetActiveWeapon();
		if (pNextWeapon)
		{
			pNextWeapon = FindPrevWeaponInWeaponSelection(pNextWeapon->GetSlot(), pNextWeapon->GetPosition());
		}
	}

	if (!pNextWeapon)
	{
		pNextWeapon = FindPrevWeaponInWeaponSelection(MAX_WEAPON_SLOTS, MAX_WEAPON_POSITIONS);
	}

	if (pNextWeapon)
	{
		SetSelectedWeapon(pNextWeapon);
		m_iSelectedSlot = pNextWeapon->GetSlot();
		m_iSelectedPos = pNextWeapon->GetPosition();

		if (hud_fastswitch.GetInt() > 0)
		{
			SelectWeapon();
			return;
		}

		if (!IsInSelectionMode())
		{
			OpenSelection();
		}

		m_flSelectionTime = gpGlobals->curtime;
		ResetSelectorFade();
		pPlayer->EmitSound("Player.WeaponSelectionMoveSlot");
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::FastWeaponSwitch(int iWeaponSlot)
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	int iPosition = -1;
	C_BaseCombatWeapon* pActiveWeapon = pPlayer->GetActiveWeapon();
	if (pActiveWeapon && pActiveWeapon->GetSlot() == iWeaponSlot)
	{
		iPosition = pActiveWeapon->GetPosition();
	}

	C_BaseCombatWeapon* pNextWeapon = FindNextWeaponInWeaponSelection(iWeaponSlot, iPosition);
	if (!pNextWeapon || pNextWeapon->GetSlot() != iWeaponSlot)
	{
		pNextWeapon = FindNextWeaponInWeaponSelection(iWeaponSlot, -1);
	}

	if (pNextWeapon && pNextWeapon != pActiveWeapon && pNextWeapon->GetSlot() == iWeaponSlot)
	{
		::input->MakeWeaponSelection(pNextWeapon);
		pPlayer->EmitSound("Player.WeaponSelected");
	}
	else if (pNextWeapon != pActiveWeapon)
	{
		CallWeaponSelectorDeny();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::SelectWeaponSlot(int iSlot)
{
	--iSlot;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer || iSlot < 0 || iSlot >= MAX_WEAPON_SLOTS || !pPlayer->IsAllowedToSwitchWeapons())
		return;

	if (GetWeaponsCountInSlot(iSlot) == 0)
	{
		CallWeaponSelectorDeny();
		return;
	}

	if (hud_fastswitch.GetInt() > 0)
	{
		FastWeaponSwitch(iSlot);
		return;
	}

	int slotPos = 0;
	C_BaseCombatWeapon* pSelectedWeapon = GetSelectedWeapon();

	if (IsInSelectionMode() && pSelectedWeapon && pSelectedWeapon->GetSlot() == iSlot)
	{
		slotPos = pSelectedWeapon->GetPosition() + 1;
	}

	C_BaseCombatWeapon* pNextWeapon = GetNextActivePos(iSlot, slotPos);
	if (!pNextWeapon)
	{
		pNextWeapon = GetNextActivePos(iSlot, 0);
	}

	if (pNextWeapon)
	{
		bool bWasOpen = IsInSelectionMode();

		SetSelectedWeapon(pNextWeapon);
		m_iSelectedSlot = iSlot;
		m_iSelectedPos = pNextWeapon->GetPosition();
		m_flSelectionTime = gpGlobals->curtime;

		if (!bWasOpen)
		{
			OpenSelection();
			pPlayer->EmitSound("Player.WeaponSelectionOpen");
		}
		else
		{
			pPlayer->EmitSound("Player.WeaponSelectionMoveSlot");
		}

		ResetSelectorFade();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::Paint(void)
{
	if (!ShouldDraw())
		return;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	DrawWeaponSelector(pPlayer);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudBAWeaponSelection::DrawWeaponSelector(C_BasePlayer* pPlayer)
{
	int activeSlot = m_iSelectedSlot;
	if (!IsInSelectionMode())
	{
		activeSlot = -1;
	}

	bool deny = (activeSlot < 0 || GetWeaponsCountInSlot(activeSlot) == 0);

	int inaSquareW = ScreenScale(50.0f);
	int inaSquareH = ScreenScale(30.0f);
	int aSquareW = ScreenScale(120.0f);
	int aSquareH = ScreenScale(70.0f);
	int padding = ScreenScale(9.0f);
	int inpadding = ScreenScale(3.0f);
	int ypadding = ScreenScale(5.0f);
	int selectorBarH = ScreenScale(4.0f);

	int width = inaSquareW * (deny ? 6 : 5) + aSquareW * (deny ? 0 : 1);
	int x = ScreenWidth() / 2 - width / 2;
	int y = (int)(0.06f * ScreenHeight());

	if (m_flDenyShake > gpGlobals->realtime)
	{
		float flAmp = clamp((m_flDenyShake - gpGlobals->realtime) / 0.3f, 0.0f, 1.0f);
		x += (int)(sin(gpGlobals->realtime * 55.0f) * flAmp * ScreenScaleF(6.0f));
	}

	int originX = x;
	int totalW = width;

	float alpha = GetSelectorAlpha();

	Color active = ModifyAlpha(BAHUD_COLOR_SELECT_ACTIVE2, (int)((BAHUD_COLOR_SELECT_ACTIVE2.a() / 255.0f) * alpha));
	Color active2 = MultColor(active, 200);
	Color active3 = ModifyAlpha(BAHUD_COLOR_SELECT_BAR, (int)((BAHUD_COLOR_SELECT_BAR.a() / 255.0f) * alpha));
	Color empty = ModifyAlpha(BAHUD_COLOR_SELECT_EMPTY, (int)((BAHUD_COLOR_SELECT_BAR.a() / 255.0f) * alpha));
	Color empty2 = MultColor(empty, 200);
	Color empty3 = MultColor(ModifyAlpha(empty, (int)((empty.a() / 500.0f) * alpha)), 200);
	Color empty4 = MultColor(ModifyAlpha(empty, (int)((empty.a() / 1000.0f) * alpha)), 120);
	Color inactive = ModifyAlpha(BAHUD_COLOR_SELECT_INACTIVE2, (int)((BAHUD_COLOR_SELECT_INACTIVE2.a() / 255.0f) * alpha));
	Color inactive2 = MultColor(ModifyAlpha(inactive, (int)(inactive.a() * 0.4f)), 170);
	Color inactive3 = MultColor(ModifyAlpha(inactive, (int)((BAHUD_COLOR_SELECT_ACTIVE2.a() / 255.0f) * alpha)), 500);

	CUtlVector<C_BaseCombatWeapon*> weaponsInSlot;

	for (int i = 0; i < 6; ++i)
	{
		int slotSlide = 0;
		if (m_flSelectorOpenTime > 0.0f)
		{
			float flSince = gpGlobals->realtime - m_flSelectorOpenTime - (float)i * 0.035f;
			float flAnim = clamp(flSince / 0.16f, 0.0f, 1.0f);
			slotSlide = (int)((1.0f - flAnim) * ScreenScaleF(22.0f));
		}

		bool bPickupMark = (i == m_iPickupSlot && m_flPickupFlash > gpGlobals->realtime);
		int pickupThickness = MAX(1, ScreenScale(2.0f));
		Color pickupCol = ModifyAlpha(BAHUD_COLOR_SELECT_BAR, (int)((BAHUD_COLOR_SELECT_BAR.a() / 255.0f) * alpha * BAHudBlink(8.0f)));

		if (slotSlide != 0)
			x += slotSlide;

		if (activeSlot == i && !deny)
		{
			weaponsInSlot.RemoveAll();
			for (int slotpos = 0; slotpos <= GetLastPosInSlot(i); ++slotpos)
			{
				C_BaseCombatWeapon* pWpn = GetWeaponInSlot(i, slotpos);
				if (pWpn)
				{
					weaponsInSlot.AddToTail(pWpn);
				}
			}

			int colOffset = GetDepthOffset(x, originX, totalW, false);
			int curY = y + colOffset;
			int activeWpnPos = 0;
			C_BaseCombatWeapon* pSelWpn = GetSelectedWeapon();

			for (int k = 0; k < weaponsInSlot.Count(); ++k)
			{
				if (weaponsInSlot[k] == pSelWpn)
				{
					activeWpnPos = k;
					break;
				}
			}

			int scrollStart = 0;
			int scrollEnd = weaponsInSlot.Count() - 1;

			if (weaponsInSlot.Count() > 4)
			{
				scrollEnd = MIN(weaponsInSlot.Count() - 1, MAX(activeWpnPos + 2, 4));
				scrollStart = MAX(0, scrollEnd - 4);
			}

			for (int i2 = scrollStart; i2 <= scrollEnd; ++i2)
			{
				C_BaseCombatWeapon* pWpn = weaponsInSlot[i2];
				bool isactive = (i2 == activeWpnPos);

				int clip1 = pWpn->Clip1();
				int clip2 = pWpn->Clip2();
				int mclip1 = pWpn->GetMaxClip1();
				int mclip2 = pWpn->GetMaxClip2();
				int atype1 = pWpn->GetPrimaryAmmoType();
				int atype2 = pWpn->GetSecondaryAmmoType();
				bool isempty = true;

				if (atype1 < 0 && atype2 < 0 && mclip1 <= 0 && mclip2 <= 0)
				{
					isempty = false;
				}
				else
				{
					if (clip1 > 0 || (atype1 > 0 && pPlayer->GetAmmoCount(atype1) > 0))
					{
						isempty = false;
					}
					if (isempty && (clip2 > 0 || (atype2 > 0 && pPlayer->GetAmmoCount(atype2) > 0)))
					{
						isempty = false;
					}
				}

				Color textcol = isactive ? (isempty ? active : active3) : (isempty ? empty : active);
				Color bgCol = isactive ? (isempty ? empty3 : ModifyAlpha(MultColor(active, 220), (int)(alpha * 0.5f))) : (isempty ? empty4 : inactive);

				DrawFilledRect(x, curY, aSquareW, aSquareH, bgCol);

				if (bPickupMark)
				{
					DrawSlotHighlight(x, curY, aSquareW, aSquareH, pickupThickness, pickupCol);
				}

				const CHudTexture* icon = isactive ? pWpn->GetSpriteActive() : pWpn->GetSpriteInactive();
				if (icon)
				{
					Color iconCol = isactive ? (isempty ? empty : active3) : (isempty ? empty2 : active2);
					DrawWeaponIcon(icon, x + inpadding, curY + inpadding, aSquareW - inpadding * 2, aSquareH - inpadding * 2, iconCol);
				}

				const FileWeaponInfo_t& wpnInfo = pWpn->GetWpnData();
				wchar_t* pwszName = g_pVGuiLocalize->Find(wpnInfo.szPrintName);
				wchar_t wszName[64];
				if (pwszName)
				{
					wcsncpy(wszName, pwszName, ARRAYSIZE(wszName));
					wszName[ARRAYSIZE(wszName) - 1] = 0;
				}
				else
				{
					g_pVGuiLocalize->ConvertANSIToUnicode(wpnInfo.szPrintName, wszName, sizeof(wszName));
				}

				int tw = 0, th = 0;
				GetTextSize(m_hFontSmallNumbers, wszName, tw, th);
				DrawTextString(m_hFontSmallNumbers, x + inpadding, curY + aSquareH - th - inpadding, wszName, textcol);

				char szAmmoText[64] = "";
				if (atype1 >= 0)
				{
					int ammo = pPlayer->GetAmmoCount(atype1);
					if (clip1 >= 0)
					{
						V_snprintf(szAmmoText, sizeof(szAmmoText), "%d / %d", clip1, ammo);
					}
					else
					{
						V_snprintf(szAmmoText, sizeof(szAmmoText), "%d", ammo);
					}
				}

				if (atype2 >= 0)
				{
					int ammo = pPlayer->GetAmmoCount(atype2);
					char szSecondary[32];
					if (clip2 >= 0)
					{
						if (szAmmoText[0] != '\0')
						{
							V_snprintf(szSecondary, sizeof(szSecondary), " | %d / %d", clip2, ammo);
						}
						else
						{
							V_snprintf(szSecondary, sizeof(szSecondary), "- / - | %d / %d", clip2, ammo);
						}
					}
					else
					{
						if (szAmmoText[0] != '\0')
						{
							V_snprintf(szSecondary, sizeof(szSecondary), " | %d", ammo);
						}
						else
						{
							V_snprintf(szSecondary, sizeof(szSecondary), "- / - | %d", ammo);
						}
					}
					Q_strncat(szAmmoText, szSecondary, sizeof(szAmmoText));
				}

				if (szAmmoText[0] != '\0')
				{
					DrawTextStringA(m_hFontSmallNumbers, x + inpadding, curY + inpadding, szAmmoText, textcol);
				}

				if (pWpn == pPlayer->GetActiveWeapon())
				{
					int size = ScreenScale(8.0f);
					DrawFilledCircle(x - inpadding + aSquareW - size + size / 2, curY + inpadding + size / 2, size / 2, textcol);
				}

				curY += ypadding + aSquareH;

				if (isempty)
				{
					if (mclip1 > 0)
					{
						DrawFilledRect(x, curY - ypadding - 1, aSquareW, selectorBarH, MultColor(empty3, 300));
						curY += selectorBarH;
					}

					if (mclip2 > 0)
					{
						DrawFilledRect(x, curY - ypadding - 1, aSquareW, selectorBarH, MultColor(empty3, 300));
						curY += selectorBarH;
					}
				}
				else
				{
					if (mclip1 > 0)
					{
						DrawFilledRect(x, curY - ypadding - 1, aSquareW, selectorBarH, inactive3);
						float fill = clamp((float)clip1 / (float)mclip1, 0.0f, 1.0f);
						DrawFilledRect(x, curY - ypadding - 1, (int)(aSquareW * fill), selectorBarH, active3);
						curY += selectorBarH;
					}

					if (mclip2 > 0)
					{
						DrawFilledRect(x, curY - ypadding - 1, aSquareW, selectorBarH, inactive3);
						float fill = clamp((float)clip2 / (float)mclip2, 0.0f, 1.0f);
						DrawFilledRect(x, curY - ypadding - 1, (int)(aSquareW * fill), selectorBarH, active3);
						curY += selectorBarH;
					}
				}
			}

			x += aSquareW + padding;
		}
		else
		{
			bool bHasWeapons = (GetWeaponsCountInSlot(i) > 0);
			int colOffset = GetDepthOffset(x, originX, totalW, false);
			DrawFilledRect(x, y + colOffset, inaSquareW, inaSquareH, bHasWeapons ? inactive : inactive2);

			if (bPickupMark)
			{
				DrawSlotHighlight(x, y + colOffset, inaSquareW, inaSquareH, pickupThickness, pickupCol);
			}

			wchar_t wszSlotNum[4];
			V_snwprintf(wszSlotNum, ARRAYSIZE(wszSlotNum), L"%d", i + 1);
			DrawTextString(m_hFontSmallNumbers, x + inpadding, y + inpadding + colOffset, wszSlotNum, bHasWeapons ? active : active2);

			x += inaSquareW + padding;
		}

		if (slotSlide != 0)
			x -= slotSlide;
	}
}
