// ============================================================================
// Automation-тесты UE (запуск: Session Frontend → Automation, фильтр «Rubezh»,
// либо UnrealEditor-Cmd -ExecCmds="Automation RunTests Rubezh; Quit").
// Зеркалят Tools/logic_tests.cpp — единые числа с эталонным прототипом.
// ВАЖНО: RTSCore:: пишем полностью — глобальный using ловит конфликт с
// движковым enum class EUnit (Slate NumericTypeInterface).
// ============================================================================

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Core/RTSBalanceCore.h"
#include "Data/RTSDataSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhDamageMatrixTest,
	"Rubezh.Balance.DamageMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubezhDamageMatrixTest::RunTest(const FString& Parameters)
{
	using RTSCore::EDmg;
	using RTSCore::EArmor;

	// заполненность и пределы
	for (uint8 D = 0; D < uint8(EDmg::COUNT); ++D)
	{
		for (uint8 A = 0; A < uint8(EArmor::COUNT); ++A)
		{
			TestTrue(TEXT("множитель в (0..2]"),
				RTSCore::DamageMatrix[D][A] > 0.f && RTSCore::DamageMatrix[D][A] <= 2.f);
		}
	}
	// камень-ножницы-бумага
	TestTrue(TEXT("пули > пехота, слабее по тяж. броне"),
		RTSCore::DamageMultiplier(EDmg::Bullet, EArmor::INF) > RTSCore::DamageMultiplier(EDmg::Bullet, EArmor::HVY));
	TestTrue(TEXT("ракеты — лучший ответ тяж. броне"),
		RTSCore::DamageMultiplier(EDmg::Rocket, EArmor::HVY) > RTSCore::DamageMultiplier(EDmg::Bullet, EArmor::HVY));
	TestTrue(TEXT("снаряды ломают здания лучше всех"),
		RTSCore::DamageMultiplier(EDmg::Shell, EArmor::BLD) > RTSCore::DamageMultiplier(EDmg::Rocket, EArmor::BLD) &&
		RTSCore::DamageMultiplier(EDmg::Shell, EArmor::BLD) > RTSCore::DamageMultiplier(EDmg::Bullet, EArmor::BLD));
	TestTrue(TEXT("турели не штурмуют здания"),
		RTSCore::DamageMultiplier(EDmg::Defense, EArmor::BLD) <= 0.5f);
	// эталоны
	TestNearlyEqual(TEXT("bullet×INF"), RTSCore::DamageMultiplier(EDmg::Bullet, EArmor::INF), 1.25f, 1e-4f);
	TestNearlyEqual(TEXT("rocket×HVY"), RTSCore::DamageMultiplier(EDmg::Rocket, EArmor::HVY), 1.30f, 1e-4f);
	TestNearlyEqual(TEXT("shell×BLD"), RTSCore::DamageMultiplier(EDmg::Shell, EArmor::BLD), 1.25f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhFactionTest,
	"Rubezh.Balance.Factions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubezhFactionTest::RunTest(const FString& Parameters)
{
	using RTSCore::EFaction;

	TestNearlyEqual(TEXT("Легион цена ×1.15"), RTSCore::Mods(EFaction::Legion).CostMul, 1.15f, 1e-4f);
	TestNearlyEqual(TEXT("Легион HP ×1.15"), RTSCore::Mods(EFaction::Legion).HpMul, 1.15f, 1e-4f);
	TestNearlyEqual(TEXT("Легион урон ×1.15"), RTSCore::Mods(EFaction::Legion).DamageMul, 1.15f, 1e-4f);
	TestNearlyEqual(TEXT("Фронт цена ×0.85"), RTSCore::Mods(EFaction::Front).CostMul, 0.85f, 1e-4f);
	TestNearlyEqual(TEXT("Фронт HP ×0.90"), RTSCore::Mods(EFaction::Front).HpMul, 0.90f, 1e-4f);
	TestTrue(TEXT("Фронт быстрее"),
		RTSCore::Mods(EFaction::Front).SpeedMul > RTSCore::Mods(EFaction::Legion).SpeedMul);

	const int32 TankCost = RTSCore::Unit(RTSCore::EUnit::Tank).Cost;
	TestEqual(TEXT("танк Легиона 368"), RTSCore::ScaledCost(TankCost, EFaction::Legion), 368);
	TestEqual(TEXT("танк Фронта 272"), RTSCore::ScaledCost(TankCost, EFaction::Front), 272);

	// фракционный бонус урона в полном конвейере
	TestNearlyEqual(TEXT("Легион 100 shell→HVY = 115"),
		RTSCore::EffectiveDamage(100.f, EFaction::Legion, RTSCore::EDmg::Shell, RTSCore::EArmor::HVY),
		115.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhEconomyTest,
	"Rubezh.Economy.TruckCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubezhEconomyTest::RunTest(const FString& Parameters)
{
	namespace Econ = RTSCore::Econ;
	using RTSCore::EBuilding;

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
	TestTrue(TEXT("КЦ делает техника"), RTSCore::Produces(EBuilding::HQ, RTSCore::EUnit::Technician));
	TestTrue(TEXT("казармы делают ракетчика"), RTSCore::Produces(EBuilding::Barracks, RTSCore::EUnit::Rocketeer));
	TestTrue(TEXT("завод делает артиллерию"), RTSCore::Produces(EBuilding::Factory, RTSCore::EUnit::Artillery));
	TestFalse(TEXT("казармы НЕ делают танк"), RTSCore::Produces(EBuilding::Barracks, RTSCore::EUnit::Tank));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubezhSplashTest,
	"Rubezh.Combat.Splash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubezhSplashTest::RunTest(const FString& Parameters)
{
	TestNearlyEqual(TEXT("эпицентр = 1"), RTSCore::SplashFalloff(0.f, 120.f), 1.f, 1e-4f);
	TestTrue(TEXT("край зоны ≥ 0.45"), RTSCore::SplashFalloff(140.f, 120.f) >= 0.45f - 1e-4f);
	TestNearlyEqual(TEXT("вне зоны = 0"), RTSCore::SplashFalloff(150.f, 120.f), 0.f, 1e-4f);
	TestTrue(TEXT("затухание монотонно"),
		RTSCore::SplashFalloff(10.f, 120.f) >= RTSCore::SplashFalloff(60.f, 120.f) &&
		RTSCore::SplashFalloff(60.f, 120.f) >= RTSCore::SplashFalloff(119.f, 120.f));

	// параметры артиллерии
	const RTSCore::FUnitStats& Art = RTSCore::Unit(RTSCore::EUnit::Artillery);
	TestTrue(TEXT("у артиллерии есть сплеш и минимальная дальность"),
		Art.SplashRadius > 0.f && Art.MinRange > 0.f && Art.MinRange < Art.Range);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
