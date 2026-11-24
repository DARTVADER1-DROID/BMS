#include "application/battery/battery_repository.h"

bool battery_repository_is_valid(const BatteryRepository *repository)
{
    return repository != NULL
        && repository->create != NULL
        && repository->find_all != NULL
        && repository->find_by_id != NULL
        && repository->update != NULL
        && repository->delete != NULL;
}
