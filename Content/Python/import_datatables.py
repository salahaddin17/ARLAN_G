# ============================================================================
# РУБЕЖ: АРЛАН — импорт CSV-баланса в DataTable-ассеты (ОПЦИОНАЛЬНО).
#
# Игра уже читает CSV напрямую в рантайме (URTSDataSubsystem), так что этот
# скрипт нужен, только если хочется редактировать баланс в UI редактора.
#
# Запуск: py "Content/Python/import_datatables.py"
# ============================================================================

import os
import unreal

TABLES = [
    ("Units.csv", "/Script/RubezhArlan.UnitRow", "DT_Units"),
    ("Buildings.csv", "/Script/RubezhArlan.BuildingRow", "DT_Buildings"),
    ("DamageMatrix.csv", "/Script/RubezhArlan.DamageMatrixRow", "DT_DamageMatrix"),
    ("Factions.csv", "/Script/RubezhArlan.FactionRow", "DT_Factions"),
    ("Difficulty.csv", "/Script/RubezhArlan.DifficultyRow", "DT_Difficulty"),
]

PKG = "/Game/Data"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
content_dir = unreal.SystemLibrary.get_project_content_directory()


def import_one(csv_name, struct_path, asset_name):
    struct = unreal.load_object(None, struct_path)
    if struct is None:
        unreal.log_error("Не найден struct %s — модуль собран?" % struct_path)
        return

    full = PKG + "/" + asset_name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        table = unreal.EditorAssetLibrary.load_asset(full)
    else:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", struct)
        table = asset_tools.create_asset(asset_name, PKG, unreal.DataTable, factory)

    csv_path = os.path.join(content_dir, "Data", csv_name)
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv_path)
    if ok:
        unreal.EditorAssetLibrary.save_asset(full)
        unreal.log("[РУБЕЖ] %s ← %s: импортировано" % (asset_name, csv_name))
    else:
        unreal.log_error("[РУБЕЖ] %s: ошибка импорта %s" % (asset_name, csv_path))


def main():
    for csv_name, struct_path, asset_name in TABLES:
        import_one(csv_name, struct_path, asset_name)


if __name__ == "__main__":
    main()
