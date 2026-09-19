#include "cest.h"

void run_system_tests();
void run_tools_tests();

int main(int argc, char* argv[])
{
    cest_init(argc, argv);

    run_system_tests();
    run_tools_tests();

    return cest_result();
}
