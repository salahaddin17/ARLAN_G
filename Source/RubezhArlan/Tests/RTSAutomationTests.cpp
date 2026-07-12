// ============================================================================
// Automation-тесты UE (запуск: Session Frontend → Automation, фильтр «Rubezh»,
// либо UnrealEditor-Cmd -ExecCmds="Automation RunTests Rubezh; Quit").
// Зеркалят Tools/logic_tests.cpp — единые числа с эталонным прототипом.
// ============================================================================

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Core/RTSBalanceCore.h"
#include "Data/RTSDataSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace RTSCore;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhDamageMatrixTest,
	"Rubezh.Balance.DamageMatrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRubezhDamageMatrixTest::RunTest(const FString& Parameters)
{
	// заполненность и пределы
	for (uint8 D = 0; D < uint8(EDmg::COUNT); ++D)
	{
		for (uint8 A = 0; A < uint8(EArmor::COUNT); ++A)
		{
			TestTrue(TEXT("множитель в (0..2]"), DamageMatrix[D][A] > 0.f && DamageMatrix[D][A] <= 2.f);
		}
	}
	// камень-ножницы-бумага
	TestTrue(TEXT("пули > пехота, слабее по тяж. броне"),
		DamageMultiplier(EDmg::Bullet, EArmor::INF) > DamageMultiplier(EDmg::Bullet, EArmor::HVY));
	TestTrue(TEXT("ракеты — лучший ответ тяж. броне"),
		DamageMultiplier(EDmg::Rocket, EArmor::HVY) > DamageMultiplier(EDmg::Bullet, EArmor::HVY));
	TestTrue(TEXT("снаряды ломают здания лучше всех"),
		DamageMultiplier(EDmg::Shell, EArmor::BLD) > DamageMultiplier(EDmg::Rocket, EArmor::BLD) &&
		DamageMultiplier(EDmg::Shell, EArmor::BLD) > DamageMultiplier(EDmg::Bullet, EArmor::BLD));
	TestTrue(TEXT("турели не штурмуют здания"),
		DamageMultiplier(EDmg::Defense, EArmor::BLD) <= 0.5f);
	// эталоны
	TestNearlyEqual(TEXT("bullet×INF"), DamageMultiplier(EDmg::Bullet, EArmor::INF), 1.25f, 1e-4f);
	TestNearlyEqual(TEXT("rocket×HVY"), DamageMultiplier(EDmg::Rocket, EArmor::HVY), 1.30f, 1e-4f);
	TestNearlyEqual(TEXT("shell×BLD"), DamageMultiplier(EDmg::Shell, EArmor::BLD), 1.25f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhFactionTest,
	"Rubezh.Balance.Factions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRubezhFactionTest::RunTest(const FString& Parameters)
{
	TestNearlyEqual(TEXT("Легион цена ×1.15"), Mods(EFaction::Legion).CostMul, 1.15f, 1e-4f);
	TestNearlyEqual(TEXT("Легион HP ×1.15"), Mods(EFaction::Legion).HpMul, 1.15f, 1e-4f);
	TestNearlyEqual(TEXT("Легион урон ×1.15"), Mods(EFaction::Legion).DamageMul, 1.15f, 1e-4f);
	TestNearlyEqual(TEXT("Фронт цена ×0.85"), Mods(EFaction::Front).CostMul, 0.85f, 1e-4f);
	TestNearlyEqual(TEXT("Фронт HP ×0.90"), Mods(EFaction::Front).HpMul, 0.90f, 1e-4f);
	TestTrue(TEXT("Фронт быстрее"), Mods(EFaction::Front).SpeedMul > Mods(EFaction::Legion).SpeedMul);

	TestEqual(TEXT("танк Легиона 368"), ScaledCost(Unit(EUnit::Tank).Cost, EFaction::Legion), 368);
	TestEqual(TEXT("танк Фронта 272"), ScaledCost(Unit(EUnit::Tank).Cost, EFaction::Front), 272);

	// фракционный бонус урона в полном конвейере
	TestNearlyEqual(TEXT("Легион 100 shell→HVY = 115"),
		EffectiveDamage(100.f, EFaction::Legion, EDmg::Shell, EArmor::HVY), 115.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhEconomyTest,
	"Rubezh.Economy.TruckCycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRubezhEconomyTest::RunTest(const FString& Parameters)
{
	// энергодефицит
	TestNearlyEqual(TEXT("дефицит ×0.5"), Econ::ProductionMultiplier(20, 10), 0.5f, 1e-4f);
	TestNearlyEqual(TEXT("баланс ×1"), Econ::ProductionMultiplier(10, 10), 1.0f, 1e-4f);

	// цикл грузовика: симуляция погрузки тиками
	float Cargo = 0.f, Depot = 100.f;
	const float Dt = 0.05f;
	int32 Ticks = 0;
	while (Cargo < Econ::TruckCapacity - 1e-3f && Ticks < 200)
	{
		const float Take = FMath::Min(Econ::LoadRate * Dt, FMath::Min(Econ::TruckCapacity - Cargo, Depot));
		Cargo += Take;
		Depot -= Take;
		++Ticks;
	}
	TestNearlyEqual(TEXT("полный кузов"), Cargo, Econ::TruckCapacity, 1e-3f);
	TestNearlyEqual(TEXT("склад уменьшился ровно на кузов"), Depot, 100.f - Econ::TruckCapacity, 1e-3f);
	TestTrue(TEXT("погрузка по расчётному времени"),
		Ticks <= int32(Econ::FullLoadTime() / Dt) + 2);

	// производственные цепочки
	TestTrue(TEXT("КЦ делает техника"), Produces(EBuilding::HQ, EUnit::Technician));
	TestTrue(TEXT("казармы делают ракетчика"), Produces(EBuilding::Barracks, EUnit::Rocketeer));
	TestTrue(TEXT("завод делает артиллерию"), Produces(EBuilding::Factory, EUnit::Artillery));
	TestFalse(TEXT("казармы НЕ делают танк"), Produces(EBuilding::Barracks, EUnit::Tank));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhSplashTest,
	"Rubezh.Combat.Splash",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRubezhSplashTest::RunTest(const FString& Parameters)
{
	TestNearlyEqual(TEXT("эпицентр = 1"), SplashFalloff(0.f, 120.f), 1.f, 1e-4f);
	TestTrue(TEXT("край зоны ≥ 0.45"), SplashFalloff(140.f, 120.f) >= 0.45f - 1e-4f);
	TestNearlyEqual(TEXT("вне зоны = 0"), SplashFalloff(150.f, 120.f), 0.f, 1e-4f);
	TestTrue(TEXT("затухание монотонно"),
		SplashFalloff(10.f, 120.f) >= SplashFalloff(60.f, 120.f) &&
		SplashFalloff(60.f, 120.f) >= SplashFalloff(119.f, 120.f));

	// параметры артиллерии
	const FUnitStats& Art = Unit(EUnit::Artillery);
	TestTrue(TEXT("у артиллерии есть сплеш и минимальная дальность"),
		Art.SplashRadius > 0.f && Art.MinRange > 0.f && Art.MinRange < Art.Range);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
