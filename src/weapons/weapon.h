#ifndef WEAPON_H
#define WEAPON_H

/* ================================================================== */
/*  Weapon types and data-driven weapon struct                         */
/* ================================================================== */

enum class WeaponType  { NONE, ASSAULT_RIFLE, SHOTGUN, ENERGY_WEAPON, HANDGUN };
enum class WeaponState { IDLE, FIRE_BULLET, FIRE_GRENADE, RELOAD };

struct Weapon {
    WeaponType  type  = WeaponType::NONE;
    WeaponState state = WeaponState::IDLE;

    /* animation state */
    float animTimer    = 0.0f;
    int   reloadFrame  = 0;
    float autoFireTimer = 0.0f;
    bool  prevLmb      = false;
    bool  prevRmb      = false;
    bool  firedThisFrame = false;  /* set by weaponFirePrimary, cleared each frame */
    bool  firedAltThisFrame = false; /* set by weaponFireSecondary, cleared each frame */

    /* GL texture handles */
    unsigned int texIdle    = 0;
    unsigned int texFire    = 0;
    unsigned int texAltFire = 0;
    static constexpr int MAX_ANIM = 11;
    unsigned int texAnim[MAX_ANIM] = {};
    int animFrameCount = 0;

    /* timing */
    float fireDur      = 0.12f;
    float altFireDur   = 0.25f;
    float animFrameDur = 0.10f;

    /* ammo */
    bool unlimited = true;
    int  maxAmmo1 = 0, ammo1 = 0;
    int  maxAmmo2 = 0, ammo2 = 0;
    bool hasAltFire = false;

    /* behaviour */
    bool  autoFire         = false;
    float autoFireInterval = 0.07f;

    /* recoil */
    float recoilKick    = 0.04f;
    float altRecoilKick = 0.06f;

    /* energy weapon fields */
    bool  isEnergy     = false;
    float charge1      = 1.0f;
    float charge2      = 1.0f;
    float drainTime1   = 6.0f;
    float drainTime2   = 3.0f;
    float rechargeRate = 0.15f;
    bool  depleted1    = false;
    bool  depleted2    = false;

    /* ammo-bar display config */
    struct BarCfg { bool show = false; float r = 0, g = 0, b = 0; };
    BarCfg bar1, bar2;

    /* per-weapon callbacks (set by factory) */
    void (*handleFire)(Weapon& w, float dt, bool lmb, bool rmb);
    void (*advanceAnim)(Weapon& w, float dt);
};

/* ------------------------------------------------------------------ */
/*  shared helpers                                                     */
/* ------------------------------------------------------------------ */

unsigned int loadWeaponTexture(const char* path);

void weaponStartReload(Weapon& w);
void weaponFirePrimary(Weapon& w);
void weaponFireSecondary(Weapon& w);
void weaponReload(Weapon& w);
void updateWeapon(Weapon& w, float dt, bool lmb, bool rmb);
unsigned int getWeaponTexture(const Weapon& w);
void cleanupWeapon(Weapon& w);

/* ------------------------------------------------------------------ */
/*  factory functions (one per weapon file)                            */
/* ------------------------------------------------------------------ */

Weapon createAssaultRifle();
Weapon createShotgun();
Weapon createEnergyWeapon();
Weapon createHandgun();

#endif
