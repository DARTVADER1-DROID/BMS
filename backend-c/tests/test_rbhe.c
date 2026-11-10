#include "rbhe.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    Rbhe e;
    rbhe_init(&e, NULL);
    assert(rbhe_begin(&e, 0.0, 4.00, 25.0) == RBHE_OK);
    const RbheSample s[] = {
        {1.0, 3.98, 0.50, 25.0},
        {2.0, 3.97, 0.50, 25.0},
        {3.0, 3.96, 0.50, 25.1},
        {4.0, 3.95, 0.50, 25.1},
        {5.0, 3.98, 0.00, 25.1},
        {6.0, 3.99, 0.00, 25.1}
    };
    for (size_t i = 0; i < sizeof(s)/sizeof(s[0]); ++i)
        assert(rbhe_sample(&e, &s[i]) == RBHE_OK);
    RbheResultData out;
    assert(rbhe_finish(&e, &out) == RBHE_OK);
    assert(out.valid);
    assert(out.step_resistance_ohm > 0.0);
    assert(out.steady_resistance_ohm > 0.0);
    assert(!out.soh_available); /* No chemistry/cell-calibrated training model. */
    assert(!out.rul_available);

    assert(rbhe_add_health_history(&e, 0, 100) == RBHE_OK);
    assert(rbhe_add_health_history(&e, 50, 96) == RBHE_OK);
    assert(rbhe_add_health_history(&e, 100, 91) == RBHE_OK);
    double rul = 0.0;
    assert(rbhe_estimate_rul(&e, 91, 80, &rul) == RBHE_OK);
    assert(rul > 0.0 && isfinite(rul));

    puts("RBHE tests passed");
    return 0;
}
