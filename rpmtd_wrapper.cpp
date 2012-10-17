#include "rpmtd_wrapper.hpp"

rpmtd_wrapper::~rpmtd_wrapper()
{
  if (raw != NULL) {
    rpmtdFreeData(raw);
    rpmtdFree(raw);
  }
}

void
rpmtd_wrapper::reset()
{
  if (raw != NULL) {
    rpmtdReset(raw);
  }
}

