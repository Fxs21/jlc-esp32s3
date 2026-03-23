#include "HAL.h"

namespace {

HAL::CommitFunc_t sMagCommit = nullptr;
void *sMagCommitUser = nullptr;

}  // namespace

bool HAL::MAG_Init()
{
    return false;
}

void HAL::MAG_SetCommitCallback(CommitFunc_t func, void *userData)
{
    sMagCommit = func;
    sMagCommitUser = userData;
}

void HAL::MAG_Update()
{
    (void)sMagCommit;
    (void)sMagCommitUser;
}
