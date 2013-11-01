/*
 * Copyright (C) 2013 Red Hat, Inc.
 * Written by Florian Weimer <fweimer@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cxxll/rpmfi_handle.hpp>
#include <cxxll/rpm_parser_exception.hpp>

cxxll::rpmfi_handle::rpmfi_handle(Header header)
{
  raw_ = rpmfiNew(0, header, 0, 0);
  if (raw_ == nullptr) {
    throw rpm_parser_exception("rpmfiNew failed");
  }
}

cxxll::rpmfi_handle::~rpmfi_handle()
{
  close();
}

void
cxxll::rpmfi_handle::close()
{
  if (raw_ != nullptr) {
    rpmfiFree(raw_);
    raw_ = nullptr;
  }
}

void
cxxll::rpmfi_handle::reset_header(Header header)
{
  rpmfi fi = rpmfiNew(0, header, 0, 0);
  if (fi == nullptr) {
    throw rpm_parser_exception("rpmfiNew failed");
  }
  close();
  raw_ = fi;
}
