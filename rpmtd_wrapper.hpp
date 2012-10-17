#pragma once

#include <rpm/rpmtd.h>

// Simple wrapper around an rpmtd value.
struct rpmtd_wrapper {
  rpmtd raw;
 
  rpmtd_wrapper()
    : raw(rpmtdNew())
  {
  }

  // Frees also the pointed-to data.
  // If this is not what you want, call reset() first.
  ~rpmtd_wrapper();

  // Invokes rpmtdReset(raw) unless raw is NULL.
  void reset();

private:
  rpmtd_wrapper(const rpmtd_wrapper &); // not implemented
  rpmtd_wrapper &operator=(const rpmtd_wrapper &); // not implemented
};
