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

    # 6. Сохранить
    les.save_current_level()
    log("TestMap готова: пол, NavMesh, свет, PlayerStart. "
        "Нажми Play — GameMode заспавнит базы и юнитов.")


if __name__ == "__main__":
    main()
