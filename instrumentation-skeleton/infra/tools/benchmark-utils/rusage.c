#include <sys/time.h>
#include <sys/resource.h>
#include "report.h"

__attribute__((destructor))
static void report_rusage() {
    struct rusage u;

    if (getrusage(RUSAGE_SELF, &u) < 0) {
        perror("getrusage");
        return;
    }

    report_begin();
    reporti("maxrss_kb", u.ru_maxrss);
    reporti("page_faults", u.ru_minflt + u.ru_majflt);
    reporti("io_operations", u.ru_inblock + u.ru_oublock);
    reporti("context_switches", u.ru_nvcsw + u.ru_nivcsw);
    report_end();
}
