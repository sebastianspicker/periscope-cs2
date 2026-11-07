// weapon_table.hpp: CS2 item-definition index to display name (E51).
//
// A def index is the only stable weapon identity in memory; the display name
// is a local lookup. The table is intentionally small: the common primary,
// secondary, utility and knife indices. Unknown indices fall back to
// "Weapon <n>" so a new item still renders something.
//
// Pure and host-testable.

#pragma once

#include <cstdint>
#include <string>

namespace real::cs2::weapon {

struct Entry {
  std::uint16_t def_index;
  const char* name;
};

inline constexpr Entry kTable[] = {
    {1, "Desert Eagle"},   {2, "Dual Berettas"}, {3, "Five-SeveN"},
    {4, "Glock-18"},       {7, "AK-47"},         {8, "AUG"},
    {9, "AWP"},            {10, "FAMAS"},        {11, "G3SG1"},
    {13, "Galil AR"},      {14, "M249"},         {16, "M4A4"},
    {17, "MAC-10"},        {19, "P90"},          {23, "MP5-SD"},
    {24, "UMP-45"},        {25, "XM1014"},       {26, "PP-Bizon"},
    {27, "MAG-7"},         {28, "Negev"},        {29, "Sawed-Off"},
    {30, "Tec-9"},         {31, "Zeus x27"},     {32, "P2000"},
    {33, "MP7"},           {34, "MP9"},          {35, "Nova"},
    {36, "P250"},          {38, "SCAR-20"},      {39, "SG 553"},
    {40, "SSG 08"},        {42, "Knife"},        {43, "Flashbang"},
    {44, "HE Grenade"},    {45, "Smoke"},        {46, "Molotov"},
    {47, "Decoy"},         {48, "Incendiary"},   {49, "C4"},
    {59, "Knife"},         {60, "M4A1-S"},       {61, "USP-S"},
    {63, "CZ75-Auto"},     {64, "R8 Revolver"},  {500, "Bayonet"},
    {503, "Shadow Daggers"},{505, "Flip Knife"}, {506, "Gut Knife"},
    {507, "Karambit"},     {508, "M9 Bayonet"},  {509, "Huntsman Knife"},
    {512, "Falchion Knife"},{514, "Bowie Knife"},{515, "Butterfly Knife"},
    {516, "Ursus Knife"},  {517, "Navaja Knife"}, {518, "Stiletto Knife"},
    {519, "Talon Knife"},  {520, "Skeleton Knife"},{521, "Nomad Knife"},
    {522, "Survival Knife"},{523, "Paracord Knife"},{525, "Classic Knife"},
    {526, "Kukri Knife"},
};

inline const char* name_for(std::uint16_t def_index) {
  for (const Entry& entry : kTable) {
    if (entry.def_index == def_index) return entry.name;
  }
  return nullptr;
}

inline std::string display_name(std::uint16_t def_index) {
  if (def_index == 0) return {};
  if (const char* known = name_for(def_index)) return known;
  return "Weapon " + std::to_string(def_index);
}

/// True for indices that represent a knife/melee.
inline bool is_knife(std::uint16_t def_index) {
  return (def_index >= 42 && def_index <= 59) || def_index >= 500;
}

}  // namespace real::cs2::weapon
