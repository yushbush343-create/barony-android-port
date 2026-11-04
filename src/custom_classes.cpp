#include "main.hpp"
#include "custom_classes.hpp"
#include "items.hpp"
#include "magic/magic.hpp"
#include "stat.hpp"

#include <algorithm>
#include <set>
#include <vector>

#include "rapidjson/document.h"

namespace
{
constexpr PHYSFS_sint64 MAX_CUSTOM_CLASSES_FILE_SIZE = 1024 * 1024;
constexpr int MAX_CUSTOM_CLASSES = 64;
constexpr int MAX_CUSTOM_CLASS_ID = 255;

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
        if (customClass.displayName.size() > 128)
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
