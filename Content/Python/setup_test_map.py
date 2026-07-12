# ============================================================================
# РУБЕЖ: АРЛАН — генерация тестовой карты (запускать в редакторе UE 5.5).
#
# Как запустить: Window → Output Log → сменить Cmd на Python →
#   py "Content/Python/setup_test_map.py"
# или: Tools → Execute Python Script… → выбрать этот файл.
#
# Создаёт /Game/Maps/TestMap: «бумажный» пол 64×64 клетки, NavMeshBoundsVolume
# на всю карту, свет, PlayerStart у базы игрока — и сохраняет уровень.
# Юниты/здания/склады спавнит сам ARTSGameMode в StartPlay — на карте их нет.
# ============================================================================

import unreal

MAP_PATH = "/Game/Maps/TestMap"
MAP_HALF = 3200.0          # RTSCore::MapHalfSize
PAPER = unreal.LinearColor(0.784, 0.733, 0.561, 1.0)   # #c8bb8f

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[РУБЕЖ] " + msg)


def make_paper_material():
    """Материал-инстанс бумажного тона поверх BasicShapeMaterial."""
    pkg_path, name = "/Game/Materials", "MI_PaperGround"
    full = pkg_path + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        return unreal.EditorAssetLibrary.load_asset(full)
    parent = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    factory = unreal.MaterialInstanceConstantFactoryNew()
    factory.set_editor_property("initial_parent", parent)
    mi = asset_tools.create_asset(name, pkg_path, unreal.MaterialInstanceConstant, factory)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", PAPER)
    unreal.EditorAssetLibrary.save_asset(full)
    return mi


def spawn(cls, loc, rot=unreal.Rotator(0.0, 0.0, 0.0)):
    return eas.spawn_actor_from_class(cls, loc, rot)


# HLSL «штабной карты» для Custom-ноды: вход SceneColor (float3) и UV (float2).
STAFF_MAP_HLSL = """
float Luma = dot(SceneColor, float3(0.299, 0.587, 0.114));
float3 Paper = float3(0.784, 0.733, 0.561);
float3 Ink = float3(0.149, 0.133, 0.110);
float3 Toned = lerp(Ink, Paper, saturate(Luma * 1.15));
float3 Color = lerp(Toned, SceneColor, 0.55);
float Grain = frac(sin(dot(UV * 913.0, float2(12.9898, 78.233))) * 43758.5453);
Color += (Grain - 0.5) * 0.025;
float2 C = UV - 0.5;
Color *= 1.0 - dot(C, C) * 0.55;
return Color;
"""


def make_staff_map_postprocess():
    """M_StaffMapPP: бумажный тон + зерно + виньетка. Возвращает материал или None."""
    pkg_path, name = "/Game/Materials", "M_StaffMapPP"
    full = pkg_path + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        return unreal.EditorAssetLibrary.load_asset(full)
    try:
        mel = unreal.MaterialEditingLibrary
        factory = unreal.MaterialFactoryNew()
        mat = asset_tools.create_asset(name, pkg_path, unreal.Material, factory)
        mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
        # имя значения BlendableLocation менялось между версиями — пробуем оба
        for loc_name in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
            try:
                mat.set_editor_property("blendable_location",
                                        getattr(unreal.BlendableLocation, loc_name))
                break
            except Exception:
                continue

        scene = mel.create_material_expression(
            mat, unreal.MaterialExpressionSceneTexture, -950, -100)
        scene.set_editor_property("scene_texture_id",
                                  unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
        texcoord = mel.create_material_expression(
            mat, unreal.MaterialExpressionTextureCoordinate, -950, 150)

        custom = mel.create_material_expression(
            mat, unreal.MaterialExpressionCustom, -550, 0)
        custom.set_editor_property("code", STAFF_MAP_HLSL)
        custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        in_scene = unreal.CustomInput()
        in_scene.set_editor_property("input_name", "SceneColor")
        in_uv = unreal.CustomInput()
        in_uv.set_editor_property("input_name", "UV")
        custom.set_editor_property("inputs", [in_scene, in_uv])

        mel.connect_material_expressions(scene, "Color", custom, "SceneColor")
        mel.connect_material_expressions(texcoord, "", custom, "UV")
        mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(full)
        log("Создан пост-процесс M_StaffMapPP")
        return mat
    except Exception as err:  # noqa: BLE001 — стиль важнее падения всего сетапа
        unreal.log_warning("[РУБЕЖ] Пост-процесс не создан (%s) — карта будет без стилизации" % err)
        return None


def main():
    # 1. Новый пустой уровень
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        log("TestMap уже существует — открываю и пересобираю содержимое")
        les.load_level(MAP_PATH)
        for actor in list(eas.get_all_level_actors()):
            eas.destroy_actor(actor)
    else:
        les.new_level(MAP_PATH)

    # 2. Пол: плоскость 100×100 → скейл 66 (небольшой запас за краями карты)
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
    floor.set_actor_label("Ground")
    mesh_comp = floor.static_mesh_component
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    mesh_comp.set_static_mesh(plane)
    floor.set_actor_scale3d(unreal.Vector(66.0, 66.0, 1.0))
    mesh_comp.set_material(0, make_paper_material())
    floor.set_actor_enable_collision(True)
    # статичный пол для навигации
    floor.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

    # 3. NavMeshBoundsVolume на всю карту (brush по умолчанию 200 UU)
    nav = spawn(unreal.NavMeshBoundsVolume, unreal.Vector(0.0, 0.0, 200.0))
    nav.set_actor_label("NavBounds")
    nav.set_actor_scale3d(unreal.Vector(2.0 * MAP_HALF / 200.0 + 4.0,
                                        2.0 * MAP_HALF / 200.0 + 4.0, 10.0))

    # 4. Свет
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 2000.0),
                unreal.Rotator(-52.0, 0.0, 35.0))
    sun.set_actor_label("Sun")
    sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 2200.0))
    sky.set_actor_label("Sky")

    # 5. PlayerStart у базы игрока (юго-запад)
    start = spawn(unreal.PlayerStart, unreal.Vector(-2100.0, -2100.0, 150.0))
    start.set_actor_label("PlayerStart_SW")

    # 5.5. Пост-процесс «штабная карта» (не критично, при ошибке просто пропустится)
    pp_mat = make_staff_map_postprocess()
    if pp_mat is not None:
        try:
            ppv = spawn(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 300.0))
            ppv.set_actor_label("StaffMapPP")
            ppv.set_editor_property("unbound", True)
            settings = ppv.get_editor_property("settings")
            blendable = unreal.WeightedBlendable(1.0, pp_mat)
            blendables = unreal.WeightedBlendables([blendable])
            settings.set_editor_property("weighted_blendables", blendables)
            ppv.set_editor_property("settings", settings)
        except Exception as err:  # noqa: BLE001
            unreal.log_warning("[РУБЕЖ] PostProcessVolume не настроен: %s" % err)

    # 6. Сохранить
    les.save_current_level()
    log("TestMap готова: пол, NavMesh, свет, PlayerStart. "
        "Нажми Play — GameMode заспавнит базы и юнитов.")


if __name__ == "__main__":
    main()
