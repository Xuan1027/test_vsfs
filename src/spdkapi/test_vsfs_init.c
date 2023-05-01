#include "vsfs_init.h"
#include "spdk.h"

int main(int argc, char const *argv[])
{
    init_spdk();
    init_vsfs("vsfs");
    mount_vsfs("vsfs");
    exit_spdk();
    return 0;
}
