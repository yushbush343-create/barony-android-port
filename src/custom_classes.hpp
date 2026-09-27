#pragma once

#include <string>
#include <vector>

struct CustomClass
{
    std::string internalName;
    std::string displayName;

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

    std::vector<int> proficiencies;

    std::vector<int> startingSpells;
    std::vector<int> startingItems;
};

class CustomClassManager
{
public:
    static void init();

    static const std::vector<CustomClass>& getClasses();

    // Custom class IDs start immediately after the built-in classes.
    static bool isCustomClassId(int classId);
    static int getCustomClassIndex(int classId);
    static const CustomClass* getClass(int classId);

private:
    static std::vector<CustomClass> classes;
};
