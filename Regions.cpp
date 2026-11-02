#include "Regions.h"

#include <ctype.h>
#include <stddef.h>

// Channel ranges for 2.4 GHz. Select the country where the device is operated.
static const Region kRegions[] = {
    {"DE", "Germany", 1, 13},       {"AT", "Austria", 1, 13},        {"CH", "Switzerland", 1, 13},
    {"FR", "France", 1, 13},        {"IT", "Italy", 1, 13},          {"ES", "Spain", 1, 13},
    {"NL", "Netherlands", 1, 13},   {"BE", "Belgium", 1, 13},        {"LU", "Luxembourg", 1, 13},
    {"DK", "Denmark", 1, 13},       {"SE", "Sweden", 1, 13},         {"NO", "Norway", 1, 13},
    {"FI", "Finland", 1, 13},       {"PL", "Poland", 1, 13},         {"CZ", "Czechia", 1, 13},
    {"SK", "Slovakia", 1, 13},      {"HU", "Hungary", 1, 13},        {"SI", "Slovenia", 1, 13},
    {"HR", "Croatia", 1, 13},       {"PT", "Portugal", 1, 13},       {"GR", "Greece", 1, 13},
    {"IE", "Ireland", 1, 13},       {"GB", "United Kingdom", 1, 13}, {"US", "United States", 1, 11},
    {"CA", "Canada", 1, 11},        {"AU", "Australia", 1, 13},      {"NZ", "New Zealand", 1, 13},
    {"JP", "Japan", 1, 14},         {"01", "World safe (channels 1-11)", 1, 11},
};

int regionCount() { return (int)(sizeof kRegions / sizeof kRegions[0]); }
const Region& regionAt(int i) { return kRegions[i]; }
const Region& defaultRegion() { return kRegions[0]; }

const Region* findRegion(const char* cc) {
  if (!cc || !cc[0] || !cc[1] || cc[2]) return nullptr;
  for (int i = 0; i < regionCount(); ++i)
    if (toupper((unsigned char)cc[0]) == kRegions[i].cc[0] && toupper((unsigned char)cc[1]) == kRegions[i].cc[1]) return &kRegions[i];
  return nullptr;
}
