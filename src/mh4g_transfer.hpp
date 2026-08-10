#ifndef MH4G_TRANSFER_HPP
#define MH4G_TRANSFER_HPP

#include "mh4g_ui_compat.hpp"

#include <array>
#include <string>
#include <vector>

namespace MH3U_Transfer
{
struct chest_entry_t
{
    std::uint32_t panel;
    std::uint32_t slot;
    std::uint16_t itemId;
    std::uint16_t count;
};

struct equipment_entry_t
{
    std::uint32_t panel;
    std::uint32_t slot;
    std::array<std::uint8_t, EQUIPMENT_SIZE> bytes;
};

std::string exportChest(const save_t &save);
std::string exportEquipmentBox(const save_t &save);
bool parseChest(const std::string &form, std::vector<chest_entry_t> &entries, std::string &error);
bool parseEquipmentBox(const std::string &form, std::vector<equipment_entry_t> &entries, std::string &error);
void applyChest(const std::vector<chest_entry_t> &entries, save_t &save);
void applyEquipmentBox(const std::vector<equipment_entry_t> &entries, save_t &save);
}

#endif
