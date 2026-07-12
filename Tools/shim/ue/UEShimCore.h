// ============================================================================
// МИНИ-ШИМ Unreal Engine для псевдосборки clang'ом БЕЗ установленного UE.
// Цель: ловить ошибки C++ в коде модуля (типы, сигнатуры, шаблоны, const).
// Это НЕ Unreal: поведение заглушено, важна только компилируемость.
// Список правил: держать API-сигнатуры максимально близко к UE 5.5;
// код игры не должен подстраиваться под шим — если что-то не покрыто,
// расширяем шим, а не упрощаем игровой код.
// ============================================================================
#pragma once

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

// --- базовые типы -----------------------------------------------------------

using int8 = int8_t;   using uint8 = uint8_t;
using int16 = int16_t; using uint16 = uint16_t;
using int32 = int32_t; using uint32 = uint32_t;
using int64 = int64_t; using uint64 = uint64_t;
using TCHAR = char;

#define TEXT(x) x
#define RUBEZHARLAN_API
#define UE_API

constexpr int32 INDEX_NONE = -1;

// --- макросы рефлексии (пустышки) --------------------------------------------

#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UMETA(...)
#define UPARAM(...)

class UClass {};
class UScriptStruct {};

#define GENERATED_BODY() \
public: \
	static ::UClass* StaticClass() { static ::UClass C; return &C; } \
	static ::UScriptStruct* StaticStruct() { static ::UScriptStruct S; return &S; } \
public:

#define DECLARE_LOG_CATEGORY_EXTERN(Cat, Default, Verbosity) extern int Cat
#define DEFINE_LOG_CATEGORY(Cat) int Cat = 0
#define UE_LOG(...) ((void)0)

#define check(x) ((void)(x))
#define checkf(x, ...) ((void)(x))
#define ensure(x) (!!(x))
#define ensureMsgf(x, ...) (!!(x))

// --- строки -------------------------------------------------------------------

class FString
{
public:
	std::string S;
	FString() {}
	FString(const char* In) : S(In ? In : "") {}
	FString(const std::string& In) : S(In) {}
	const char* operator*() const { return S.c_str(); }
	bool IsEmpty() const { return S.empty(); }
	int32 Len() const { return int32(S.size()); }
	FString operator+(const FString& O) const { return FString(S + O.S); }
	FString& operator+=(const FString& O) { S += O.S; return *this; }
	bool operator==(const FString& O) const { return S == O.S; }
	template <typename... TArgs>
	static FString Printf(const char* Fmt, TArgs...) { return FString(Fmt); }
	static FString FromInt(int32 V) { return FString(std::to_string(V)); }
	static FString SanitizeFloat(double V) { return FString(std::to_string(V)); }
};

inline FString operator/(const FString& A, const char* B) { return FString(A.S + "/" + B); }
inline FString operator/(const FString& A, const FString& B) { return FString(A.S + "/" + B.S); }

class FName
{
public:
	std::string S;
	FName() {}
	FName(const char* In) : S(In ? In : "") {}
	FName(const FString& In) : S(In.S) {}
	bool operator==(const FName& O) const { return S == O.S; }
	bool operator!=(const FName& O) const { return S != O.S; }
	bool operator<(const FName& O) const { return S < O.S; }
	bool IsNone() const { return S.empty(); }
	FString ToString() const { return FString(S); }
};
static const FName NAME_None;

class FText
{
public:
	FString S;
	static FText FromString(const FString& In) { FText T; T.S = In; return T; }
	FString ToString() const { return S; }
};

// --- математика ------------------------------------------------------------------

struct FVector2D
{
	float X = 0, Y = 0;
	FVector2D() {}
	FVector2D(float InX, float InY) : X(InX), Y(InY) {}
	FVector2D operator-(const FVector2D& O) const { return {X - O.X, Y - O.Y}; }
	FVector2D operator+(const FVector2D& O) const { return {X + O.X, Y + O.Y}; }
	FVector2D operator*(float K) const { return {X * K, Y * K}; }
	float Size() const { return std::sqrt(X * X + Y * Y); }
	static const FVector2D ZeroVector;
};
inline const FVector2D FVector2D::ZeroVector;

struct FVector
{
	double X = 0, Y = 0, Z = 0;
	FVector() {}
	FVector(double V) : X(V), Y(V), Z(V) {}
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	FVector operator+(const FVector& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
	FVector operator-(const FVector& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
	FVector operator*(double K) const { return {X * K, Y * K, Z * K}; }
	FVector operator/(double K) const { return {X / K, Y / K, Z / K}; }
	FVector& operator+=(const FVector& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	FVector& operator-=(const FVector& O) { X -= O.X; Y -= O.Y; Z -= O.Z; return *this; }
	bool operator==(const FVector& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
	double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	double Size2D() const { return std::sqrt(X * X + Y * Y); }
	double SizeSquared() const { return X * X + Y * Y + Z * Z; }
	bool IsNearlyZero(double Tol = 1e-4) const { return Size() < Tol; }
	FVector GetSafeNormal(double Tol = 1e-8) const
	{
		const double L = Size();
		return L > Tol ? FVector(X / L, Y / L, Z / L) : FVector();
	}
	FVector GetSafeNormal2D(double Tol = 1e-8) const
	{
		const double L = Size2D();
		return L > Tol ? FVector(X / L, Y / L, 0) : FVector();
	}
	static double Dist(const FVector& A, const FVector& B) { return (A - B).Size(); }
	static double Dist2D(const FVector& A, const FVector& B) { return (A - B).Size2D(); }
	static double DistSquared(const FVector& A, const FVector& B) { return (A - B).SizeSquared(); }
	FString ToString() const { return FString("V"); }
	static const FVector ZeroVector;
	static const FVector UpVector;
	static const FVector ForwardVector;
	static const FVector RightVector;
	static const FVector OneVector;
};
inline const FVector FVector::ZeroVector;
inline const FVector FVector::UpVector(0, 0, 1);
inline const FVector FVector::ForwardVector(1, 0, 0);
inline const FVector FVector::RightVector(0, 1, 0);
inline const FVector FVector::OneVector(1, 1, 1);

struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	FRotator() {}
	FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	FVector Vector() const
	{
		const double P = Pitch * 3.14159265358979 / 180.0, Yw = Yaw * 3.14159265358979 / 180.0;
		return FVector(std::cos(P) * std::cos(Yw), std::cos(P) * std::sin(Yw), std::sin(P));
	}
	static const FRotator ZeroRotator;
};
inline const FRotator FRotator::ZeroRotator;

struct FLinearColor
{
	float R = 0, G = 0, B = 0, A = 1;
	FLinearColor() {}
	FLinearColor(float InR, float InG, float InB, float InA = 1.f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor operator*(float K) const { return {R * K, G * K, B * K, A}; }
	static const FLinearColor White, Black, Red, Green, Blue, Yellow, Gray;
};
inline const FLinearColor FLinearColor::White(1, 1, 1);
inline const FLinearColor FLinearColor::Black(0, 0, 0);
inline const FLinearColor FLinearColor::Red(1, 0, 0);
inline const FLinearColor FLinearColor::Green(0, 1, 0);
inline const FLinearColor FLinearColor::Blue(0, 0, 1);
inline const FLinearColor FLinearColor::Yellow(1, 1, 0);
inline const FLinearColor FLinearColor::Gray(0.5f, 0.5f, 0.5f);

struct FColor
{
	uint8 R = 0, G = 0, B = 0, A = 255;
	FColor() {}
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : R(InR), G(InG), B(InB), A(InA) {}
	static const FColor White, Black, Red, Green, Blue, Yellow, Orange, Cyan, Silver, Emerald;
};
inline const FColor FColor::White(255, 255, 255);
inline const FColor FColor::Black(0, 0, 0);
inline const FColor FColor::Red(255, 0, 0);
inline const FColor FColor::Green(0, 255, 0);
inline const FColor FColor::Blue(0, 0, 255);
inline const FColor FColor::Yellow(255, 255, 0);
inline const FColor FColor::Orange(243, 156, 18);
inline const FColor FColor::Cyan(0, 255, 255);
inline const FColor FColor::Silver(189, 195, 199);
inline const FColor FColor::Emerald(46, 204, 113);

struct FMath
{
	template <typename T> static T Clamp(T V, T Lo, T Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Abs(T V) { return V < T(0) ? -V : V; }
	template <typename T> static T Square(T V) { return V * V; }
	template <typename T, typename U> static T Lerp(T A, T B, U Alpha) { return T(A + (B - A) * Alpha); }
	static float Sqrt(float V) { return std::sqrt(V); }
	static float Pow(float A, float B) { return std::pow(A, B); }
	static float Exp(float V) { return std::exp(V); }
	static float Sin(float V) { return std::sin(V); }
	static float Cos(float V) { return std::cos(V); }
	static float Atan2(float A, float B) { return std::atan2(A, B); }
	static float Fmod(float A, float B) { return std::fmod(A, B); }
	static int32 RoundToInt(float V) { return int32(std::lround(V)); }
	static int32 FloorToInt(float V) { return int32(std::floor(V)); }
	static int32 CeilToInt(float V) { return int32(std::ceil(V)); }
	static float DegreesToRadians(float D) { return D * 3.1415926535f / 180.f; }
	static float RadiansToDegrees(float R) { return R * 180.f / 3.1415926535f; }
	static bool IsNearlyZero(float V, float Tol = 1e-4f) { return Abs(V) < Tol; }
	static bool IsNearlyEqual(float A, float B, float Tol = 1e-4f) { return Abs(A - B) < Tol; }
	static int32 RandRange(int32 Lo, int32 Hi) { return Lo + (Hi > Lo ? std::rand() % (Hi - Lo + 1) : 0); }
	static float RandRange(float Lo, float Hi) { return Lo + (Hi - Lo) * FRand(); }
	static float FRandRange(float Lo, float Hi) { return RandRange(Lo, Hi); }
	static float FRand() { return float(std::rand()) / float(RAND_MAX); }
	static bool RandBool() { return (std::rand() & 1) != 0; }
};

// --- контейнеры ---------------------------------------------------------------------

template <typename T>
class TArray
{
public:
	std::vector<T> V;
	int32 Num() const { return int32(V.size()); }
	bool IsEmpty() const { return V.empty(); }
	int32 Add(const T& Item) { V.push_back(Item); return Num() - 1; }
	int32 AddUnique(const T& Item)
	{
		const int32 Found = Find(Item);
		return Found != INDEX_NONE ? Found : Add(Item);
	}
	template <typename... TArgs> T& Emplace(TArgs&&... Args)
	{
		V.emplace_back(std::forward<TArgs>(Args)...);
		return V.back();
	}
	void Append(const TArray<T>& O) { V.insert(V.end(), O.V.begin(), O.V.end()); }
	T& operator[](int32 I) { return V[size_t(I)]; }
	const T& operator[](int32 I) const { return V[size_t(I)]; }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
	T& Last() { return V.back(); }
	const T& Last() const { return V.back(); }
	T Pop() { T Item = V.back(); V.pop_back(); return Item; }
	void RemoveAt(int32 I) { V.erase(V.begin() + I); }
	void RemoveAtSwap(int32 I) { std::swap(V[size_t(I)], V.back()); V.pop_back(); }
	int32 Remove(const T& Item)
	{
		const auto It = std::remove(V.begin(), V.end(), Item);
		const int32 N = int32(V.end() - It);
		V.erase(It, V.end());
		return N;
	}
	template <typename TPred> int32 RemoveAll(TPred Pred)
	{
		const auto It = std::remove_if(V.begin(), V.end(), Pred);
		const int32 N = int32(V.end() - It);
		V.erase(It, V.end());
		return N;
	}
	bool Contains(const T& Item) const { return std::find(V.begin(), V.end(), Item) != V.end(); }
	int32 Find(const T& Item) const
	{
		const auto It = std::find(V.begin(), V.end(), Item);
		return It == V.end() ? INDEX_NONE : int32(It - V.begin());
	}
	template <typename TPred> T* FindByPredicate(TPred Pred)
	{
		const auto It = std::find_if(V.begin(), V.end(), Pred);
		return It == V.end() ? nullptr : &*It;
	}
	template <typename TPred> const T* FindByPredicate(TPred Pred) const
	{
		const auto It = std::find_if(V.begin(), V.end(), Pred);
		return It == V.end() ? nullptr : &*It;
	}
	template <typename TPred> void Sort(TPred Pred) { std::sort(V.begin(), V.end(), Pred); }
	void Reset(int32 = 0) { V.clear(); }
	void Empty(int32 = 0) { V.clear(); }
	void Reserve(int32 N) { V.reserve(size_t(N)); }
	void SetNum(int32 N) { V.resize(size_t(N)); }
	void Init(const T& Item, int32 N) { V.assign(size_t(N), Item); }
	auto begin() { return V.begin(); }
	auto end() { return V.end(); }
	auto begin() const { return V.begin(); }
	auto end() const { return V.end(); }
};

template <typename K, typename V>
struct TPair
{
	K Key;
	V Value;
};

template <typename K, typename V>
class TMap
{
public:
	std::vector<TPair<K, V>> P;
	V& Add(const K& Key, const V& Val)
	{
		if (V* Found = Find(Key)) { *Found = Val; return *Found; }
		P.push_back({Key, Val});
		return P.back().Value;
	}
	V& FindOrAdd(const K& Key)
	{
		if (V* Found = Find(Key)) { return *Found; }
		P.push_back({Key, V{}});
		return P.back().Value;
	}
	V* Find(const K& Key)
	{
		for (auto& Pair : P) { if (Pair.Key == Key) return &Pair.Value; }
		return nullptr;
	}
	const V* Find(const K& Key) const
	{
		for (const auto& Pair : P) { if (Pair.Key == Key) return &Pair.Value; }
		return nullptr;
	}
	V FindRef(const K& Key) const
	{
		const V* Found = Find(Key);
		return Found ? *Found : V{};
	}
	bool Contains(const K& Key) const { return Find(Key) != nullptr; }
	int32 Remove(const K& Key)
	{
		const auto It = std::remove_if(P.begin(), P.end(), [&](const TPair<K, V>& Pair) { return Pair.Key == Key; });
		const int32 N = int32(P.end() - It);
		P.erase(It, P.end());
		return N;
	}
	int32 Num() const { return int32(P.size()); }
	void Empty(int32 = 0) { P.clear(); }
	auto begin() { return P.begin(); }
	auto end() { return P.end(); }
	auto begin() const { return P.begin(); }
	auto end() const { return P.end(); }
};

template <typename T>
class TSet
{
public:
	std::vector<T> V;
	void Add(const T& Item) { if (!Contains(Item)) V.push_back(Item); }
	bool Contains(const T& Item) const { return std::find(V.begin(), V.end(), Item) != V.end(); }
	int32 Remove(const T& Item)
	{
		const auto It = std::remove(V.begin(), V.end(), Item);
		const int32 N = int32(V.end() - It);
		V.erase(It, V.end());
		return N;
	}
	int32 Num() const { return int32(V.size()); }
	void Empty(int32 = 0) { V.clear(); }
	auto begin() { return V.begin(); }
	auto end() { return V.end(); }
	auto begin() const { return V.begin(); }
	auto end() const { return V.end(); }
};

// --- объектная модель ------------------------------------------------------------------

class UWorld;
class UGameInstance;
class AActor;

class UObject
{
public:
	virtual ~UObject() {}
	virtual UWorld* GetWorld() const { return nullptr; }
	FString GetName() const { return FString("Object"); }
	FName GetFName() const { return FName("Object"); }
};

inline bool IsValid(const UObject* Obj) { return Obj != nullptr; }

template <typename T>
T* NewObject(UObject* /*Outer*/ = nullptr) { return new T(); }

template <typename T>
T* LoadObject(UObject*, const TCHAR*) { return nullptr; }

template <typename T>
T* Cast(UObject* Obj) { return dynamic_cast<T*>(Obj); }
template <typename T>
const T* Cast(const UObject* Obj) { return dynamic_cast<const T*>(Obj); }
template <typename T>
T* CastChecked(UObject* Obj) { return dynamic_cast<T*>(Obj); }

template <typename T>
struct TWeakObjectPtr
{
	T* Ptr = nullptr;
	TWeakObjectPtr() {}
	TWeakObjectPtr(T* In) : Ptr(In) {}
	bool IsValid() const { return Ptr != nullptr; }
	T* Get() const { return Ptr; }
	T* operator->() const { return Ptr; }
	TWeakObjectPtr& operator=(T* In) { Ptr = In; return *this; }
	bool operator==(const TWeakObjectPtr& O) const { return Ptr == O.Ptr; }
	void Reset() { Ptr = nullptr; }
};

template <typename T> using TObjectPtr = T*;

struct TSubclassOfBase {};
template <typename T>
struct TSubclassOf
{
	UClass* C = nullptr;
	TSubclassOf() {}
	TSubclassOf(UClass* In) : C(In) {}
	TSubclassOf& operator=(UClass* In) { C = In; return *this; }
	operator UClass*() const { return C; }
	explicit operator bool() const { return C != nullptr; }
};

// --- ключи ввода ---------------------------------------------------------------------------

struct FKey
{
	FName Name;
	FKey() {}
	FKey(const char* In) : Name(In) {}
	bool operator==(const FKey& O) const { return Name == O.Name; }
};

namespace EKeys
{
	inline const FKey LeftMouseButton("LeftMouseButton");
	inline const FKey RightMouseButton("RightMouseButton");
	inline const FKey MiddleMouseButton("MiddleMouseButton");
	inline const FKey MouseScrollUp("MouseScrollUp");
	inline const FKey MouseScrollDown("MouseScrollDown");
	inline const FKey W("W"); inline const FKey A("A"); inline const FKey S("S"); inline const FKey D("D");
	inline const FKey Q("Q"); inline const FKey E("E"); inline const FKey R("R"); inline const FKey T("T");
	inline const FKey F("F"); inline const FKey H("H"); inline const FKey B("B");
	inline const FKey One("One"); inline const FKey Two("Two"); inline const FKey Three("Three");
	inline const FKey Four("Four"); inline const FKey Five("Five"); inline const FKey Six("Six");
	inline const FKey Seven("Seven"); inline const FKey Eight("Eight"); inline const FKey Nine("Nine");
	inline const FKey Zero("Zero");
	inline const FKey Escape("Escape"); inline const FKey SpaceBar("SpaceBar");
	inline const FKey LeftShift("LeftShift"); inline const FKey RightShift("RightShift");
	inline const FKey LeftControl("LeftControl"); inline const FKey RightControl("RightControl");
	inline const FKey Up("Up"); inline const FKey Down("Down");
	inline const FKey Left("Left"); inline const FKey Right("Right");
}

enum EInputEvent { IE_Pressed = 0, IE_Released = 1, IE_Repeat = 2, IE_DoubleClick = 3, IE_Axis = 4 };

// --- компоненты -----------------------------------------------------------------------------

struct FActorComponentTickFunction { bool bCanEverTick = false; float TickInterval = 0.f; };
struct FActorTickFunction { bool bCanEverTick = false; float TickInterval = 0.f; };
enum ELevelTick { LEVELTICK_All };
namespace EEndPlayReason { enum Type { Destroyed, LevelTransition, EndPlayInEditor, RemovedFromWorld, Quit }; }

class UActorComponent : public UObject
{
public:
	FActorComponentTickFunction PrimaryComponentTick;
	AActor* Owner = nullptr;
	AActor* GetOwner() const { return Owner; }
	virtual void BeginPlay() {}
	virtual void TickComponent(float, ELevelTick, FActorComponentTickFunction*) {}
	virtual void EndPlay(EEndPlayReason::Type) {}
	void SetComponentTickEnabled(bool) {}
	void RegisterComponent() {}
	UWorld* GetWorld() const override;
};

class USceneComponent : public UActorComponent
{
public:
	void SetupAttachment(USceneComponent*, FName = NAME_None) {}
	void SetRelativeLocation(const FVector&) {}
	void SetRelativeRotation(const FRotator&) {}
	void SetRelativeScale3D(const FVector&) {}
	void SetWorldLocation(const FVector&) {}
	void SetWorldScale3D(const FVector&) {}
	FVector GetComponentLocation() const { return FVector(); }
	void SetVisibility(bool, bool = false) {}
};

namespace ECollisionEnabled { enum Type { NoCollision, QueryOnly, PhysicsOnly, QueryAndPhysics }; }
enum ECollisionChannel { ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn, ECC_Visibility, ECC_Camera };
enum class ECollisionResponse { ECR_Ignore, ECR_Overlap, ECR_Block };

class UPrimitiveComponent : public USceneComponent
{
public:
	void SetCollisionEnabled(ECollisionEnabled::Type) {}
	void SetCollisionProfileName(FName, bool = false) {}
	void SetCollisionResponseToChannel(ECollisionChannel, ECollisionResponse) {}
	void SetCollisionResponseToAllChannels(ECollisionResponse) {}
	void SetGenerateOverlapEvents(bool) {}
	void SetCanEverAffectNavigation(bool) {}
	void SetCastShadow(bool) {}
};

class UStaticMesh : public UObject {};
class UMaterialInterface : public UObject {};
class UMaterial : public UMaterialInterface {};
class UFont : public UObject {};

class UMaterialInstanceDynamic : public UMaterialInterface
{
public:
	static UMaterialInstanceDynamic* Create(UMaterialInterface*, UObject*) { return new UMaterialInstanceDynamic(); }
	void SetVectorParameterValue(FName, const FLinearColor&) {}
	void SetScalarParameterValue(FName, float) {}
};

class UMeshComponent : public UPrimitiveComponent
{
public:
	void SetMaterial(int32, UMaterialInterface*) {}
	UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(int32) { return new UMaterialInstanceDynamic(); }
};

class UStaticMeshComponent : public UMeshComponent
{
public:
	bool SetStaticMesh(UStaticMesh*) { return true; }
};

class UCapsuleComponent : public UPrimitiveComponent
{
public:
	void SetCapsuleSize(float, float, bool = true) {}
	void SetCapsuleHalfHeight(float, bool = true) {}
	void SetCapsuleRadius(float, bool = true) {}
	float GetScaledCapsuleRadius() const { return 40.f; }
	float GetScaledCapsuleHalfHeight() const { return 90.f; }
};

class USkeletalMeshComponent : public UMeshComponent {};

class USpringArmComponent : public USceneComponent
{
public:
	inline static const FName SocketName = FName("SpringEndpoint");
	float TargetArmLength = 300.f;
	bool bDoCollisionTest = true;
	bool bEnableCameraLag = false;
	float CameraLagSpeed = 10.f;
	bool bInheritPitch = true, bInheritYaw = true, bInheritRoll = true;
};

class UCameraComponent : public USceneComponent
{
public:
	void SetFieldOfView(float) {}
	float FieldOfView = 90.f;
};

class UCharacterMovementComponent : public UActorComponent
{
public:
	float MaxWalkSpeed = 600.f;
	float MaxAcceleration = 2048.f;
	bool bOrientRotationToMovement = false;
	bool bUseRVOAvoidance = false;
	float AvoidanceConsiderationRadius = 500.f;
	FRotator RotationRate;
	void SetAvoidanceEnabled(bool) {}
};

class UInputComponent : public UActorComponent
{
public:
	template <typename UserClass>
	int32 BindKey(const FKey&, EInputEvent, UserClass*, void (UserClass::*)()) { return 0; }
};

// --- акторы ------------------------------------------------------------------------------------

enum class ESpawnActorCollisionHandlingMethod : uint8
{
	Undefined, AlwaysSpawn, AdjustIfPossibleButAlwaysSpawn, AdjustIfPossibleButDontSpawnIfColliding, DontSpawnIfColliding,
};

class APawn;
struct FActorSpawnParameters
{
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined;
	AActor* Owner = nullptr;
	APawn* Instigator = nullptr;
};

class AActor : public UObject
{
public:
	FActorTickFunction PrimaryActorTick;
	USceneComponent* RootComponent = nullptr;
	UWorld* WorldPtr = nullptr;

	virtual void BeginPlay() {}
	virtual void Tick(float) {}
	virtual void EndPlay(const EEndPlayReason::Type) {}
	virtual void PostInitializeComponents() {}
	virtual bool Destroy() { return true; }
	bool IsActorBeingDestroyed() const { return false; }

	FVector GetActorLocation() const { return ShimActorLocation; }
	bool SetActorLocation(const FVector& In) { ShimActorLocation = In; return true; }
	FRotator GetActorRotation() const { return ShimActorRotation; }
	bool SetActorRotation(const FRotator& In) { ShimActorRotation = In; return true; }
	FVector GetActorForwardVector() const { return ShimActorRotation.Vector(); }
	FVector GetVelocity() const { return FVector(); }
	void SetActorHiddenInGame(bool bHidden) { bShimHiddenInGame = bHidden; }
	bool IsHidden() const { return bShimHiddenInGame; }
	void SetActorEnableCollision(bool) {}
	void SetActorTickEnabled(bool) {}
	void SetLifeSpan(float) {}
	void SetRootComponent(USceneComponent* In) { RootComponent = In; }
	USceneComponent* GetRootComponent() const { return RootComponent; }
	UWorld* GetWorld() const override { return WorldPtr; }
	float GetGameTimeSinceCreation() const { return 0.f; }

	template <typename T>
	T* CreateDefaultSubobject(FName)
	{
		T* C = new T();
		C->Owner = this;
		return C;
	}
	template <typename T>
	T* FindComponentByClass() const { return nullptr; }

	// имена нарочно «шимовые»: в реальном AActor таких публичных полей нет,
	// и они не должны конфликтовать с параметрами игрового кода (-Wshadow-field)
	FVector ShimActorLocation;
	FRotator ShimActorRotation;
	bool bShimHiddenInGame = false;
};

inline UWorld* UActorComponent::GetWorld() const { return Owner ? Owner->GetWorld() : nullptr; }

enum class EAutoPossessAI : uint8 { Disabled, PlacedInWorld, Spawned, PlacedInWorldOrSpawned };

class AController;

class APawn : public AActor
{
public:
	TSubclassOf<AController> AIControllerClass;
	EAutoPossessAI AutoPossessAI = EAutoPossessAI::Disabled;
	AController* Controller = nullptr;
	AController* GetController() const { return Controller; }
	virtual void PossessedBy(AController* C) { Controller = C; }
	virtual void UnPossessed() { Controller = nullptr; }
	void SpawnDefaultController() {}
	virtual void SetupPlayerInputComponent(UInputComponent*) {}
};

class ACharacter : public APawn
{
public:
	ACharacter()
	{
		CapsuleComp = new UCapsuleComponent();
		MoveComp = new UCharacterMovementComponent();
		MeshComp = new USkeletalMeshComponent();
	}
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComp; }
	UCharacterMovementComponent* GetCharacterMovement() const { return MoveComp; }
	USkeletalMeshComponent* GetMesh() const { return MeshComp; }
	UCapsuleComponent* CapsuleComp;
	UCharacterMovementComponent* MoveComp;
	USkeletalMeshComponent* MeshComp;
};

class AController : public AActor
{
public:
	APawn* PawnPtr = nullptr;
	APawn* GetPawn() const { return PawnPtr; }
	void Possess(APawn* P) { PawnPtr = P; OnPossess(P); }
	virtual void OnPossess(APawn* P) { PawnPtr = P; }
	virtual void OnUnPossess() { PawnPtr = nullptr; }
};

namespace EPathFollowingRequestResult { enum Type { Failed, AlreadyAtGoal, RequestSuccessful }; }

class AAIController : public AController
{
public:
	EPathFollowingRequestResult::Type MoveToLocation(
		const FVector&, float AcceptanceRadius = -1.f, bool bStopOnOverlap = true,
		bool bUsePathfinding = true, bool bProjectDestinationToNavigation = false,
		bool bCanStrafe = true, void* FilterClass = nullptr, bool bAllowPartialPath = true)
	{
		return EPathFollowingRequestResult::RequestSuccessful;
	}
	EPathFollowingRequestResult::Type MoveToActor(
		AActor*, float AcceptanceRadius = -1.f, bool bStopOnOverlap = true,
		bool bUsePathfinding = true, bool bCanStrafe = true,
		void* FilterClass = nullptr, bool bAllowPartialPath = true)
	{
		return EPathFollowingRequestResult::RequestSuccessful;
	}
	void StopMovement() {}
};

enum class EMouseLockMode { DoNotLock, LockOnCapture, LockAlways, LockInFullscreen };
struct FInputModeGameAndUI
{
	FInputModeGameAndUI& SetHideCursorDuringCapture(bool) { return *this; }
	FInputModeGameAndUI& SetLockMouseToViewportBehavior(EMouseLockMode) { return *this; }
};
struct FInputModeGameOnly {};

class APlayerController : public AController
{
public:
	bool bShowMouseCursor = false;
	bool bEnableClickEvents = false;
	bool bEnableMouseOverEvents = false;
	UInputComponent* InputComponent = nullptr;
	virtual void SetupInputComponent() { InputComponent = new UInputComponent(); }
	virtual void PlayerTick(float) {}
	bool GetMousePosition(float& X, float& Y) const { X = 0; Y = 0; return true; }
	bool DeprojectMousePositionToWorld(FVector&, FVector&) const { return true; }
	bool DeprojectScreenPositionToWorld(float, float, FVector&, FVector&) const { return true; }
	bool ProjectWorldLocationToScreen(const FVector&, FVector2D&, bool = false) const { return true; }
	bool IsInputKeyDown(const FKey&) const { return false; }
	bool WasInputKeyJustPressed(const FKey&) const { return false; }
	void GetViewportSize(int32& X, int32& Y) const { X = 1920; Y = 1080; }
	void SetInputMode(const FInputModeGameAndUI&) {}
	void SetInputMode(const FInputModeGameOnly&) {}
};

class UCanvas : public UObject
{
public:
	float SizeX = 1920.f, SizeY = 1080.f;
	float ClipX = 1920.f, ClipY = 1080.f;
};

class AHUD : public AActor
{
public:
	APlayerController* PlayerOwner = nullptr;
	UCanvas* Canvas = nullptr;
	virtual void DrawHUD() {}
	void DrawRect(const FLinearColor&, float, float, float, float) {}
	void DrawLine(float, float, float, float, FLinearColor, float = 0.f) {}
	void DrawText(const FString&, FLinearColor, float, float, UFont* = nullptr, float = 1.f, bool = false) {}
};

class AGameModeBase : public AActor
{
public:
	TSubclassOf<APawn> DefaultPawnClass;
	TSubclassOf<APlayerController> PlayerControllerClass;
	TSubclassOf<AHUD> HUDClass;
	virtual void StartPlay() {}
	virtual void InitGame(const FString&, const FString&, FString&) {}
};

class APlayerStart : public AActor {};

// --- мир и подсистемы ------------------------------------------------------------------------------

class UGameInstance : public UObject
{
public:
	template <typename T> T* GetSubsystem() const { static T* I = new T(); return I; }
};

class UWorld : public UObject
{
public:
	template <typename T>
	T* SpawnActor(const FVector& Loc, const FRotator& Rot, const FActorSpawnParameters& = FActorSpawnParameters())
	{
		T* A = new T();
		A->WorldPtr = const_cast<UWorld*>(this);
		A->SetActorLocation(Loc);
		A->SetActorRotation(Rot);
		return A;
	}
	template <typename T>
	T* SpawnActor(UClass*, const FVector& Loc, const FRotator& Rot, const FActorSpawnParameters& = FActorSpawnParameters())
	{
		return SpawnActor<T>(Loc, Rot);
	}
	template <typename T> T* GetSubsystem() const { static T* I = new T(); return I; }
	UGameInstance* GetGameInstance() const { static UGameInstance GI; return const_cast<UGameInstance*>(&GI); }
	AGameModeBase* GetAuthGameMode() const { return nullptr; }
	float GetTimeSeconds() const { return 0.f; }
	float GetDeltaSeconds() const { return 0.f; }
};

class FSubsystemCollectionBase {};

class USubsystem : public UObject
{
public:
	virtual void Initialize(FSubsystemCollectionBase&) {}
	virtual void Deinitialize() {}
};

class UGameInstanceSubsystem : public USubsystem
{
public:
	UGameInstance* GetGameInstance() const { return nullptr; }
};

class UWorldSubsystem : public USubsystem
{
public:
	UWorld* GetWorld() const override { return nullptr; }
};

// --- таблицы -------------------------------------------------------------------------------------------

struct FTableRowBase {};

class UDataTable : public UObject
{
public:
	UScriptStruct* RowStruct = nullptr;
	TArray<FString> CreateTableFromCSVString(const FString&) { return TArray<FString>(); }
	template <typename T>
	T* FindRow(FName, const FString&, bool = true) const { return nullptr; }
};

// --- утилиты --------------------------------------------------------------------------------------------

struct FPaths
{
	static FString ProjectContentDir() { return FString("Content"); }
	static FString ProjectDir() { return FString("."); }
};

struct FFileHelper
{
	static bool LoadFileToString(FString&, const TCHAR*) { return false; }
};

class UGameplayStatics
{
public:
	static APlayerController* GetPlayerController(const UObject*, int32) { return nullptr; }
	static AGameModeBase* GetGameMode(const UObject*) { return nullptr; }
	static void OpenLevel(const UObject*, FName, bool = true, FString = FString()) {}
};

namespace ConstructorHelpers
{
	template <typename T>
	struct FObjectFinder
	{
		T* Object = nullptr;
		explicit FObjectFinder(const TCHAR*) {}
		bool Succeeded() const { return false; }
	};
	template <typename T>
	struct FClassFinder
	{
		TSubclassOf<T> Class;
		explicit FClassFinder(const TCHAR*) {}
		bool Succeeded() const { return false; }
	};
}

class FDefaultGameModuleImpl {};
#define IMPLEMENT_PRIMARY_GAME_MODULE(ImplClass, ModuleName, GameName) \
	static int ModuleAnchor_##ModuleName = 0;

// --- отладочная отрисовка ---------------------------------------------------------------------------------

inline void DrawDebugLine(UWorld*, const FVector&, const FVector&, const FColor&,
	bool = false, float = -1.f, uint8 = 0, float = 0.f) {}
inline void DrawDebugSphere(UWorld*, const FVector&, float, int32, const FColor&,
	bool = false, float = -1.f, uint8 = 0, float = 0.f) {}
inline void DrawDebugBox(UWorld*, const FVector&, const FVector&, const FColor&,
	bool = false, float = -1.f, uint8 = 0, float = 0.f) {}
inline void DrawDebugCylinder(UWorld*, const FVector&, const FVector&, float, int32, const FColor&,
	bool = false, float = -1.f, uint8 = 0, float = 0.f) {}
inline void DrawDebugString(UWorld*, const FVector&, const FString&, AActor* = nullptr,
	const FColor& = FColor::White, float = -1.f, bool = false, float = 1.f) {}

// --- автоматизация ------------------------------------------------------------------------------------------

class FAutomationTestBase
{
public:
	virtual ~FAutomationTestBase() {}
	bool TestTrue(const FString&, bool V) { return V; }
	bool TestFalse(const FString&, bool V) { return !V; }
	template <typename A, typename B>
	bool TestEqual(const FString&, A Actual, B Expected) { return Actual == A(Expected); }
	template <typename A>
	bool TestNearlyEqual(const FString&, A Actual, A Expected, A Tol = A(1e-4)) { return FMath::Abs(Actual - Expected) <= Tol; }
	template <typename T>
	bool TestNotNull(const FString&, T* Ptr) { return Ptr != nullptr; }
	void AddError(const FString&) {}
};

namespace EAutomationTestFlags
{
	enum Flags
	{
		EditorContext = 1, ClientContext = 2, ApplicationContextMask = 3,
		SmokeFilter = 4, EngineFilter = 8, ProductFilter = 16,
	};
}

#define IMPLEMENT_SIMPLE_AUTOMATION_TEST(TClass, PrettyName, TFlags) \
	class TClass : public FAutomationTestBase \
	{ \
	public: \
		bool RunTest(const FString& Parameters); \
	}; \
	static TClass TClass##Instance;
