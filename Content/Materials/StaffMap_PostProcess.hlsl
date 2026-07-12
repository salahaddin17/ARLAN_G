// ============================================================================
// БОНУС (черновик): пост-процесс «штабная карта» для Custom-ноды материала.
//
// Как подключить (редактор):
// 1. Content Browser → Materials → правый клик → Material: M_StaffMapPP,
//    Material Domain = Post Process, Blendable Location = After Tonemapping.
// 2. Добавить ноду Custom (HLSL), вставить код ниже, Output Type = CMOT Float3,
//    входы: UV (TexCoord[0]).
// 3. Custom → Emissive Color; материал → в PostProcessVolume (Infinite
//    Extent) на TestMap, Post Process Materials → добавить M_StaffMapPP.
//
// Эффект: бумажный тон + лёгкая сепия, тактильная сетка карты 1 клетка =
// 100 UU (через SceneDepth восстанавливать мировые координаты в черновике
// не будем — сетку даёт экранная проекция), виньетка и зерно бумаги.
// ============================================================================

float3 Scene = SceneTextureLookup(UV, 14, false).rgb; // 14 = PostProcessInput0

// --- сепия к бумаге #c8bb8f -------------------------------------------------
float Luma = dot(Scene, float3(0.299, 0.587, 0.114));
float3 Paper = float3(0.784, 0.733, 0.561);
float3 Ink   = float3(0.149, 0.133, 0.110);
// светлое уходит в бумагу, тёмное — в чернила
float3 Toned = lerp(Ink, Paper, saturate(Luma * 1.15));
// сохранить немного исходного цвета (фракционные фишки должны читаться)
float3 Color = lerp(Toned, Scene, 0.55);

// --- зерно бумаги --------------------------------------------------------------
float Grain = frac(sin(dot(UV * 913.0, float2(12.9898, 78.233))) * 43758.5453);
Color += (Grain - 0.5) * 0.025;

// --- виньетка -------------------------------------------------------------------
float2 Center = UV - 0.5;
float Vignette = 1.0 - dot(Center, Center) * 0.55;
Color *= Vignette;

// --- экранная «сетка планшета» (тонкая, каждые ~64 пикселя) ---------------------
float2 GridUV = UV * float2(30.0, 17.0);
float2 GridDist = abs(frac(GridUV) - 0.5);
float GridLine = 1.0 - smoothstep(0.47, 0.5, max(GridDist.x, GridDist.y));
Color = lerp(Color, Color * 0.93, (1.0 - GridLine) * 0.0 + step(0.498, max(GridDist.x, GridDist.y)) * 0.06);

return Color;
