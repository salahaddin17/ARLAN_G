// ============================================================================
// Standalone-тесты чистого ядра баланса (RTSBalanceCore.h) — компилируются
// и бегут БЕЗ Unreal: Tools/run_logic_tests.sh. Числа-эталоны согласованы
// с prototype-web/tests (vitest) — единая проверенная база баланса.
// ============================================================================

#include "../Source/RubezhArlan/Core/RTSBalanceCore.h"
#include <cstdio>
#include <cmath>

using namespace RTSCore;

static int GFailed = 0;
static int GTotal = 0;

#define CHECK(Cond) \
	do { ++GTotal; if (!(Cond)) { ++GFailed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); } } while (0)

#define CHECK_NEAR(A, B, Tol) CHECK(std::fabs((A) - (B)) <= (Tol))

static void TestDamageMatrix()
{
	// матрица заполнена и в разумных пределах
	for (uint8_t d = 0; d < uint8_t(EDmg::COUNT); ++d)
	{
		for (uint8_t a = 0; a < uint8_t(EArmor::COUNT); ++a)
		{
			const float M = DamageMatrix[d][a];
			CHECK(M > 0.f && M <= 2.f);
		}
	}
	// камень-ножницы-бумага
	CHECK(DamageMultiplier(EDmg::Bullet, EArmor::INF) > DamageMultiplier(EDmg::Bullet, EArmor::HVY));
	CHECK(DamageMultiplier(EDmg::Bullet, EArmor::INF) > DamageMultiplier(EDmg::Rocket, EArmor::INF));
	CHECK(DamageMultiplier(EDmg::Rocket, EArmor::HVY) > DamageMultiplier(EDmg::Bullet, EArmor::HVY));
	CHECK(DamageMultiplier(EDmg::Shell, EArmor::BLD) > DamageMultiplier(EDmg::Bullet, EArmor::BLD));
	CHECK(DamageMultiplier(EDmg::Shell, EArmor::BLD) > DamageMultiplier(EDmg::Rocket, EArmor::BLD));
	CHECK(DamageMultiplier(EDmg::Defense, EArmor::BLD) <= 0.5f);
	// конкретные эталоны (совпадают с prototype-web)
	CHECK_NEAR(DamageMultiplier(EDmg::Bullet, EArmor::INF), 1.25f, 1e-6f);
	CHECK_NEAR(DamageMultiplier(EDmg::Rocket, EArmor::HVY), 1.30f, 1e-6f);
	CHECK_NEAR(DamageMultiplier(EDmg::Shell, EArmor::BLD), 1.25f, 1e-6f);
	CHECK_NEAR(DamageMultiplier(EDmg::Bullet, EArmor::BLD), 0.35f, 1e-6f);
}

static void TestFactions()
{
	CHECK_NEAR(Mods(EFaction::Legion).CostMul, 1.15f, 1e-6f);
	CHECK_NEAR(Mods(EFaction::Legion).HpMul, 1.15f, 1e-6f);
	CHECK_NEAR(Mods(EFaction::Legion).DamageMul, 1.15f, 1e-6f);
	CHECK_NEAR(Mods(EFaction::Front).CostMul, 0.85f, 1e-6f);
	CHECK_NEAR(Mods(EFaction::Front).HpMul, 0.90f, 1e-6f);
	CHECK(Mods(EFaction::Front).SpeedMul > Mods(EFaction::Legion).SpeedMul);

	// округление цены: танк 320 → 368 у Легиона, 272 у Фронта
	CHECK(ScaledCost(Unit(EUnit::Tank).Cost, EFaction::Legion) == 368);
	CHECK(ScaledCost(Unit(EUnit::Tank).Cost, EFaction::Front) == 272);
	CHECK(ScaledHp(Unit(EUnit::Rifleman).MaxHp, EFaction::Legion) >
	      ScaledHp(Unit(EUnit::Rifleman).MaxHp, EFaction::Front));
}

static void TestEffectiveDamage()
{
	// ракетчик против танка эффективнее пехотинца против танка (по DPS, >2×)
	const FUnitStats& Rok = Unit(EUnit::Rocketeer);
	const FUnitStats& Rif = Unit(EUnit::Rifleman);
	const float RokDps = EffectiveDamage(Rok.Damage, EFaction::Front, Rok.DmgType, EArmor::HVY) / Rok.Cooldown;
	const float RifDps = EffectiveDamage(Rif.Damage, EFaction::Front, Rif.DmgType, EArmor::HVY) / Rif.Cooldown;
	CHECK(RokDps > RifDps * 2.f);

	// фракционный бонус урона Легиона учитывается
	CHECK_NEAR(EffectiveDamage(100.f, EFaction::Legion, EDmg::Shell, EArmor::HVY), 115.f, 1e-3f);
	CHECK_NEAR(EffectiveDamage(100.f, EFaction::Front, EDmg::Shell, EArmor::HVY), 100.f, 1e-3f);
}

static void TestEconomy()
{
	// дефицит энергии → ×0.5, иначе ×1
	CHECK_NEAR(Econ::ProductionMultiplier(20, 10), 0.5f, 1e-6f);
	CHECK_NEAR(Econ::ProductionMultiplier(10, 10), 1.0f, 1e-6f);
	CHECK_NEAR(Econ::ProductionMultiplier(0, 35), 1.0f, 1e-6f);

	// цикл грузовика: полная погрузка 40/22 ≈ 1.818 c, разгрузка 1.2 c
	CHECK_NEAR(Econ::FullLoadTime(), 40.f / 22.f, 1e-4f);
	CHECK(Econ::FullLoadTime() + Econ::UnloadTime < 4.f); // цикл без дороги короче 4 c

	// симуляция погрузки тиками 20 Гц: ровно до вместимости, склад уменьшается
	float Cargo = 0.f, Depot = 100.f;
	const float Dt = 0.05f;
	int Ticks = 0;
	while (Cargo < Econ::TruckCapacity - 1e-3f && Ticks < 200)
	{
		const float Take = std::fmin(Econ::LoadRate * Dt, std::fmin(Econ::TruckCapacity - Cargo, Depot));
		Cargo += Take;
		Depot -= Take;
		++Ticks;
	}
	CHECK_NEAR(Cargo, Econ::TruckCapacity, 1e-3f);
	CHECK_NEAR(Depot, 100.f - Econ::TruckCapacity, 1e-3f);
	CHECK(Ticks <= int(Econ::FullLoadTime() / Dt) + 2);
}

static void TestUnitsAndBuildings()
{
	// производственные цепочки
	CHECK(Produces(EBuilding::HQ, EUnit::Technician));
	CHECK(Produces(EBuilding::HQ, EUnit::Truck));
	CHECK(Produces(EBuilding::Barracks, EUnit::Rifleman));
	CHECK(Produces(EBuilding::Barracks, EUnit::Rocketeer));
	CHECK(Produces(EBuilding::Factory, EUnit::Tank));
	CHECK(Produces(EBuilding::Factory, EUnit::Artillery));
	CHECK(!Produces(EBuilding::Power, EUnit::Rifleman));
	CHECK(!Produces(EBuilding::Barracks, EUnit::Tank));

	// артиллерия: сплеш, минимальная дальность, летящий снаряд
	const FUnitStats& Art = Unit(EUnit::Artillery);
	CHECK(Art.SplashRadius > 0.f);
	CHECK(Art.MinRange > 0.f && Art.MinRange < Art.Range);
	CHECK(Art.ProjectileSpeed > 0.f);
	// у остальных сплеша нет
	CHECK(Unit(EUnit::Tank).SplashRadius == 0.f);

	// энергобаланс зданий: одна станция покрывает казармы + турель
	CHECK(Building(EBuilding::Power).PowerProduce >=
	      Building(EBuilding::Barracks).PowerUse + Building(EBuilding::Turret).PowerUse);

	// турель — defense-урон, не годится для штурма зданий
	CHECK(Building(EBuilding::Turret).DmgType == EDmg::Defense);
}

static void TestSplash()
{
	CHECK_NEAR(SplashFalloff(0.f, 120.f), 1.f, 1e-6f);
	CHECK(SplashFalloff(120.f, 120.f) >= 0.45f - 1e-6f);
	CHECK(SplashFalloff(140.f, 120.f) > 0.f);   // в пределах зоны ×1.2 (144)
	CHECK(SplashFalloff(150.f, 120.f) == 0.f);  // за пределами зоны
	CHECK(SplashFalloff(50.f, 0.f) == 0.f);     // без сплеша — только прямое попадание
	// монотонность затухания
	CHECK(SplashFalloff(10.f, 120.f) >= SplashFalloff(60.f, 120.f));
	CHECK(SplashFalloff(60.f, 120.f) >= SplashFalloff(119.f, 120.f));
}

static void TestDifficulty()
{
	CHECK(Difficulty(EDifficulty::Easy).IncomeMul < Difficulty(EDifficulty::Hard).IncomeMul);
	CHECK(Difficulty(EDifficulty::Easy).ArmyCap < Difficulty(EDifficulty::Hard).ArmyCap);
	// первая стычка в окне 3–5 минут: волна стартует за 150–240 c (плюс дорога)
	for (uint8_t d = 0; d < 3; ++d)
	{
		CHECK(DifficultyStats[d].FirstWaveAt >= 150.f && DifficultyStats[d].FirstWaveAt <= 240.f);
	}
}

int main()
{
	TestDamageMatrix();
	TestFactions();
	TestEffectiveDamage();
	TestEconomy();
	TestUnitsAndBuildings();
	TestSplash();
	TestDifficulty();

	std::printf("%s: %d/%d проверок пройдено\n", GFailed == 0 ? "OK" : "FAILED", GTotal - GFailed, GTotal);
	return GFailed == 0 ? 0 : 1;
}
