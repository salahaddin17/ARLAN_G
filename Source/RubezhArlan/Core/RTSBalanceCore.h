#pragma once

// ============================================================================
// Чистое ядро баланса «РУБЕЖ: АРЛАН». НОЛЬ зависимостей от Unreal —
// компилируется любым C++17-компилятором, тестируется вне движка
// (Tools/logic_tests.cpp) и включается UE-модулем как источник
// fallback-значений и формул. Числа перенесены из prototype-web
// (эталонный Canvas-прототип), масштаб: 1 клетка = 32 px = 100 UU,
// т.е. дистанции/скорости прототипа × 3.125.
// ============================================================================

#include <cstdint>

namespace RTSCore
{

// --- Перечисления (порядок ДОЛЖЕН совпадать с UENUM в Data/RTSTypes.h) -----

enum class EDmg : uint8_t { Bullet = 0, Rocket, Shell, Defense, COUNT };
enum class EArmor : uint8_t { INF = 0, LGT, HVY, BLD, COUNT };
enum class EFaction : uint8_t { Legion = 0, Front, COUNT };
enum class EUnit : uint8_t { Technician = 0, Truck, Rifleman, Rocketeer, Scout, Tank, Artillery, COUNT };
enum class EBuilding : uint8_t { HQ = 0, Power, Barracks, Factory, Turret, COUNT };
enum class EDifficulty : uint8_t { Easy = 0, Normal, Hard, COUNT };

// --- Мировые константы ------------------------------------------------------

constexpr float UUPerTile = 100.f;     // 1 клетка = 1 м
constexpr int32_t MapTiles = 64;       // карта 64×64 клетки
constexpr float MapHalfSize = MapTiles * UUPerTile * 0.5f; // мир центрирован в 0

// --- Матрица урона: [тип урона][тип брони] ----------------------------------
// bullet косит пехоту, rocket жжёт тяжёлую броню, shell ломает здания,
// defense (турели) сдерживает всё, но не годится для штурма зданий.

constexpr float DamageMatrix[uint8_t(EDmg::COUNT)][uint8_t(EArmor::COUNT)] = {
	//            INF    LGT    HVY    BLD
	/* Bullet */ {1.25f, 0.75f, 0.40f, 0.35f},
	/* Rocket */ {0.60f, 1.00f, 1.30f, 0.90f},
	/* Shell  */ {0.80f, 1.15f, 1.00f, 1.25f},
	/* Defense*/ {1.00f, 1.00f, 0.80f, 0.50f},
};

constexpr float DamageMultiplier(EDmg Dmg, EArmor Armor)
{
	return DamageMatrix[uint8_t(Dmg)][uint8_t(Armor)];
}

// --- Фракции -----------------------------------------------------------------

struct FFactionMods
{
	float CostMul;
	float HpMul;
	float SpeedMul;
	float DamageMul;
};

constexpr FFactionMods FactionMods[uint8_t(EFaction::COUNT)] = {
	/* Legion */ {1.15f, 1.15f, 1.00f, 1.15f},
	/* Front  */ {0.85f, 0.90f, 1.10f, 1.00f},
};

constexpr const FFactionMods& Mods(EFaction F) { return FactionMods[uint8_t(F)]; }

inline int32_t ScaledCost(int32_t Base, EFaction F)
{
	return int32_t(float(Base) * Mods(F).CostMul + 0.5f);
}
inline float ScaledHp(float Base, EFaction F) { return Base * Mods(F).HpMul; }
inline float ScaledSpeed(float Base, EFaction F) { return Base * Mods(F).SpeedMul; }
inline float ScaledDamage(float Base, EFaction F) { return Base * Mods(F).DamageMul; }

/** Полный типизированный урон с учётом фракции атакующего. */
inline float EffectiveDamage(float BaseDamage, EFaction Attacker, EDmg Dmg, EArmor Armor)
{
	return ScaledDamage(BaseDamage, Attacker) * DamageMultiplier(Dmg, Armor);
}

// --- Экономика ----------------------------------------------------------------

namespace Econ
{
	constexpr int32_t StartCredits = 600;
	constexpr float   TruckCapacity = 40.f;
	constexpr float   LoadRate = 22.f;        // припасов/сек при погрузке
	constexpr float   UnloadTime = 1.2f;      // сек разгрузки у КЦ
	constexpr float   LowPowerMul = 0.5f;     // производство при дефиците энергии
	constexpr float   RepairSpeedMul = 0.5f;
	constexpr int32_t DepotNearBase = 2600;
	constexpr int32_t DepotCenter = 4200;

	/** Множитель скорости производства по энергобалансу. */
	constexpr float ProductionMultiplier(int32_t PowerUse, int32_t PowerProduce)
	{
		return PowerUse > PowerProduce ? LowPowerMul : 1.0f;
	}

	/** Секунды на полную погрузку грузовика. */
	constexpr float FullLoadTime() { return TruckCapacity / LoadRate; }
}

// --- Юниты ----------------------------------------------------------------------

struct FUnitStats
{
	const char* Name;
	int32_t Cost;
	float MaxHp;
	float Speed;          // UU/сек
	EArmor Armor;
	int32_t VisionTiles;
	float BuildTime;      // сек производства
	float Damage;         // 0 = не вооружён
	float Range;          // UU
	float Cooldown;       // сек между выстрелами
	EDmg DmgType;
	float SplashRadius;   // UU, 0 = нет сплеша
	float MinRange;       // UU
	float ProjectileSpeed;// UU/с, 0 = мгновенное попадание
	float CargoCapacity;  // только грузовик
	bool bInfantry;       // фишка-круг vs фишка-прямоугольник
	EBuilding ProducedAt;
};

constexpr FUnitStats UnitStats[uint8_t(EUnit::COUNT)] = {
	//                name          cost   hp    speed  armor        vis bld  dmg   range  cd    dmgtype        splash minR  projV  cargo  inf    from
	/* Technician */ {"Tekhnik",     120,  60.f, 172.f, EArmor::INF, 5,  8.f,  0.f,   0.f, 0.0f, EDmg::Bullet,    0.f,   0.f,   0.f,  0.f, true,  EBuilding::HQ},
	/* Truck      */ {"Gruzovik",    140, 150.f, 244.f, EArmor::LGT, 5, 10.f,  0.f,   0.f, 0.0f, EDmg::Bullet,    0.f,   0.f,   0.f, 40.f, false, EBuilding::HQ},
	/* Rifleman   */ {"Pekhotinets",  60,  70.f, 156.f, EArmor::INF, 6,  5.f,  9.f, 345.f, 0.7f, EDmg::Bullet,    0.f,   0.f,   0.f,  0.f, true,  EBuilding::Barracks},
	/* Rocketeer  */ {"Raketchik",   110,  60.f, 144.f, EArmor::INF, 6,  7.f, 26.f, 455.f, 1.9f, EDmg::Rocket,    0.f,   0.f,   0.f,  0.f, true,  EBuilding::Barracks},
	/* Scout      */ {"Razvedka",    130, 120.f, 350.f, EArmor::LGT, 9,  8.f,  8.f, 375.f, 0.35f,EDmg::Bullet,    0.f,   0.f,   0.f,  0.f, false, EBuilding::Factory},
	/* Tank       */ {"Tank",        320, 340.f, 194.f, EArmor::HVY, 6, 14.f, 46.f, 470.f, 2.2f, EDmg::Shell,     0.f,   0.f,   0.f,  0.f, false, EBuilding::Factory},
	/* Artillery  */ {"Artilleriya", 380, 160.f, 144.f, EArmor::LGT, 6, 16.f, 60.f, 830.f, 4.6f, EDmg::Shell,   120.f, 220.f, 600.f,  0.f, false, EBuilding::Factory},
};

constexpr const FUnitStats& Unit(EUnit U) { return UnitStats[uint8_t(U)]; }

// --- Здания -----------------------------------------------------------------------

struct FBuildingStats
{
	const char* Name;
	int32_t Cost;
	float MaxHp;
	int32_t SizeX;        // клетки
	int32_t SizeY;
	float BuildTime;      // сек стройки техником
	int32_t VisionTiles;
	int32_t PowerProduce;
	int32_t PowerUse;
	float Damage;         // турель
	float Range;
	float Cooldown;
	EDmg DmgType;
};

constexpr FBuildingStats BuildingStats[uint8_t(EBuilding::COUNT)] = {
	//              name             cost   hp     sx sy bld    vis prod use  dmg   range  cd    type
	/* HQ       */ {"KomandnyCentr",  900, 1600.f, 4, 3, 45.f,  7,  10,  0,   0.f,   0.f, 0.f,  EDmg::Defense},
	/* Power    */ {"Energostantsiya",220,  500.f, 2, 2, 14.f,  4,  25,  0,   0.f,   0.f, 0.f,  EDmg::Defense},
	/* Barracks */ {"Kazarmy",        300,  700.f, 3, 2, 18.f,  5,   0, 10,   0.f,   0.f, 0.f,  EDmg::Defense},
	/* Factory  */ {"ZavodTekhniki",  520, 1000.f, 3, 3, 26.f,  5,   0, 15,   0.f,   0.f, 0.f,  EDmg::Defense},
	/* Turret   */ {"Turel",          260,  420.f, 1, 1, 12.f,  6,   0, 10,  22.f, 580.f, 1.1f, EDmg::Defense},
};

constexpr const FBuildingStats& Building(EBuilding B) { return BuildingStats[uint8_t(B)]; }

/** Кто что производит. */
constexpr bool Produces(EBuilding B, EUnit U)
{
	switch (B)
	{
	case EBuilding::HQ:       return U == EUnit::Technician || U == EUnit::Truck;
	case EBuilding::Barracks: return U == EUnit::Rifleman || U == EUnit::Rocketeer;
	case EBuilding::Factory:  return U == EUnit::Scout || U == EUnit::Tank || U == EUnit::Artillery;
	default:                  return false;
	}
}

// --- Сложности ИИ -------------------------------------------------------------------

struct FDifficultyStats
{
	const char* Name;
	float IncomeMul;      // множитель дохода ИИ
	float FirstWaveAt;    // сек до старта первой волны (стычка на 3–5 мин)
	float WaveInterval;   // базовый интервал волн
	int32_t WaveStart;    // размер первой волны
	int32_t WaveGrowth;   // прирост размера
	int32_t ArmyCap;      // максимум боевых юнитов ИИ
};

constexpr FDifficultyStats DifficultyStats[uint8_t(EDifficulty::COUNT)] = {
	/* Easy   */ {"Rekrut",   0.80f, 230.f, 110.f, 3, 2, 14},
	/* Normal */ {"Veteran",  1.00f, 190.f,  90.f, 5, 3, 22},
	/* Hard   */ {"Komissar", 1.25f, 165.f,  72.f, 6, 4, 32},
};

constexpr const FDifficultyStats& Difficulty(EDifficulty D) { return DifficultyStats[uint8_t(D)]; }

// --- Сплеш ---------------------------------------------------------------------------

/** Затухание сплеша: 1.0 в эпицентре, не ниже 0.45 в зоне поражения
 *  (радиус ×1.2), ноль за её пределами. */
inline float SplashFalloff(float Distance, float SplashRadius)
{
	if (SplashRadius <= 0.f) { return Distance <= 0.f ? 1.f : 0.f; }
	if (Distance > SplashRadius * 1.2f) { return 0.f; }
	const float F = 1.f - Distance / (SplashRadius * 1.4f);
	return F < 0.45f ? 0.45f : F;
}

} // namespace RTSCore
