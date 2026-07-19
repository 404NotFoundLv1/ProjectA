import unreal

ASSET_PATH = "/Game/ProjectRift/Items"

def create_equipment(name, item_id, slot, effect_class_path, ability_class_path=None):
    asset_path = f"{ASSET_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        asset.set_editor_property("item_id", unreal.Name(item_id))
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log(f"v0.7.1 asset already exists: {asset_path}")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.load_class(None, "/Script/ProjectA.PREquipmentDataAsset"))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ASSET_PATH, None, factory)
    asset.set_editor_property("item_id", unreal.Name(item_id))
    asset.set_editor_property("max_stack_count", 1)
    asset.set_editor_property("can_equip", True)
    asset.set_editor_property("can_drop", True)
    asset.set_editor_property("equipment_slot", slot)
    asset.set_editor_property("granted_effects", [unreal.load_class(None, effect_class_path)])
    if ability_class_path:
        asset.set_editor_property("granted_abilities", [unreal.load_class(None, ability_class_path)])
    asset.set_editor_property("visual_actor_class", unreal.load_class(None, "/Script/ProjectA.PREquipmentVisualActor"))
    asset.set_editor_property("attachment_socket", unreal.Name("spine_03"))
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log(f"Created v0.7.1 asset: {asset_path}")

create_equipment("DA_TestArmor", "DA_TestArmor", unreal.PREquipmentSlot.ARMOR, "/Script/ProjectA.PRTestArmorEquipmentGameplayEffect")
create_equipment("DA_TestChip", "DA_TestChip", unreal.PREquipmentSlot.CHIP, "/Script/ProjectA.PRTestChipEquipmentGameplayEffect")
create_equipment("DA_FieldToolkit", "DA_FieldToolkit", unreal.PREquipmentSlot.TOOL, "/Script/ProjectA.PRTestToolEquipmentGameplayEffect", "/Script/ProjectA.GA_EquipmentFieldToolkitPassive")
