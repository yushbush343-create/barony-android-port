#include "main.hpp"
#include "custom_classes.hpp"
#include "items.hpp"
#include "magic/magic.hpp"
#include "stat.hpp"

#include <algorithm>
#include <set>
#include <vector>
#include <utility>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace
{
constexpr PHYSFS_sint64 MAX_CUSTOM_CLASSES_FILE_SIZE = 1024 * 1024;
constexpr int MAX_CUSTOM_CLASSES = 64;
constexpr int MAX_CUSTOM_CLASS_ID = 127 - NUMCLASSES;

bool readOptionalInt(
    const rapidjson::Value& object,
    const char* name,
    int& output,
    int minimum,
    int maximum)
{
    if (!object.HasMember(name))
    {
        return true;
    }

    const rapidjson::Value& value = object[name];
    if (!value.IsInt())
    {
        return false;
    }

    const int parsed = value.GetInt();
    if (parsed < minimum || parsed > maximum)
    {
        return false;
    }

    output = parsed;
    return true;
}
}

std::vector<CustomClass> CustomClassManager::classes;

void CustomClassManager::init()
{
    classes.clear();

    PHYSFS_File* file = PHYSFS_openRead("custom/classes.json");
    if (!file)
    {
        printlog("[CustomClasses]: No custom/classes.json found: %s",
            PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return;
    }

    const PHYSFS_sint64 fileSize = PHYSFS_fileLength(file);
    if (fileSize <= 0 || fileSize > MAX_CUSTOM_CLASSES_FILE_SIZE)
    {
        printlog("[CustomClasses]: classes.json has an invalid size (%lld bytes).",
            static_cast<long long>(fileSize));
        PHYSFS_close(file);
        return;
    }

    std::vector<char> buffer(static_cast<size_t>(fileSize) + 1, '\0');
    const PHYSFS_sint64 bytesRead = PHYSFS_readBytes(file, buffer.data(), fileSize);
    PHYSFS_close(file);

    if (bytesRead != fileSize)
    {
        printlog("[CustomClasses]: Could not read the complete classes.json file.");
        return;
    }

    rapidjson::Document document;
    document.Parse(buffer.data());

    if (document.HasParseError()
        || !document.IsObject()
        || !document.HasMember("version")
        || !document["version"].IsInt()
        || !document.HasMember("classes")
        || !document["classes"].IsArray())
    {
        printlog("[CustomClasses]: Invalid classes.json structure.");
        return;
    }

    const int version = document["version"].GetInt();
    if (version != 1 && version != 2)
    {
        printlog("[CustomClasses]: Unsupported classes.json version %d.", version);
        return;
    }

    const rapidjson::Value& classList = document["classes"];
    std::set<int> usedIds;
    std::set<std::string> usedNames;
    int legacyId = 0;

    for (rapidjson::Value::ConstValueIterator itr = classList.Begin();
         itr != classList.End() && classes.size() < MAX_CUSTOM_CLASSES;
         ++itr)
    {
        if (!itr->IsObject()
            || !itr->HasMember("internal_name")
            || !(*itr)["internal_name"].IsString())
        {
            printlog("[CustomClasses]: Skipping a class without a valid internal_name.");
            continue;
        }

        CustomClass customClass;
        customClass.internalName = (*itr)["internal_name"].GetString();

        if (customClass.internalName.empty() || customClass.internalName.size() > 64)
        {
            printlog("[CustomClasses]: Skipping a class with an invalid internal_name.");
            continue;
        }

        if (version >= 2)
        {
            if (!itr->HasMember("id") || !(*itr)["id"].IsInt())
            {
                printlog("[CustomClasses]: Skipping '%s': version 2 classes require an integer id.",
                    customClass.internalName.c_str());
                continue;
            }
            customClass.id = (*itr)["id"].GetInt();
        }
        else
        {
            customClass.id = legacyId;
        }
        ++legacyId;

        if (customClass.id < 0 || customClass.id > MAX_CUSTOM_CLASS_ID
            || usedIds.find(customClass.id) != usedIds.end()
            || usedNames.find(customClass.internalName) != usedNames.end())
        {
            printlog("[CustomClasses]: Skipping '%s': duplicate or out-of-range id/name.",
                customClass.internalName.c_str());
            continue;
        }

        if (itr->HasMember("display_name") && (*itr)["display_name"].IsString())
        {
            customClass.displayName = (*itr)["display_name"].GetString();
        }
        if (customClass.displayName.empty())
        {
            customClass.displayName = customClass.internalName;
        }
        if (customClass.displayName.size() > 63)
        {
            printlog("[CustomClasses]: Skipping '%s': display_name is too long.",
                customClass.internalName.c_str());
            continue;
        }

        if (itr->HasMember("description") && (*itr)["description"].IsString())
        {
            customClass.description = (*itr)["description"].GetString();
        }
        if (customClass.description.size() > 512)
        {
            printlog("[CustomClasses]: Skipping '%s': description is too long.",
                customClass.internalName.c_str());
            continue;
        }

        if (!readOptionalInt(*itr, "max_hp", customClass.maxHP, -30, 1000)
            || !readOptionalInt(*itr, "max_mp", customClass.maxMP, -30, 1000)
            || !readOptionalInt(*itr, "STR", customClass.STR, -100, 100)
            || !readOptionalInt(*itr, "DEX", customClass.DEX, -100, 100)
            || !readOptionalInt(*itr, "CON", customClass.CON, -100, 100)
            || !readOptionalInt(*itr, "INT", customClass.INT, -100, 100)
            || !readOptionalInt(*itr, "PER", customClass.PER, -100, 100)
            || !readOptionalInt(*itr, "CHR", customClass.CHR, -100, 100))
        {
            printlog("[CustomClasses]: Skipping '%s': invalid stat value.",
                customClass.internalName.c_str());
            continue;
        }

        if (itr->HasMember("allowed_races"))
        {
            const rapidjson::Value& races = (*itr)["allowed_races"];
            if (!races.IsArray())
            {
                printlog("[CustomClasses]: Skipping '%s': allowed_races must be an array.",
                    customClass.internalName.c_str());
                continue;
            }

            bool validRaces = true;
            for (rapidjson::Value::ConstValueIterator race = races.Begin();
                 race != races.End(); ++race)
            {
                if (!race->IsInt() || race->GetInt() < 0 || race->GetInt() >= RACE_ENUM_END)
                {
                    validRaces = false;
                    break;
                }
                if (std::find(customClass.allowedRaces.begin(), customClass.allowedRaces.end(),
                    race->GetInt()) == customClass.allowedRaces.end())
                {
                    customClass.allowedRaces.push_back(race->GetInt());
                }
            }
            if (!validRaces)
            {
                printlog("[CustomClasses]: Skipping '%s': invalid race ID.",
                    customClass.internalName.c_str());
                continue;
            }
        }
        if (customClass.allowedRaces.empty())
        {
            customClass.allowedRaces.push_back(RACE_HUMAN);
        }

        if (itr->HasMember("proficiencies"))
        {
            const rapidjson::Value& proficiencies = (*itr)["proficiencies"];
            if (!proficiencies.IsArray() || (proficiencies.Size() % 2) != 0)
            {
                printlog("[CustomClasses]: Skipping '%s': proficiencies must contain id/value pairs.",
                    customClass.internalName.c_str());
                continue;
            }

            bool validProficiencies = true;
            for (rapidjson::SizeType i = 0; i < proficiencies.Size(); i += 2)
            {
                if (!proficiencies[i].IsInt() || !proficiencies[i + 1].IsInt()
                    || proficiencies[i].GetInt() < 0
                    || proficiencies[i].GetInt() >= NUMPROFICIENCIES
                    || proficiencies[i + 1].GetInt() < 0
                    || proficiencies[i + 1].GetInt() > 100)
                {
                    validProficiencies = false;
                    break;
                }
                customClass.proficiencies.push_back(proficiencies[i].GetInt());
                customClass.proficiencies.push_back(proficiencies[i + 1].GetInt());
            }
            if (!validProficiencies)
            {
                printlog("[CustomClasses]: Skipping '%s': invalid proficiency id/value.",
                    customClass.internalName.c_str());
                continue;
            }
        }

        if (itr->HasMember("starting_spells"))
        {
            const rapidjson::Value& spells = (*itr)["starting_spells"];
            if (!spells.IsArray())
            {
                printlog("[CustomClasses]: Skipping '%s': starting_spells must be an array.",
                    customClass.internalName.c_str());
                continue;
            }

            bool validSpells = true;
            for (rapidjson::Value::ConstValueIterator spell = spells.Begin();
                 spell != spells.End(); ++spell)
            {
                if (!spell->IsInt() || spell->GetInt() < 0 || spell->GetInt() >= NUM_SPELLS)
                {
                    validSpells = false;
                    break;
                }
                customClass.startingSpells.push_back(spell->GetInt());
            }
            if (!validSpells)
            {
                printlog("[CustomClasses]: Skipping '%s': invalid starting spell ID.",
                    customClass.internalName.c_str());
                continue;
            }
        }

        if (itr->HasMember("starting_items"))
        {
            const rapidjson::Value& items = (*itr)["starting_items"];
            if (!items.IsArray())
            {
                printlog("[CustomClasses]: Skipping '%s': starting_items must be an array.",
                    customClass.internalName.c_str());
                continue;
            }

            bool validItems = true;
            for (rapidjson::Value::ConstValueIterator item = items.Begin();
                 item != items.End(); ++item)
            {
                if (!item->IsInt() || item->GetInt() < 0 || item->GetInt() >= NUMITEMS)
                {
                    validItems = false;
                    break;
                }
                customClass.startingItems.push_back(item->GetInt());
            }
            if (!validItems)
            {
                printlog("[CustomClasses]: Skipping '%s': invalid starting item ID.",
                    customClass.internalName.c_str());
                continue;
            }
        }

        usedIds.insert(customClass.id);
        usedNames.insert(customClass.internalName);
        classes.push_back(customClass);
    }

    printlog("[CustomClasses]: Loaded %d custom classes.",
        static_cast<int>(classes.size()));
}

bool CustomClassManager::save()
{
    if ( classes.size() > MAX_CUSTOM_CLASSES )
    {
        printlog("[CustomClasses]: Refusing to save more than %d classes.", MAX_CUSTOM_CLASSES);
        return false;
    }

    std::vector<CustomClass> normalizedClasses = classes;
    std::set<int> usedIds;
    std::set<std::string> usedNames;
    for ( CustomClass& customClass : normalizedClasses )
    {
        if ( customClass.id < 0 || customClass.id > MAX_CUSTOM_CLASS_ID
            || customClass.internalName.empty() || customClass.internalName.size() > 64
            || customClass.displayName.size() > 63 || customClass.description.size() > 512
            || usedIds.find(customClass.id) != usedIds.end()
            || usedNames.find(customClass.internalName) != usedNames.end()
            || customClass.maxHP < -30 || customClass.maxHP > 1000
            || customClass.maxMP < -30 || customClass.maxMP > 1000
            || customClass.STR < -100 || customClass.STR > 100
            || customClass.DEX < -100 || customClass.DEX > 100
            || customClass.CON < -100 || customClass.CON > 100
            || customClass.INT < -100 || customClass.INT > 100
            || customClass.PER < -100 || customClass.PER > 100
            || customClass.CHR < -100 || customClass.CHR > 100
            || (customClass.proficiencies.size() % 2) != 0)
        {
            printlog("[CustomClasses]: Refusing to save invalid class '%s'.",
                customClass.internalName.c_str());
            return false;
        }

        if ( customClass.displayName.empty() )
        {
            customClass.displayName = customClass.internalName;
        }
        if ( customClass.allowedRaces.empty() )
        {
            customClass.allowedRaces.push_back(RACE_HUMAN);
        }

        for ( int race : customClass.allowedRaces )
        {
            if ( race < 0 || race >= RACE_ENUM_END )
            {
                printlog("[CustomClasses]: Invalid race ID in '%s'.", customClass.internalName.c_str());
                return false;
            }
        }
        for ( size_t i = 0; i < customClass.proficiencies.size(); i += 2 )
        {
            if ( customClass.proficiencies[i] < 0
                || customClass.proficiencies[i] >= NUMPROFICIENCIES
                || customClass.proficiencies[i + 1] < 0
                || customClass.proficiencies[i + 1] > 100 )
            {
                printlog("[CustomClasses]: Invalid proficiency in '%s'.", customClass.internalName.c_str());
                return false;
            }
        }
        for ( int spell : customClass.startingSpells )
        {
            if ( spell < 0 || spell >= NUM_SPELLS )
            {
                printlog("[CustomClasses]: Invalid starting spell in '%s'.", customClass.internalName.c_str());
                return false;
            }
        }
        for ( int item : customClass.startingItems )
        {
            if ( item < 0 || item >= NUMITEMS )
            {
                printlog("[CustomClasses]: Invalid starting item in '%s'.", customClass.internalName.c_str());
                return false;
            }
        }

        usedIds.insert(customClass.id);
        usedNames.insert(customClass.internalName);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("version");
    writer.Int(2);
    writer.Key("classes");
    writer.StartArray();
    for ( const CustomClass& customClass : normalizedClasses )
    {
        writer.StartObject();
        writer.Key("id");
        writer.Int(customClass.id);
        writer.Key("internal_name");
        writer.String(customClass.internalName.c_str());
        writer.Key("display_name");
        writer.String(customClass.displayName.c_str());
        writer.Key("description");
        writer.String(customClass.description.c_str());
        writer.Key("max_hp");
        writer.Int(customClass.maxHP);
        writer.Key("max_mp");
        writer.Int(customClass.maxMP);
        writer.Key("STR");
        writer.Int(customClass.STR);
        writer.Key("DEX");
        writer.Int(customClass.DEX);
        writer.Key("CON");
        writer.Int(customClass.CON);
        writer.Key("INT");
        writer.Int(customClass.INT);
        writer.Key("PER");
        writer.Int(customClass.PER);
        writer.Key("CHR");
        writer.Int(customClass.CHR);

        writer.Key("allowed_races");
        writer.StartArray();
        for ( int race : customClass.allowedRaces )
        {
            writer.Int(race);
        }
        writer.EndArray();

        writer.Key("proficiencies");
        writer.StartArray();
        for ( int proficiency : customClass.proficiencies )
        {
            writer.Int(proficiency);
        }
        writer.EndArray();

        writer.Key("starting_spells");
        writer.StartArray();
        for ( int spell : customClass.startingSpells )
        {
            writer.Int(spell);
        }
        writer.EndArray();

        writer.Key("starting_items");
        writer.StartArray();
        for ( int item : customClass.startingItems )
        {
            writer.Int(item);
        }
        writer.EndArray();
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();

    PHYSFS_mkdir("custom");
    PHYSFS_File* file = PHYSFS_openWrite("custom/classes.json");
    if ( !file )
    {
        printlog("[CustomClasses]: Could not open classes.json for writing: %s",
            PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return false;
    }

    const PHYSFS_sint64 bytesWritten = PHYSFS_writeBytes(file, buffer.GetString(), buffer.GetSize());
    const bool closed = PHYSFS_close(file);
    if ( bytesWritten != static_cast<PHYSFS_sint64>(buffer.GetSize()) || !closed )
    {
        printlog("[CustomClasses]: Failed to write classes.json completely.");
        return false;
    }

    classes = std::move(normalizedClasses);
    printlog("[CustomClasses]: Saved %d custom classes.", static_cast<int>(classes.size()));
    return true;
}

bool CustomClassManager::addOrUpdateClass(const CustomClass& customClass)
{
    std::vector<CustomClass> previousClasses = classes;
    auto it = std::find_if(classes.begin(), classes.end(),
        [&customClass](const CustomClass& existing) { return existing.id == customClass.id; });
    if ( it == classes.end() )
    {
        if ( classes.size() >= MAX_CUSTOM_CLASSES )
        {
            return false;
        }
        classes.push_back(customClass);
    }
    else
    {
        *it = customClass;
    }

    if ( !save() )
    {
        classes = std::move(previousClasses);
        return false;
    }
    return true;
}

bool CustomClassManager::removeClass(int customId)
{
    std::vector<CustomClass> previousClasses = classes;
    auto it = std::find_if(classes.begin(), classes.end(),
        [customId](const CustomClass& existing) { return existing.id == customId; });
    if ( it == classes.end() )
    {
        return false;
    }
    classes.erase(it);
    if ( !save() )
    {
        classes = std::move(previousClasses);
        return false;
    }
    return true;
}

const std::vector<CustomClass>& CustomClassManager::getClasses()
{
    return classes;
}

bool CustomClassManager::isCustomClassId(int classId)
{
    return getCustomClassIndex(classId) >= 0;
}

int CustomClassManager::getCustomClassIndex(int classId)
{
    if (classId < NUMCLASSES)
    {
        return -1;
    }

    const int customId = classId - NUMCLASSES;
    for (size_t i = 0; i < classes.size(); ++i)
    {
        if (classes[i].id == customId)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

const CustomClass* CustomClassManager::getClass(int classId)
{
    const int index = getCustomClassIndex(classId);
    if (index < 0 || index >= static_cast<int>(classes.size()))
    {
        return nullptr;
    }
    return &classes[static_cast<size_t>(index)];
}
