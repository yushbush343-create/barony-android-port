#pragma once

#include <string>
#include <vector>

struct CustomClass
{
    // Stable user-defined ID. The engine class ID is NUMCLASSES + id.
    int id = -1;
    std::string internalName;
    std::string displayName;
    std::string description;

    // These are modifiers added to the base 30 HP / 30 MP,
    // matching the existing built-in class system.
    int maxHP = 0;
    int maxMP = 0;

    int STR = 0;
    int DEX = 0;
    int CON = 0;
    int INT = 0;
    int PER = 0;
    int CHR = 0;

    // Empty/omitted allowed_races defaults to human in the loader.
    std::vector<int> allowedRaces;
    std::vector<int> proficiencies;
    std::vector<int> startingSpells;
};

class CustomClassManager
{
public:
    static void init();
    static bool save();
    static bool addOrUpdateClass(const CustomClass& customClass);
    static bool removeClass(int customId);

    static const std::vector<CustomClass>& getClasses();

    // Custom class IDs occupy NUMCLASSES + [0, 127 - NUMCLASSES].
    static int getMaxCustomClassId();
    static bool isCustomClassId(int classId);
    static int getCustomClassIndex(int classId);
    static const CustomClass* getClass(int classId);

private:
    static std::vector<CustomClass> classes;
};
