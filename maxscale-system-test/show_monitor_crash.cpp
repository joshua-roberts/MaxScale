/**
 * Regression case for crash if 'show monitors' command is issued, but no monitor is not running
 *
 * - maxscale.cnf contains wrong monitor config (user name is wrong)
 * - issue 'show monitors' command
 * - check for crash
 */

#include "testconnections.h"

using namespace std;

int main(int argc, char* argv[])
{
    TestConnections test(argc, argv);
    test.check_maxctrl("show monitors");
    test.log_includes(0, "Auth Error, Down");
    return test.global_result;
}
