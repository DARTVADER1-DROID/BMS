#include "domain/battery.h"
#include "domain/battery_fleet.h"

#include <stddef.h>

int main(void)
{
    BatteryFleet fleet = {0};
    Battery battery = {0};

    /* The scaffold models are plain data structures with zero-value support. */
    if (sizeof(fleet.fleet_id.value) != 37U
        || sizeof(fleet.owner_id.value) != 37U
        || sizeof(battery.battery_id.value) != 37U
        || sizeof(battery.fleet_id.value) != 37U) {
        return 1;
    }

    if (fleet.name != NULL
        || battery.name != NULL
        || battery.status != NULL
        || battery.configuration_json != NULL) {
        return 1;
    }

    return 0;
}
