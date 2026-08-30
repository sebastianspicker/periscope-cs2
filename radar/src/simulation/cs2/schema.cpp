#include "cs2/schema.hpp"

#include <array>
#include <cstring>

namespace cs2 {
namespace {

// Static field tables. Offsets mirror FieldOffsets / ModuleGlobals defaults so
// plant/read and schema resolution stay coherent for the educational sim.
const SchemaField kBaseEntityFields[] = {
    {"C_BaseEntity", "m_iHealth", fnv1a32("m_iHealth"), 0x34C, "int32"},
    {"C_BaseEntity", "m_iTeamNum", fnv1a32("m_iTeamNum"), 0x3E7, "uint8"},
    {"C_BaseEntity", "m_lifeState", fnv1a32("m_lifeState"), 0x354, "uint8"},
    {"C_BaseEntity", "m_pGameSceneNode", fnv1a32("m_pGameSceneNode"), 0x330,
     "CGameSceneNode*"},
};

const SchemaField kSceneNodeFields[] = {
    {"CGameSceneNode", "m_vecAbsOrigin", fnv1a32("m_vecAbsOrigin"), 0xC8,
     "Vector"},
};

const SchemaField kPlayerPawnFields[] = {
    {"C_CSPlayerPawn", "m_vOldOrigin", fnv1a32("m_vOldOrigin"), 0x13B8, "Vector"},
    {"C_CSPlayerPawn", "m_angEyeAngles", fnv1a32("m_angEyeAngles"), 0x3340,
     "QAngle"},
    {"C_CSPlayerPawn", "m_bIsScoped", fnv1a32("m_bIsScoped"), 0x26F8, "bool"},
    {"C_CSPlayerPawn", "m_bIsDefusing", fnv1a32("m_bIsDefusing"), 0x2700, "bool"},
    {"C_CSPlayerPawn", "m_flFlashDuration", fnv1a32("m_flFlashDuration"), 0x1588,
     "float"},
    {"C_CSPlayerPawn", "m_ArmorValue", fnv1a32("m_ArmorValue"), 0x25C4, "int32"},
    {"C_CSPlayerPawn", "m_pWeaponServices", fnv1a32("m_pWeaponServices"), 0x13E0,
     "CPlayer_WeaponServices*"},
    {"C_CSPlayerPawn", "m_entitySpottedState", fnv1a32("m_entitySpottedState"),
     0x26D0, "EntitySpottedState_t"},
};

const SchemaField kControllerFields[] = {
    {"CCSPlayerController", "m_hPlayerPawn", fnv1a32("m_hPlayerPawn"), 0x914,
     "CHandle<C_CSPlayerPawn>"},
    {"CCSPlayerController", "m_iszPlayerName", fnv1a32("m_iszPlayerName"), 0x6E8,
     "char[128]"},
    {"CCSPlayerController", "m_iDesiredFOV", fnv1a32("m_iDesiredFOV"), 0x78C,
     "uint32"},
};

const SchemaField kPlantedC4Fields[] = {
    {"C_PlantedC4", "m_bBombTicking", fnv1a32("m_bBombTicking"), 0xF48, "bool"},
    {"C_PlantedC4", "m_nBombSite", fnv1a32("m_nBombSite"), 0xF4C, "int32"},
    {"C_PlantedC4", "m_flC4Blow", fnv1a32("m_flC4Blow"), 0xF58, "float"},
    {"C_PlantedC4", "m_flDefuseCountDown", fnv1a32("m_flDefuseCountDown"), 0xF70,
     "float"},
    {"C_PlantedC4", "m_bBombDefused", fnv1a32("m_bBombDefused"), 0xF7C, "bool"},
};

const SchemaField kWeaponServicesFields[] = {
    {"CPlayer_WeaponServices", "m_hActiveWeapon", fnv1a32("m_hActiveWeapon"),
     0x58, "CHandle<C_BasePlayerWeapon>"},
};

}  // namespace

const SchemaRegistry& SchemaRegistry::get() {
  static const SchemaRegistry registry = [] {
    SchemaRegistry reg;
    reg.classes[0] = {"C_BaseEntity",
                      static_cast<int>(std::size(kBaseEntityFields)),
                      kBaseEntityFields};
    reg.classes[1] = {"CGameSceneNode",
                      static_cast<int>(std::size(kSceneNodeFields)),
                      kSceneNodeFields};
    reg.classes[2] = {"C_CSPlayerPawn",
                      static_cast<int>(std::size(kPlayerPawnFields)),
                      kPlayerPawnFields};
    reg.classes[3] = {"CCSPlayerController",
                      static_cast<int>(std::size(kControllerFields)),
                      kControllerFields};
    reg.classes[4] = {"C_PlantedC4",
                      static_cast<int>(std::size(kPlantedC4Fields)),
                      kPlantedC4Fields};
    reg.classes[5] = {"CPlayer_WeaponServices",
                      static_cast<int>(std::size(kWeaponServicesFields)),
                      kWeaponServicesFields};
    return reg;
  }();
  return registry;
}

const SchemaClass* SchemaRegistry::find_class(std::string_view class_name) const {
  for (const auto& cls : classes) {
    if (cls.name == class_name) return &cls;
  }
  return nullptr;
}

const SchemaField* SchemaRegistry::find(std::string_view class_name,
                                        std::string_view field_name) const {
  const SchemaClass* cls = find_class(class_name);
  if (!cls || !cls->fields) return nullptr;
  for (int i = 0; i < cls->field_count; ++i) {
    if (cls->fields[i].field_name == field_name) return &cls->fields[i];
  }
  return nullptr;
}

const SchemaField* SchemaRegistry::find_by_hash(std::string_view class_name,
                                                std::uint32_t field_hash) const {
  const SchemaClass* cls = find_class(class_name);
  if (!cls || !cls->fields) return nullptr;
  for (int i = 0; i < cls->field_count; ++i) {
    if (cls->fields[i].name_hash == field_hash) return &cls->fields[i];
  }
  return nullptr;
}

int SchemaRegistry::resolve_offset(std::string_view class_name,
                                   std::string_view field_name) const {
  const SchemaField* field = find(class_name, field_name);
  return field ? field->offset : -1;
}

int SchemaRegistry::resolve_offset_by_hash(std::string_view class_name,
                                           std::uint32_t field_hash) const {
  const SchemaField* field = find_by_hash(class_name, field_hash);
  return field ? field->offset : -1;
}

int SchemaRegistry::total_fields() const {
  int total = 0;
  for (const auto& cls : classes) total += cls.field_count;
  return total;
}

std::vector<SchemaField> SchemaRegistry::all_fields() const {
  std::vector<SchemaField> out;
  out.reserve(static_cast<std::size_t>(total_fields()));
  for (const auto& cls : classes) {
    for (int i = 0; i < cls.field_count; ++i) out.push_back(cls.fields[i]);
  }
  return out;
}

int apply_schema_to_field_offsets(FieldOffsets& out) {
  const auto& reg = SchemaRegistry::get();
  int applied = 0;
  auto set = [&](std::string_view cls, std::string_view field, int& dest) {
    const int off = reg.resolve_offset(cls, field);
    if (off >= 0) {
      dest = off;
      ++applied;
    }
  };
  set("C_BaseEntity", "m_iHealth", out.health);
  set("C_BaseEntity", "m_iTeamNum", out.team);
  set("C_BaseEntity", "m_lifeState", out.life_state);
  set("C_BaseEntity", "m_pGameSceneNode", out.scene_node);
  set("CGameSceneNode", "m_vecAbsOrigin", out.node_origin);
  set("C_CSPlayerPawn", "m_vOldOrigin", out.old_origin);
  set("C_CSPlayerPawn", "m_angEyeAngles", out.eye_angles);
  set("C_CSPlayerPawn", "m_bIsScoped", out.is_scoped);
  set("C_CSPlayerPawn", "m_bIsDefusing", out.is_defusing);
  set("C_CSPlayerPawn", "m_flFlashDuration", out.flash_duration);
  set("C_CSPlayerPawn", "m_ArmorValue", out.armor);
  set("C_CSPlayerPawn", "m_pWeaponServices", out.weapon_services);
  set("CCSPlayerController", "m_hPlayerPawn", out.controller_pawn_handle);
  set("CCSPlayerController", "m_iszPlayerName", out.controller_name);
  set("CCSPlayerController", "m_iDesiredFOV", out.desired_fov);
  set("C_PlantedC4", "m_bBombTicking", out.bomb_ticking);
  set("C_PlantedC4", "m_nBombSite", out.bomb_site);
  set("C_PlantedC4", "m_flC4Blow", out.bomb_blow);
  set("C_PlantedC4", "m_flDefuseCountDown", out.bomb_defuse_count);
  set("C_PlantedC4", "m_bBombDefused", out.bomb_defused);
  set("CPlayer_WeaponServices", "m_hActiveWeapon", out.active_weapon);
  return applied;
}

}  // namespace cs2
