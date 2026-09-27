#include "custom_classes.hpp"
#include "mod_tools.hpp"

std::vector<CustomClass> CustomClassManager::classes;

void CustomClassManager::init()
{
    classes.clear();

    if ( !PHYSFS_getRealDir("/custom/classes.json") )
    {
        printlog("[CustomClasses]: No custom/classes.json found.");
        return;
    }

    std::string inputPath = PHYSFS_getRealDir("/custom/classes.json");
    inputPath.append("/custom/classes.json");

    File* fp = FileIO::open(inputPath.c_str(), "rb");
    if ( !fp )
    {
        printlog("[CustomClasses]: Could not open %s", inputPath.c_str());
        return;
    }

    char buf[65536];
    int count = fp->read(buf, sizeof(buf[0]), sizeof(buf) - 1);
    buf[count] = '\0';
    FileIO::close(fp);

    rapidjson::StringStream is(buf);
    rapidjson::Document d;
    d.ParseStream(is);

    if ( d.HasParseError() || !d.HasMember("version") || !d.HasMember("classes") )
    {
        printlog("[CustomClasses]: Invalid classes.json.");
        return;
    }

    const rapidjson::Value& classList = d["classes"];

    if ( !classList.IsArray() )
    {
        printlog("[CustomClasses]: 'classes' must be an array.");
        return;
    }

    for ( rapidjson::Value::ConstValueIterator itr = classList.Begin(); itr != classList.End(); ++itr )
    {
        if ( !itr->IsObject() )
        {
            continue;
        }

        CustomClass customClass;

        if ( itr->HasMember("internal_name") && (*itr)["internal_name"].IsString() )
        {
            customClass.internalName = (*itr)["internal_name"].GetString();
        }

        if ( itr->HasMember("display_name") && (*itr)["display_name"].IsString() )
        {
            customClass.displayName = (*itr)["display_name"].GetString();
        }

        if ( itr->HasMember("max_hp") && (*itr)["max_hp"].IsInt() )
        {
            customClass.maxHP = (*itr)["max_hp"].GetInt();
        }

        if ( itr->HasMember("max_mp") && (*itr)["max_mp"].IsInt() )
        {
            customClass.maxMP = (*itr)["max_mp"].GetInt();
        }

        const char* statNames[] = { "STR", "DEX", "CON", "INT", "PER", "CHR" };
        int* statValues[] =
        {
            &customClass.STR,
            &customClass.DEX,
            &customClass.CON,
            &customClass.INT,
            &customClass.PER,
            &customClass.CHR
        };

        for ( int i = 0; i < 6; ++i )
        {
            if ( itr->HasMember(statNames[i]) && (*itr)[statNames[i]].IsInt() )
            {
                *statValues[i] = (*itr)[statNames[i]].GetInt();
            }
        }

        if ( itr->HasMember("proficiencies") && (*itr)["proficiencies"].IsArray() )
        {
            for ( rapidjson::Value::ConstValueIterator prof = (*itr)["proficiencies"].Begin();
                  prof != (*itr)["proficiencies"].End(); ++prof )
            {
                if ( prof->IsInt() )
                {
                    customClass.proficiencies.push_back(prof->GetInt());
                }
            }
        }

        if ( itr->HasMember("starting_spells") && (*itr)["starting_spells"].IsArray() )
        {
            for ( rapidjson::Value::ConstValueIterator spell = (*itr)["starting_spells"].Begin();
                  spell != (*itr)["starting_spells"].End(); ++spell )
            {
                if ( spell->IsInt() )
                {
                    customClass.startingSpells.push_back(spell->GetInt());
                }
            }
        }

        if ( itr->HasMember("starting_items") && (*itr)["starting_items"].IsArray() )
        {
            for ( rapidjson::Value::ConstValueIterator item = (*itr)["starting_items"].Begin();
                  item != (*itr)["starting_items"].End(); ++item )
            {
                if ( item->IsInt() )
                {
                    customClass.startingItems.push_back(item->GetInt());
                }
            }
        }

        if ( customClass.internalName.empty() )
        {
            continue;
        }

        if ( customClass.displayName.empty() )
        {
            customClass.displayName = customClass.internalName;
        }

        classes.push_back(customClass);
    }

    printlog("[CustomClasses]: Loaded %d custom classes.", static_cast<int>(classes.size()));
}

const std::vector<CustomClass>& CustomClassManager::getClasses()
{
    return classes;
}

bool CustomClassManager::isCustomClassId(int classId)
{
    return classId >= NUMCLASSES
        && classId < NUMCLASSES + static_cast<int>(classes.size());
}

int CustomClassManager::getCustomClassIndex(int classId)
{
    if ( !isCustomClassId(classId) )
    {
        return -1;
    }

    return classId - NUMCLASSES;
}

const CustomClass* CustomClassManager::getClass(int classId)
{
    const int index = getCustomClassIndex(classId);

    if ( index < 0 || index >= static_cast<int>(classes.size()) )
    {
        return nullptr;
    }

    return &classes[index];
}
