#include "application/fleet/fleet_repository.h"

#include <stddef.h>

bool fleet_repository_is_valid(const FleetRepository *repository)
{
    return repository != NULL
        && repository->create != NULL
        && repository->find_by_user != NULL
        && repository->update != NULL
        && repository->delete_by_user != NULL;
}
