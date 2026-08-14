#include "cs2/signatures.hpp"
#include "cs2/signatures_internal.hpp"

#include <array>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace cs2 {
namespace {

constexpr std::array<std::string_view, SignatureDatabase::CATEGORY_COUNT> kCategoryNames = {
    CAT_RENDER, CAT_ENTITY, CAT_PLAYER, CAT_WEAPON, CAT_INPUT, CAT_NET,
    CAT_MOVE, CAT_GAMESTATE, CAT_SOUND, CAT_GLOW, CAT_SYSTEM, CAT_UI,
    CAT_TRACE, CAT_ECON,
};

bool is_one_of(std::string_view name,
               std::initializer_list<std::string_view> names) {
  for (const auto candidate : names) {
    if (name == candidate) return true;
  }
  return false;
}

std::string_view category_for(std::string_view name) {
  if (is_one_of(name, {"DRAWCROSSHAIR", "DRAWOVERHEAD", "DRAWSCOPEOVERLAY",
                       "DRAWSMOKEVERTEX", "DRAWVIEWPUNCH2", "DRAWLEGS",
                       "DRAWTEAMINTRO", "DRAWLIGHTSCENE", "DRAWOBJECT_LEGACY",
                       "DRAWSKYBOXARRAY", "DRAWAGGEREGATEOBJECT",
                       "DRAWAGGREGATESCENEOBJECTARRAY", "RENDERVIEWLAYER_DISPATCH",
                       "RENDERDECALS", "GENERATEPRIMITIVES", "FRAMESTAGENOTIFY",
                       "FRAMEUPDATE", "UPDATEPOSTPROCESSING", "UPDATESKYBOX",
                       "UPDATELIGHTOBJECT", "CALCVIEWMODEL", "CALCVIEWMODELTRANSFORM_V2",
                       "CALCVIEWMODELVIEW", "CALCULATEWORLDSPACEBONES",
                       "CALCWORLDSPACEBONES", "FIRSTPERSONLEGS", "REMOVELEGS"}))
    return CAT_RENDER;
  if (is_one_of(name, {"GETENTITYBYINDEX", "GETBASEENTITY", "GETENTITYHANDLE",
                       "FINDENTITYBYCLASSNAME", "FINDENTITYBYNAME",
                       "C_BASEENTITY_COMPUTEHITBOXSURROUNDINGBOX",
                       "C_BASEENTITY_GETBONEIDBYNAME", "C_BASEENTITY_GETHITBOXSET",
                       "C_BASEENTITY_PROCESSINTERPOLATEDLIST", "C_BASEENTITY_RESTOREDATA",
                       "C_BASEENTITY_SAVEDATA", "C_BASEENTITY_STARTPARTICLESYSTEM",
                       "C_BASEENTITY_UPDATEBODYGROUPCHOICE", "ONREMOVEENTITY",
                       "ONADDENTITY", "SETENTITYNAME", "CHANGEMODEL", "SETMODEL",
                       "SETBODYGROUP", "SETBODYGROUP_INV", "ONBODYGROUPCHOICECHANGED",
                       "GETBONEPOSITIONBYNAME", "CREATEENTITYBYCLASSNAME",
                       "CREATEENTITYBYNAME", "UTIL_CREATEENTITYBYNAME", "UTIL_REMOVE",
                       "DELETEENTITY", "ACCEPTINPUT", "FINDUSEENTITY", "FIREOUTPUTINTERNAL",
                       "HANDLEENTITYLIST", "SETMESHGROUPMASK", "ONSKELETONMODELCHANGED"}))
    return CAT_ENTITY;
  if (is_one_of(name, {"GETLOCALPLAYERCONTROLLER", "GETLOCALPAWN",
                       "GETLOCALCONTROLLERBYID", "GETPLAYERCONTROLLER", "GETPLAYERMODEL",
                       "GETPLAYERINTERP", "GETPLAYERTEAMNAME", "GETEYEANGLES",
                       "GETVIEWANGLES", "SETVIEWANGLES", "SETVIEWANGLE", "SETPLAYERREADY",
                       "SETLOCALPLAYERREADY", "GETCLIENTSYSTEM", "PCLIENTMODE",
                       "CLIENTMODECSNORMAL_ONEVENT", "CLIENT_DISPATCHSPAWN", "DISPATCHSPAWN",
                       "DISPATCHSPAWN_CALLER", "POSTDATAUPDATE", "ONPOSTDATAUPDATE",
                       "QUEUEPOSTDATAUPDATES", "CLIENTCOMMAND", "CLIENTPRINT", "SETPAWN",
                       "GETABSORIGIN", "SETABSORIGIN_PAWN", "OVERRIDEVIEW", "GETCONTROLLERCMD",
                       "GETCUSERCMDARRAY", "GETCUSERCMDBYSEQUENCENUMBER", "GETCUSERCMDTICK",
                       "SETUPCMD", "PROCESSUSERCMDS", "PLOCALPLAYERCONTROLLER",
                       "SOMETIMINGFROMPAWN", "GETTICKBASE"}))
    return CAT_PLAYER;
  if (is_one_of(name, {"CSBASEGUNFIREDATA", "GETINACCURACY", "GETSPREAD", "CALCSPREAD",
                       "FX_FIREBULLETS", "C_CSWEAPONBASE_GETECONWPNDATA",
                       "C_CSWEAPONBASE_UPDATECOMPOSITEMATERIAL",
                       "C_CSWEAPONBASE_UPDATECOMPOSITEMATERIALSET",
                       "C_CSWEAPONBASEGUN_GETINACCURACY", "C_CSWEAPONBASEGUN_GETSPREAD",
                       "C_ECONITEMVIEW_GETBASEPLAYERWEAPONVDATA",
                       "C_ECONITEMVIEW_GETSTATICDATA", "GETCSWEAPONDATAFROMKEY",
                       "GETWEAPONINACCURACYRECOVERYTIME", "UPDATETURNINGINACCURACY",
                       "REGENERATEWEAPONSKIN", "REGENERATEWEAPONSKIN_V2",
                       "REGENERATEWEAPONSKINS", "REMOVEPLAYERITEM", "EQUIPITEMINLOADOUT",
                       "GETITEMINLOADOUT", "GIVENAMEDITEM", "GLOVEAPPLY_PERTICK",
                       "WEAPONSERVICES_EQUIPWEAPON", "ITEMSERVICES_CANACQUIRE", "NOSPREAD1",
                       "GETATTRIBUTEDEFBYNAME", "GETATTRIBUTEDEFINITIONBYNAME",
                       "GETATTRIBUTEDEFINITIONINTERFACE", "GETHITGROUP", "CTAKEDAMAGEINFO",
                       "TAKEDAMAGEOLD", "REPORTHIT", "GETREMOVEDAIMPUNCH",
                       "GETREMOVEDAIMPUNCH_E8", "UNLOCKINVENTORY", "GETCUSTOMPAINTKITINDEX",
                       "SPREADSEEDGEN", "PWEAPONC4"}))
    return CAT_WEAPON;
  if (is_one_of(name, {"PINPUTSYSTEM", "PINPUTSYSTEMSVC", "INPUTTESTACTIVATOR",
                       "INPUTTRIGGERFORACTIVATEDPLAYER", "FORCEBUTTONSDOWN", "PCSGOINPUT",
                       "PCVAR", "CINPUTPTRGLOBAL", "GETINT64", "GETSTRING", "SETSTRING",
                       "PSENSITIVITY", "CONCOMMAND_FIRSTPERSON", "CONCOMMAND_THIRDPERSON",
                       "THIRDPERSONRESET", "SPECTATORINPUT", "GETUSERCMDMANAGER",
                       "SDL_EVENTHANDLER"}))
    return CAT_INPUT;
  if (is_one_of(name, {"SENDNETMESSAGE", "PROCESSMESSAGES", "PROCESSSERVERINFO",
                       "CREATEMOVE", "SERIALIZEUSERCMD", "SETUPSUBTICKINFO",
                       "SETUPMOVEMENTMOVES", "CREATENEWSUBTICKMOVESTEP",
                       "CREATESUBTICKMOVESTEP", "WRITESUBTICKFROMENTRY",
                       "WRITEUPDATEMESSAGEATTICK", "PARSESUBTICKDURATION",
                       "PARSESUBTICKFRACTION", "PROCESSSUBTICKINPUT", "SENDSNAPSHOT",
                       "NETSYSTEM_CNETCHAN_PROCESSMESSAGES",
                       "NETSYSTEM_CNETCHAN_SENDNETMESSAGE",
                       "ENGINE_NETWORKGAMECLIENT_CONNECT",
                       "ENGINE_NETWORKGAMECLIENT_SETSIGNONSTATE", "ENGINE_NETTIMEOUTDISCONNECT",
                       "ENGINE_DISCONNECT_MAIN", "REPLYCONNECTION", "SETSIGNONSTATE",
                       "DISPATCHEFFECT", "DISPATCHEVENT", "DISPATCHUPDATEONREMOVE",
                       "UPDATEONREMOVE", "HANDLEBULLETPENETRATION_V2", "PROCESSIMPACTS",
                       "TRACEHANDLEBULLETPEN", "TESTSURFACES", "PNETWORKGAMECLIENT",
                       "PNETWORKSYSTEM", "NETWORKSTATECHANGED", "REGISTERNETMESSAGE",
                       "REGISTERNETMESSAGEHANDLERABSTRACT"}))
    return CAT_NET;
  if (is_one_of(name, {"PROCESSMOVEMENT", "SETUPMOVE", "RUNPREDICTION", "CREATETRACE",
                       "PHYSICSRUNTHINK_CTRL", "PHYSICSRUNTHINK_PAWN", "SETCOLLISIONBOUNDS",
                       "SETGRAVITYSCALE", "SETGROUNDENTITY", "SETMOVETYPE",
                       "MARKINTERPLATCHFLAGSDIRTY", "CALCULATEINTERPOLATION",
                       "MODERNSUBTICKJUMPCHECK", "GRAVITYTOUCH", "STARTTOUCH", "ENDTOUCH",
                       "PERFORMBATCHEDINVALIDATEPHYSICSRECURSIVE", "NOCLIPONCHANGE"}))
    return CAT_MOVE;
  if (is_one_of(name, {"ISINGAME", "HASONGOINGMATCH", "ACTIONMATCHMAKING",
                       "ACTIONABANDONONGOINGMATCH", "ACTIONRECONNECTTOONGOINGMATCH",
                       "ACTIONACKNOWLEDGEPENALTY", "MATCHFOUNDHANDLER",
                       "GETCOOLDOWNSECONDSREMAINING", "GETCOOLDOWNTYPE", "GETCOOLDOWNREASON",
                       "COOLDOWNISPERMANENT", "GETGAMEMODENAME", "GETLEVELNAME",
                       "GETLEVELNAMESHORT", "GETMAPNAME", "GETMAPBSPNAME", "ENGINE_GETLEVELNAME",
                       "ENGINE_GETLEVELNAMESHORT", "ENGINE_ISCONNECTED", "ENGINE_LOADGAMEINFO",
                       "ENGINE_MOUNTADDON", "ENGINE_HOSTSTATEMGR_QUEUENEWREQUEST",
                       "HOSTSTATEREQUEST", "HOSTSTATEREQUEST_START", "HOST_FILTERTIME", "HOST_SAY",
                       "TERMINATEROUND", "ISOVERWATCH", "GAMESYSTEM_THINK_CHECKSTEAMBAN",
                       "CHECKTRANSMIT", "ISDEMOORHLTV", "ISHEARINGCLIENT", "ISLATCHED",
                       "ISLOCALPLAYERWATCHINGOWNDEMO", "LEVELINIT", "LEVELSHUTDOWN",
                       "ONVOTERESULT", "GETTOURNAMENTSTAGECOUNT", "GETTOURNAMENTSTAGENAMEBYINDEX",
                       "GETTOURNAMENTTEAMCOUNT", "GETTOURNAMENTTEAMFLAGBYID",
                       "GETTOURNAMENTTEAMFLAGBYINDEX", "GETTOURNAMENTTEAMNAMEBYID",
                       "GETTOURNAMENTTEAMNAMEBYINDEX", "GETTOURNAMENTTEAMTAGBYID",
                       "GETTOURNAMENTTEAMTAGBYINDEX", "SWITCHTEAM", "SUBMITCOMMENDATION",
                       "SUBMITPLAYERREPORT", "SHOWFAIRPLAYGUIDELINESFORCOOLDOWN",
                       "GENERATEDIRECTCHALLENGECODE", "GETDIRECTCHALLENGECODE",
                       "GETDIRECTCHALLENGECODEFORCLAN", "VALIDATEDIRECTCHALLENGECODE",
                       "FORCEDEMORECORDINGFULLUPDATEAFTERNEXTDELTAPACKET",
                       "GETROTATINGOFFICIALMAPGROUPCURRENTSTATE", "GETSPAWNGROUPS",
                       "GETBOMBSITEACENTER", "GETBOMBSITEBCENTER", "STARTDEFUSE", "ONVOTERESULT"}))
    return CAT_GAMESTATE;
  if (is_one_of(name, {"PLAYVSOUND", "PLAYVSOUND_CLIENT", "PLAYVSND", "EMITSOUNDBYHANDLE",
                       "EMITSOUNDFILTER", "EMITSOUNDPARAMS", "EMITPANORAMASOUND",
                       "STARTSOUNDEVENT", "VSCONFIG_GETSOUNDSESSION", "GETSOUNDSESSION",
                       "PSOUNDCHANNELS"}))
    return CAT_SOUND;
  if (is_one_of(name, {"GLOWOBJECTMANAGER_GETINSTANCE", "MANAGEGLOWSCENEOBJECT",
                       "ONGLOWTYPECHANGED", "SETRENDERINGORIGIN", "MATERIALGROUPFORYOURFACE",
                       "SETGLOWTYPE", "PGLOWMANAGER"}))
    return CAT_GLOW;
  if (is_one_of(name, {"FINDHUDELEMENT", "HUDCHAT_ONSAYTEXT2", "HUDCHATPRINTF",
                       "FLASHOVERLAY", "SHOULDSHOWHUDELEMENTS", "SHOULDUPDATESEQUENCES",
                       "CLEARHUDWEAPONICON", "PANORAMAEVENT", "PUIENGINE", "GETCHATOBJECT",
                       "UTIL_SAYTEXT2FILTER", "UTIL_SAYTEXTFILTER", "SHOWMESSAGEBOX",
                       "PMAINMENUPANEL", "HANDLETEAMINTRO", "PHUDPANEL", "SENDCHATMESSAGE"}))
    return CAT_UI;
  if (is_one_of(name, {"TRACECREATE", "TRACEGETINFO", "TRACEINITDATA", "TRACEINITFILTER",
                       "TRACEINITINFO", "TRACEPLAYERBBOX", "TRACESHAPE", "INITTRACEINFO",
                       "SETTRACEINIT", "INITPLAYERMOVEMENTTRACEFILTER", "GETSURFACEDATA",
                       "AUTOWALLINIT", "AUTOWALLTRACEPOS", "PGAMETRACEMANAGER"}))
    return CAT_TRACE;
  if (is_one_of(name, {"APPLYMATERIALVARSFORBATCH", "COMPOSITEMATERIALINPUT_ADDTOTAIL",
                       "INFOFORRESOURCETYPECCOMPOSITEMATERIAL_TYPEMANAGER",
                       "BUILDTEMPLATEMATERIALFROMFILE", "PREPARESCENEMATERIAL",
                       "CATTRIBUTESTRINGFILL", "CATTRIBUTESTRINGINIT", "CBUFFERSTRINGINIT",
                       "CREATEECONITEM", "CECONITEMCREATEINSTANCE", "GETECONITEMSYSTEM",
                       "GETITEMVIEWBYID", "SETITEMITEMIDFUNCTION", "SETATTRIBUTE",
                       "SETDYNAMICATTRIBUTEVALUE", "SETDYNAMICATTRIBUTEVALUE_RAW",
                       "SETORADDATTRIBUTEVALUEBYNAME", "SERIALIZETOPROTOBUFITEM",
                       "APPLYECONCUSTOMIZATION", "SETMATERIALGROUP", "SETMATERIALSHADERTYPE",
                       "FINDSOCACHE", "CREATESHAREDOBJECTSUBCLASSECONITEM",
                       "CREATESOSUBCLASSECONITEM", "ADDNAMETAGENTITY", "ADDSTATTRAKENTITY",
                       "ALLOCATEATTRIBUTELIST", "SOCREATED", "UPDATESUBCLASS"}))
    return CAT_ECON;
  return CAT_SYSTEM;
}

const char* description_for(std::string_view category) {
  if (category == CAT_RENDER) return "Rendering and drawing signature";
  if (category == CAT_ENTITY) return "Game entity signature";
  if (category == CAT_PLAYER) return "Player or pawn signature";
  if (category == CAT_WEAPON) return "Weapon and combat signature";
  if (category == CAT_INPUT) return "Input handling signature";
  if (category == CAT_NET) return "Network message signature";
  if (category == CAT_MOVE) return "Movement and physics signature";
  if (category == CAT_GAMESTATE) return "Game session signature";
  if (category == CAT_SOUND) return "Audio system signature";
  if (category == CAT_GLOW) return "Glow and visual signature";
  if (category == CAT_UI) return "HUD and Panorama signature";
  if (category == CAT_TRACE) return "Trace and collision signature";
  if (category == CAT_ECON) return "Inventory and economy signature";
  return "Engine and system signature";
}

std::string normalize_hex(std::string_view source) {
  std::istringstream tokens{std::string(source)};
  std::string token;
  std::string normalized;
  while (tokens >> token) {
    if (!normalized.empty()) normalized += ' ';
    normalized += token == "?" ? "??" : token;
  }
  return normalized;
}

struct DatabaseStorage {
  std::array<std::vector<SignaturePattern>, SignatureDatabase::CATEGORY_COUNT> patterns;
  std::vector<std::string> names;
  std::vector<std::string> hexes;
  SignatureDatabase database{};

  DatabaseStorage() {
    names.reserve(461);
    hexes.reserve(461);
    std::istringstream source(sig_detail::embedded_patterns_text());
    std::string line;
    while (std::getline(source, line)) {
      // Skip empty lines and the R"( / )" delimiters
      if (line.empty() || line == "R\"(" || line.find(")") == 0) continue;
      std::istringstream fields(line);
      std::string name;
      if (!(fields >> name)) continue;
      std::string bytes;
      std::getline(fields, bytes);
      // Skip leading whitespace in bytes
      auto first = bytes.find_first_not_of(" \t");
      if (first != std::string::npos) bytes = bytes.substr(first);
      const std::string_view category = category_for(name);
      std::size_t category_index = 0;
      while (kCategoryNames[category_index] != category) ++category_index;
      names.push_back(std::move(name));
      hexes.push_back(normalize_hex(bytes));
      patterns[category_index].push_back(
          {names.back(), hexes.back(), category, description_for(category)});
    }
    for (std::size_t i = 0; i < patterns.size(); ++i) {
      database.categories[i] = {kCategoryNames[i], static_cast<int>(patterns[i].size()),
                                patterns[i].data()};
    }
  }
};

DatabaseStorage& storage() {
  static DatabaseStorage instance;
  return instance;
}

}  // namespace

const SignatureDatabase& SignatureDatabase::get() { return storage().database; }

const SignaturePattern* SignatureDatabase::find(std::string_view name) const {
  for (const auto& entry : categories) {
    for (int i = 0; i < entry.count; ++i) {
      if (entry.patterns[i].name == name) return &entry.patterns[i];
    }
  }
  return nullptr;
}

const SignatureCategory* SignatureDatabase::category(std::string_view cat_name) const {
  for (const auto& entry : categories) {
    if (entry.name == cat_name) return &entry;
  }
  return nullptr;
}

int SignatureDatabase::total_count() const {
  int total = 0;
  for (const auto& entry : categories) total += entry.count;
  return total;
}

}  // namespace cs2
