#include "custom_class_editor.hpp"

#include "custom_classes.hpp"
#include "main.hpp"
#include "ui/Button.hpp"
#include "ui/Field.hpp"
#include "ui/Frame.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace CustomClassEditor
{
namespace
{
    enum class Action
    {
        None,
        Load,
        New,
        Save,
        Delete,
        Back
    };

    struct EditorFields
    {
        Field* id = nullptr;
        Field* internalName = nullptr;
        Field* displayName = nullptr;
        Field* description = nullptr;
        Field* maxHP = nullptr;
        Field* maxMP = nullptr;
        Field* str = nullptr;
        Field* dex = nullptr;
        Field* con = nullptr;
        Field* intelligence = nullptr;
        Field* per = nullptr;
        Field* chr = nullptr;
        Field* allowedRaces = nullptr;
        Field* proficiencies = nullptr;
        Field* startingSpells = nullptr;
        Field* status = nullptr;
    };

    Frame* rootFrame = nullptr;
    Frame* panelFrame = nullptr;
    Frame* listFrame = nullptr;
    Frame* formFrame = nullptr;
    EditorFields fields;
    std::vector<std::pair<Button*, int>> classButtons;
    int selectedCustomId = -1;
    int pendingCustomId = -1;
    Action pendingAction = Action::None;
    bool editorOpen = false;

    Uint32 color(const Uint8 r, const Uint8 g, const Uint8 b, const Uint8 a = 255)
    {
        return makeColor(r, g, b, a);
    }

    std::string trim(const std::string& value)
    {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if ( first == std::string::npos )
        {
            return "";
        }
        const size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::string fieldText(const Field* field)
    {
        return field && field->getText() ? trim(field->getText()) : "";
    }

    bool parseInteger(const std::string& text, int& value, const bool emptyIsZero = true)
    {
        const std::string clean = trim(text);
        if ( clean.empty() )
        {
            if ( emptyIsZero )
            {
                value = 0;
                return true;
            }
            return false;
        }

        char* end = nullptr;
        const long parsed = std::strtol(clean.c_str(), &end, 10);
        if ( end == clean.c_str() || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX )
        {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }

    bool parseIntegerField(const Field* field, int& value, const bool emptyIsZero = true)
    {
        return parseInteger(fieldText(field), value, emptyIsZero);
    }

    bool parseIntegerList(const std::string& text, std::vector<int>& values)
    {
        values.clear();
        const std::string clean = trim(text);
        if ( clean.empty() )
        {
            return true;
        }

        std::stringstream stream(clean);
        std::string token;
        while ( std::getline(stream, token, ',') )
        {
            int value = 0;
            if ( !parseInteger(token, value, false) )
            {
                return false;
            }
            values.push_back(value);
        }
        return !values.empty();
    }

    std::string integerListToString(const std::vector<int>& values)
    {
        std::ostringstream stream;
        for ( size_t i = 0; i < values.size(); ++i )
        {
            if ( i > 0 )
            {
                stream << ", ";
            }
            stream << values[i];
        }
        return stream.str();
    }

    const CustomClass* findCustomClass(const int customId)
    {
        const auto& classes = CustomClassManager::getClasses();
        auto it = std::find_if(classes.begin(), classes.end(),
            [customId](const CustomClass& customClass) { return customClass.id == customId; });
        return it == classes.end() ? nullptr : &(*it);
    }

    Field* addLabel(Frame& parent, const char* name, const char* text, const SDL_Rect& size)
    {
        Field* field = parent.addField(name, static_cast<int>(std::strlen(text)) + 1);
        field->setText(text);
        field->setSize(size);
        field->setEditable(false);
        field->setColor(color(255, 255, 255));
        field->setTextColor(color(218, 224, 235));
        field->setBackgroundColor(0);
        field->setBackgroundActivatedColor(0);
        field->setOutlineColor(0);
        return field;
    }

    Field* addInput(Frame& parent, const char* name, const int length,
        const SDL_Rect& size, const char* guide, const bool numbersOnly = false)
    {
        Field* field = parent.addField(name, length);
        field->setSize(size);
        field->setEditable(true);
        field->setNumbersOnly(numbersOnly);
        field->setColor(color(255, 255, 255));
        field->setTextColor(color(245, 247, 250));
        field->setOutlineColor(color(95, 110, 132));
        field->setBackgroundColor(color(37, 43, 55));
        field->setBackgroundActivatedColor(color(49, 59, 76));
        field->setBackgroundSelectAllColor(color(56, 82, 116));
        field->setGuide(guide);
        field->setScroll(true);
        return field;
    }

    Button* addButton(Frame& parent, const char* name, const char* text,
        const SDL_Rect& size, void (*callback)(Button&))
    {
        Button* button = parent.addButton(name);
        button->setText(text);
        button->setSize(size);
        button->setColor(color(48, 61, 82));
        button->setHighlightColor(color(74, 98, 132));
        button->setTextColor(color(245, 247, 250));
        button->setTextHighlightColor(color(255, 255, 255));
        button->setBorderColor(color(120, 135, 155));
        button->setBorder(1);
        button->setCallback(callback);
        return button;
    }

    void setFieldText(Field* field, const std::string& value)
    {
        if ( field )
        {
            field->deactivate();
            field->setText(value.c_str());
        }
    }

    void setStatus(const std::string& message)
    {
        setFieldText(fields.status, message);
    }

    int nextAvailableId()
    {
        const auto& classes = CustomClassManager::getClasses();
        for ( int id = 0; id <= 127 - NUMCLASSES; ++id )
        {
            const bool used = std::any_of(classes.begin(), classes.end(),
                [id](const CustomClass& customClass) { return customClass.id == id; });
            if ( !used )
            {
                return id;
            }
        }
        return -1;
    }

    void populateFields()
    {
        const CustomClass* customClass = findCustomClass(selectedCustomId);
        if ( !customClass )
        {
            setFieldText(fields.id, "");
            setFieldText(fields.internalName, "");
            setFieldText(fields.displayName, "");
            setFieldText(fields.description, "");
            setFieldText(fields.maxHP, "0");
            setFieldText(fields.maxMP, "0");
            setFieldText(fields.str, "0");
            setFieldText(fields.dex, "0");
            setFieldText(fields.con, "0");
            setFieldText(fields.intelligence, "0");
            setFieldText(fields.per, "0");
            setFieldText(fields.chr, "0");
            setFieldText(fields.allowedRaces, "");
            setFieldText(fields.proficiencies, "");
            setFieldText(fields.startingSpells, "");
            if ( fields.id )
            {
                fields.id->setEditable(true);
            }
            return;
        }

        setFieldText(fields.id, std::to_string(customClass->id));
        setFieldText(fields.internalName, customClass->internalName);
        setFieldText(fields.displayName, customClass->displayName);
        setFieldText(fields.description, customClass->description);
        setFieldText(fields.maxHP, std::to_string(customClass->maxHP));
        setFieldText(fields.maxMP, std::to_string(customClass->maxMP));
        setFieldText(fields.str, std::to_string(customClass->STR));
        setFieldText(fields.dex, std::to_string(customClass->DEX));
        setFieldText(fields.con, std::to_string(customClass->CON));
        setFieldText(fields.intelligence, std::to_string(customClass->INT));
        setFieldText(fields.per, std::to_string(customClass->PER));
        setFieldText(fields.chr, std::to_string(customClass->CHR));

        std::vector<int> races = customClass->allowedRaces;
        if ( races.size() == 1 && races.front() == RACE_HUMAN )
        {
            races.clear();
        }
        setFieldText(fields.allowedRaces, integerListToString(races));
        setFieldText(fields.proficiencies, integerListToString(customClass->proficiencies));
        setFieldText(fields.startingSpells, integerListToString(customClass->startingSpells));
        if ( fields.id )
        {
            fields.id->setEditable(false);
        }
    }

    void refreshClassList();

    void classButtonCallback(Button& button)
    {
        for ( const auto& entry : classButtons )
        {
            if ( entry.first == &button )
            {
                pendingCustomId = entry.second;
                pendingAction = Action::Load;
                return;
            }
        }
    }

    void newButtonCallback(Button&)
    {
        pendingAction = Action::New;
    }

    void saveButtonCallback(Button&)
    {
        pendingAction = Action::Save;
    }

    void deleteButtonCallback(Button&)
    {
        pendingAction = Action::Delete;
    }

    void backButtonCallback(Button&)
    {
        pendingAction = Action::Back;
    }

    void refreshClassList()
    {
        if ( !panelFrame )
        {
            return;
        }

        if ( listFrame )
        {
            auto& childFrames = panelFrame->getFrames();
            auto it = std::find(childFrames.begin(), childFrames.end(), listFrame);
            if ( it != childFrames.end() )
            {
                childFrames.erase(it);
            }
            delete listFrame;
            listFrame = nullptr;
        }
        classButtons.clear();

        listFrame = panelFrame->addFrame("customClassList");
        const int listHeight = std::max(100, panelFrame->getSize().h - 138);
        const int listWidth = 210;
        listFrame->setSize({ 20, 62, listWidth, listHeight });
        listFrame->setColor(color(25, 30, 40, 235));
        listFrame->setBorder(1);
        listFrame->setBorderColor(color(105, 120, 140));
        listFrame->setScrollBarsEnabled(true);

        const auto& classes = CustomClassManager::getClasses();
        const int contentHeight = std::max(listHeight, static_cast<int>(classes.size()) * 32 + 8);
        listFrame->setActualSize({ 0, 0, listWidth, contentHeight });

        int index = 0;
        for ( const CustomClass& customClass : classes )
        {
            const std::string buttonName = "class_" + std::to_string(customClass.id);
            Button* button = listFrame->addButton(buttonName.c_str());
            button->setText(customClass.displayName.empty()
                ? customClass.internalName.c_str() : customClass.displayName.c_str());
            button->setSize({ 4, 4 + index * 32, listWidth - 20, 28 });
            button->setColor(customClass.id == selectedCustomId
                ? color(70, 89, 120) : color(42, 50, 64));
            button->setHighlightColor(color(82, 108, 145));
            button->setTextColor(color(242, 245, 250));
            button->setTextHighlightColor(color(255, 255, 255));
            button->setBorderColor(color(105, 120, 140));
            button->setBorder(1);
            button->setTooltip(customClass.internalName.c_str());
            button->setCallback(classButtonCallback);
            classButtons.emplace_back(button, customClass.id);
            ++index;
        }

        if ( classes.empty() )
        {
            addLabel(*listFrame, "emptyClassList", "No custom classes yet.", { 8, 8, listWidth - 24, 28 });
        }
    }

    void buildUI()
    {
        const int screenWidth = Frame::virtualScreenX;
        const int screenHeight = Frame::virtualScreenY;
        const int panelWidth = std::min(920, screenWidth - 24);
        const int panelHeight = std::min(660, screenHeight - 24);

        rootFrame = new Frame("customClassEditorRoot");
        rootFrame->setSize({ 0, 0, screenWidth, screenHeight });
        rootFrame->setColor(color(0, 0, 0, 165));

        panelFrame = rootFrame->addFrame("customClassEditorPanel");
        panelFrame->setSize({ (screenWidth - panelWidth) / 2, (screenHeight - panelHeight) / 2,
            panelWidth, panelHeight });
        panelFrame->setColor(color(22, 27, 36, 250));
        panelFrame->setBorder(2);
        panelFrame->setBorderColor(color(145, 160, 180));

        Field* title = addLabel(*panelFrame, "editorTitle", "Custom Class Editor",
            { 20, 10, panelWidth - 40, 28 });
        title->setTextColor(color(255, 255, 255));
        Field* hint = addLabel(*panelFrame, "editorHint",
            "Choose a class on the left, edit its values, then Save. Changes are stored in app data.",
            { 20, 36, panelWidth - 40, 20 });
        hint->setTextColor(color(190, 202, 218));

        formFrame = panelFrame->addFrame("customClassForm");
        const int formWidth = panelWidth - 265;
        const int formHeight = panelHeight - 138;
        formFrame->setSize({ 245, 62, formWidth, formHeight });
        formFrame->setColor(0);

        addLabel(*formFrame, "idLabel", "ID", { 0, 0, 24, 24 });
        fields.id = addInput(*formFrame, "classId", 4, { 30, 0, 54, 24 },
            "Unique ID. Existing class IDs cannot be changed.", true);
        fields.id->setNumbersOnly(true);

        addLabel(*formFrame, "internalNameLabel", "Internal", { 96, 0, 62, 24 });
        fields.internalName = addInput(*formFrame, "internalName", 65, { 160, 0, 150, 24 },
            "Unique internal identifier, e.g. dune_sentinel.");

        addLabel(*formFrame, "displayNameLabel", "Display", { 318, 0, 58, 24 });
        fields.displayName = addInput(*formFrame, "displayName", 64,
            { 378, 0, formWidth - 382, 24 }, "Name shown in character creation.");

        addLabel(*formFrame, "descriptionLabel", "Description", { 0, 34, 100, 20 });
        fields.description = addInput(*formFrame, "description", 513,
            { 0, 54, formWidth - 8, 56 }, "Short class description.");
        fields.description->setScroll(true);

        const int statLabelWidth = 36;
        const int statFieldWidth = 48;
        const int statGap = 14;
        const int statY1 = 120;
        const int statY2 = 154;
        const int statX[4] = { 0, 120, 240, 360 };
        const char* statLabels1[4] = { "HP+", "MP+", "STR", "DEX" };
        const char* statLabels2[4] = { "CON", "INT", "PER", "CHR" };
        Field** statFields1[4] = { &fields.maxHP, &fields.maxMP, &fields.str, &fields.dex };
        Field** statFields2[4] = { &fields.con, &fields.intelligence, &fields.per, &fields.chr };
        const char* statNames1[4] = { "maxHP", "maxMP", "strength", "dexterity" };
        const char* statNames2[4] = { "constitution", "intelligence", "perception", "charisma" };

        for ( int i = 0; i < 4; ++i )
        {
            addLabel(*formFrame, (std::string("statLabel1_") + std::to_string(i)).c_str(),
                statLabels1[i], { statX[i], statY1, statLabelWidth, 24 });
            *statFields1[i] = addInput(*formFrame, statNames1[i], 6,
                { statX[i] + statLabelWidth, statY1, statFieldWidth, 24 },
                "Integer stat modifier.");
            addLabel(*formFrame, (std::string("statLabel2_") + std::to_string(i)).c_str(),
                statLabels2[i], { statX[i], statY2, statLabelWidth, 24 });
            *statFields2[i] = addInput(*formFrame, statNames2[i], 6,
                { statX[i] + statLabelWidth, statY2, statFieldWidth, 24 },
                "Integer stat modifier.");
        }

        addLabel(*formFrame, "allowedRacesLabel", "Allowed race IDs (blank = Human)", { 0, 190, 250, 20 });
        fields.allowedRaces = addInput(*formFrame, "allowedRaces", 256,
            { 0, 210, formWidth - 8, 24 }, "Comma-separated race IDs from the RACE enum.");

        addLabel(*formFrame, "proficienciesLabel", "Proficiency ID/value pairs", { 0, 240, 250, 20 });
        fields.proficiencies = addInput(*formFrame, "proficiencies", 256,
            { 0, 260, formWidth - 8, 24 }, "Comma-separated pairs, e.g. 2,25,5,40.");

        addLabel(*formFrame, "startingSpellsLabel", "Starting spell IDs", { 0, 290, 250, 20 });
        fields.startingSpells = addInput(*formFrame, "startingSpells", 256,
            { 0, 310, formWidth - 8, 24 }, "Comma-separated spell IDs.");

        fields.status = formFrame->addField("editorStatus", 256);
        fields.status->setText("Ready.");
        fields.status->setSize({ 0, 400, formWidth - 8, 30 });
        fields.status->setEditable(false);
        fields.status->setColor(color(255, 255, 255));
        fields.status->setTextColor(color(190, 220, 190));
        fields.status->setBackgroundColor(0);
        fields.status->setBackgroundActivatedColor(0);
        fields.status->setOutlineColor(0);

        const int buttonY = panelHeight - 52;
        addButton(*panelFrame, "newClass", "New", { 245, buttonY, 84, 30 }, newButtonCallback);
        addButton(*panelFrame, "saveClass", "Save", { 337, buttonY, 84, 30 }, saveButtonCallback);
        addButton(*panelFrame, "deleteClass", "Delete", { 429, buttonY, 84, 30 }, deleteButtonCallback);
        addButton(*panelFrame, "backToGame", "Back", { 521, buttonY, 84, 30 }, backButtonCallback);

        refreshClassList();
    }

    void closeUI()
    {
        delete rootFrame;
        rootFrame = nullptr;
        panelFrame = nullptr;
        listFrame = nullptr;
        formFrame = nullptr;
        fields = EditorFields{};
        classButtons.clear();
    }

    void beginNewClass()
    {
        const int id = nextAvailableId();
        if ( id < 0 )
        {
            setStatus("Class limit reached (64 definitions).");
            return;
        }
        selectedCustomId = -1;
        setFieldText(fields.id, std::to_string(id));
        setFieldText(fields.internalName, "custom_class_" + std::to_string(id));
        setFieldText(fields.displayName, "New Custom Class");
        setFieldText(fields.description, "");
        setFieldText(fields.maxHP, "0");
        setFieldText(fields.maxMP, "0");
        setFieldText(fields.str, "0");
        setFieldText(fields.dex, "0");
        setFieldText(fields.con, "0");
        setFieldText(fields.intelligence, "0");
        setFieldText(fields.per, "0");
        setFieldText(fields.chr, "0");
        setFieldText(fields.allowedRaces, "");
        setFieldText(fields.proficiencies, "");
        setFieldText(fields.startingSpells, "");
        if ( fields.id )
        {
            fields.id->setEditable(true);
        }
        refreshClassList();
        setStatus("New class. Fill in the fields and press Save.");
    }

    void saveFields()
    {
        CustomClass customClass;
        if ( !parseIntegerField(fields.id, customClass.id, false)
            || !parseIntegerField(fields.maxHP, customClass.maxHP)
            || !parseIntegerField(fields.maxMP, customClass.maxMP)
            || !parseIntegerField(fields.str, customClass.STR)
            || !parseIntegerField(fields.dex, customClass.DEX)
            || !parseIntegerField(fields.con, customClass.CON)
            || !parseIntegerField(fields.intelligence, customClass.INT)
            || !parseIntegerField(fields.per, customClass.PER)
            || !parseIntegerField(fields.chr, customClass.CHR) )
        {
            setStatus("Invalid number. Check the ID and stat fields.");
            return;
        }

        customClass.internalName = fieldText(fields.internalName);
        customClass.displayName = fieldText(fields.displayName);
        customClass.description = fields.description && fields.description->getText()
            ? fields.description->getText() : "";

        if ( customClass.internalName.empty() )
        {
            setStatus("Internal name is required.");
            return;
        }

        // Creating a new class must not silently replace an existing ID.
        if ( selectedCustomId < 0 && findCustomClass(customClass.id) )
        {
            setStatus("That class ID is already in use. Choose another ID.");
            return;
        }

        if ( selectedCustomId >= 0 && customClass.id != selectedCustomId )
        {
            setStatus("Existing class IDs cannot be changed.");
            return;
        }

        if ( !parseIntegerList(fieldText(fields.allowedRaces), customClass.allowedRaces)
            || !parseIntegerList(fieldText(fields.proficiencies), customClass.proficiencies)
            || !parseIntegerList(fieldText(fields.startingSpells), customClass.startingSpells)
 )
        {
            setStatus("List fields must contain comma-separated integers.");
            return;
        }

        if ( !CustomClassManager::addOrUpdateClass(customClass) )
        {
            setStatus("Save failed. Check IDs, ranges, duplicates, and app storage.");
            return;
        }

        selectedCustomId = customClass.id;
        if ( client_classes[0] == NUMCLASSES + customClass.id && stats && stats[0] )
        {
            stats[0]->clearStats();
            initClass(0);
        }
        refreshClassList();
        populateFields();
        setStatus("Saved. Changes are stored in app data.");
    }

    void deleteSelectedClass()
    {
        if ( selectedCustomId < 0 )
        {
            setStatus("Select an existing class before deleting.");
            return;
        }

        const int deletedId = selectedCustomId;
        if ( !CustomClassManager::removeClass(deletedId) )
        {
            setStatus("Delete failed.");
            return;
        }

        if ( client_classes[0] == NUMCLASSES + deletedId && stats && stats[0] )
        {
            client_classes[0] = CLASS_BARBARIAN;
            stats[0]->clearStats();
            initClass(0);
        }

        selectedCustomId = -1;
        refreshClassList();
        populateFields();
        setStatus("Class deleted. Existing saves using it may fall back to Barbarian.");
    }

    void processPendingAction()
    {
        const Action action = pendingAction;
        pendingAction = Action::None;

        switch ( action )
        {
            case Action::Load:
                selectedCustomId = pendingCustomId;
                populateFields();
                refreshClassList();
                setStatus("Editing selected class.");
                break;
            case Action::New:
                beginNewClass();
                break;
            case Action::Save:
                saveFields();
                break;
            case Action::Delete:
                deleteSelectedClass();
                break;
            case Action::Back:
                editorOpen = false;
                break;
            case Action::None:
            default:
                break;
        }
    }
}

void open()
{
    if ( editorOpen )
    {
        return;
    }
    closeUI();
    editorOpen = true;
    selectedCustomId = -1;
    pendingCustomId = -1;
    pendingAction = Action::None;
    buildUI();

    const auto& classes = CustomClassManager::getClasses();
    if ( !classes.empty() )
    {
        selectedCustomId = classes.front().id;
        populateFields();
    }
    else
    {
        beginNewClass();
    }
    refreshClassList();
}

bool draw()
{
    if ( !editorOpen || !rootFrame )
    {
        return false;
    }

    Frame::predraw();
    rootFrame->process();
    processPendingAction();
    if ( editorOpen && rootFrame )
    {
        rootFrame->draw();
    }
    Frame::postdraw();

    if ( !editorOpen )
    {
        closeUI();
    }
    return editorOpen;
}
}
