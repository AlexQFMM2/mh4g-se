#include "mh4g_transfer.hpp"

#include <cstring>
#include <limits>
#include <sstream>

namespace
{
const char *Magic = "MH4G_SAVE_EDITOR_FORM";
const char *Version = "1";

std::string trim(std::string value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    std::size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t')) ++first;
    return value.substr(first);
}

std::vector<std::string> split(const std::string &line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) fields.push_back(trim(field));
    if (!line.empty() && line.back() == ',') fields.emplace_back();
    return fields;
}

bool number(const std::string &field, std::uint32_t maximum, std::uint32_t &result)
{
    if (field.empty()) return false;
    std::size_t used = 0;
    try
    {
        const unsigned long value = std::stoul(field, &used, 10);
        if (used != field.size() || value > maximum) return false;
        result = static_cast<std::uint32_t>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool preamble(std::istringstream &stream, const char *kind, const std::string &header,
              std::size_t &lineNumber, std::string &error)
{
    std::string line;
    if (!std::getline(stream, line) || trim(line) != std::string(Magic) + ',' + Version + ',' + kind)
    {
        error = "This is not a supported MH4G " + std::string(kind) + " form.";
        return false;
    }
    ++lineNumber;
    if (!std::getline(stream, line) || trim(line) != header)
    {
        error = "The form header does not match this version.";
        return false;
    }
    ++lineNumber;
    return true;
}

std::string lineError(std::size_t line, const std::string &message)
{
    return "Line " + std::to_string(line) + ": " + message;
}

std::string equipmentHeader()
{
    std::ostringstream output;
    output << "page,slot,equipment_type";
    for (int index = 0; index < EQUIPMENT_SIZE; ++index) output << ",byte_" << index;
    return output.str();
}
}

namespace MH3U_Transfer
{
std::string exportChest(const save_t &save)
{
    std::ostringstream output;
    output << Magic << ',' << Version << ",item_box\r\npage,slot,item_id,count\r\n";
    for (std::uint32_t page = 0; page < 14; ++page)
        for (std::uint32_t slot = 0; slot < 100; ++slot)
        {
            const item_t &item = save.chest[page][slot];
            output << page + 1 << ',' << slot + 1 << ',' << item.id << ',' << item.count << "\r\n";
        }
    return output.str();
}

std::string exportEquipmentBox(const save_t &save)
{
    std::ostringstream output;
    output << Magic << ',' << Version << ",equipment_box\r\n" << equipmentHeader() << "\r\n";
    for (std::uint32_t page = 0; page < 15; ++page)
        for (std::uint32_t slot = 0; slot < 100; ++slot)
        {
            const equipment_t &equipment = save.box[page][slot];
            output << page + 1 << ',' << slot + 1 << ',' << static_cast<unsigned>(equipment[0]);
            for (int index = 0; index < EQUIPMENT_SIZE; ++index)
                output << ',' << static_cast<unsigned>(equipment[index]);
            output << "\r\n";
        }
    return output.str();
}

bool parseChest(const std::string &form, std::vector<chest_entry_t> &entries, std::string &error)
{
    entries.clear();
    error.clear();
    std::istringstream stream(form);
    std::size_t line = 0;
    if (!preamble(stream, "item_box", "page,slot,item_id,count", line, error)) return false;
    bool seen[14][100] = {};
    std::string row;
    while (std::getline(stream, row))
    {
        ++line;
        if (trim(row).empty()) continue;
        const auto fields = split(row);
        std::uint32_t page = 0, slot = 0, id = 0, count = 0;
        if (fields.size() != 4) error = lineError(line, "expected 4 columns.");
        else if (!number(fields[0], 14, page) || page == 0) error = lineError(line, "page must be 1-14.");
        else if (!number(fields[1], 100, slot) || slot == 0) error = lineError(line, "slot must be 1-100.");
        else if (!number(fields[2], 65535, id)) error = lineError(line, "item_id must be 0-65535.");
        else if (!number(fields[3], 65535, count)) error = lineError(line, "count must be 0-65535.");
        else if (seen[page - 1][slot - 1]) error = lineError(line, "duplicate page and slot.");
        if (!error.empty()) { entries.clear(); return false; }
        seen[page - 1][slot - 1] = true;
        entries.push_back({page - 1, slot - 1, static_cast<std::uint16_t>(id), static_cast<std::uint16_t>(count)});
    }
    if (entries.empty()) { error = "The form contains no item slots."; return false; }
    return true;
}

bool parseEquipmentBox(const std::string &form, std::vector<equipment_entry_t> &entries, std::string &error)
{
    entries.clear();
    error.clear();
    std::istringstream stream(form);
    std::size_t line = 0;
    if (!preamble(stream, "equipment_box", equipmentHeader(), line, error)) return false;
    bool seen[15][100] = {};
    std::string row;
    while (std::getline(stream, row))
    {
        ++line;
        if (trim(row).empty()) continue;
        const auto fields = split(row);
        std::uint32_t page = 0, slot = 0, type = 0;
        if (fields.size() != static_cast<std::size_t>(3 + EQUIPMENT_SIZE)) error = lineError(line, "wrong column count.");
        else if (!number(fields[0], 15, page) || page == 0) error = lineError(line, "page must be 1-15.");
        else if (!number(fields[1], 100, slot) || slot == 0) error = lineError(line, "slot must be 1-100.");
        else if (!number(fields[2], 20, type)) error = lineError(line, "equipment_type must be 0-20.");
        else if (seen[page - 1][slot - 1]) error = lineError(line, "duplicate page and slot.");
        equipment_entry_t entry{};
        entry.panel = page > 0 ? page - 1 : 0;
        entry.slot = slot > 0 ? slot - 1 : 0;
        if (error.empty())
            for (int index = 0; index < EQUIPMENT_SIZE; ++index)
            {
                std::uint32_t byte = 0;
                if (!number(fields[3 + index], 255, byte))
                {
                    error = lineError(line, "byte_" + std::to_string(index) + " must be 0-255.");
                    break;
                }
                entry.bytes[index] = static_cast<std::uint8_t>(byte);
            }
        if (error.empty() && entry.bytes[0] != type) error = lineError(line, "equipment_type must equal byte_0.");
        if (!error.empty()) { entries.clear(); return false; }
        seen[page - 1][slot - 1] = true;
        entries.push_back(entry);
    }
    if (entries.empty()) { error = "The form contains no equipment slots."; return false; }
    return true;
}

void applyChest(const std::vector<chest_entry_t> &entries, save_t &save)
{
    for (const auto &entry : entries)
        save.chest[entry.panel][entry.slot] = {entry.itemId, entry.count};
}

void applyEquipmentBox(const std::vector<equipment_entry_t> &entries, save_t &save)
{
    for (const auto &entry : entries)
        std::memcpy(save.box[entry.panel][entry.slot], entry.bytes.data(), EQUIPMENT_SIZE);
}
}
