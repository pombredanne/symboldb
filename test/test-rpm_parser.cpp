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

#include <cxxll/rpm_parser.hpp>
#include <cxxll/rpm_script.hpp>
#include <cxxll/rpm_trigger.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  // scripts()
  {
    rpm_parser parser("test/data/shared-mime-info-1.1-1.fc18.x86_64.rpm");
    std::vector<rpm_script> scripts;
    parser.scripts(scripts);
    COMPARE_NUMBER(scripts.size(), 1U);
    COMPARE_STRING(rpm_script::to_string(scripts.at(0).type), "postin");
    CHECK(scripts.at(0).script_present);
    COMPARE_NUMBER(scripts.at(0).prog.size(), 1U);
    COMPARE_STRING(scripts.at(0).prog.at(0), "/bin/sh");
    COMPARE_STRING(scripts.at(0).script,
      "# Should fail, as it would mean a problem in the mime database\n"
      "/usr/bin/update-mime-database /usr/share/mime &> /dev/null");
  }

  // triggers()
  {
    rpm_parser parser("test/data/firewalld-0.2.12-5.fc18.noarch.rpm");
    std::vector<rpm_trigger> triggers;
    parser.triggers(triggers);
    COMPARE_NUMBER(triggers.size(), 1U);
    COMPARE_STRING(triggers.at(0).script,
      "# Save the current service runlevel info\n"
      "# User must manually run systemd-sysv-convert --apply firewalld\n"
      "# to migrate them to systemd targets\n"
      "/usr/bin/systemd-sysv-convert --save firewalld >/dev/null 2>&1 ||:\n"
      "\n"
      "# Run these because the SysV package being removed won't do them\n"
      "/sbin/chkconfig --del firewalld >/dev/null 2>&1 || :\n"
      "/bin/systemctl try-restart firewalld.service >/dev/null 2>&1 || :");
    COMPARE_STRING(triggers.at(0).prog, "/bin/sh");
    COMPARE_NUMBER(triggers.at(0).conditions.size(), 1U);
    const rpm_trigger::condition &cond(triggers.at(0).conditions.at(0));
    COMPARE_STRING(cond.name, "firewalld");
    COMPARE_STRING(cond.version, "0.1.3-3");
  }
  {
    rpm_parser parser("test/data/cronie-1.4.10-7.fc19.x86_64.rpm");
    std::vector<rpm_trigger> triggers;
    parser.triggers(triggers);
    COMPARE_NUMBER(triggers.size(), 3U);
    for (rpm_trigger &trg : triggers) {
      COMPARE_STRING(trg.prog, "/bin/sh");
    }
    // triggerun scriptlet (using /bin/sh) -- cronie-anacron < 1.4.1
    COMPARE_STRING(triggers.at(0).script,
      "# empty /etc/crontab in case there are only old regular jobs\n"
      "cp -a /etc/crontab /etc/crontab.rpmsave\n"
      "sed -e '/^01 \\* \\* \\* \\* root run-parts \\/etc\\/cron\\.hourly/d'\\\n"
      "  -e '/^02 4 \\* \\* \\* root run-parts \\/etc\\/cron\\.daily/d'\\\n"
      "  -e '/^22 4 \\* \\* 0 root run-parts \\/etc\\/cron\\.weekly/d'\\\n"
      "  -e '/^42 4 1 \\* \\* root run-parts \\/etc\\/cron\\.monthly/d' /etc/crontab.rpmsave > /etc/crontab\n"
      "exit 0");
    COMPARE_NUMBER(triggers.at(0).conditions.size(), 1U);
    COMPARE_STRING(triggers.at(0).conditions.at(0).name, "cronie-anacron");
    COMPARE_STRING(triggers.at(0).conditions.at(0).version, "1.4.1");
    // triggerun scriptlet (using /bin/sh) -- cronie < 1.4.7-2\n"
    COMPARE_STRING(triggers.at(1).script,
      "# Save the current service runlevel info\n"
      "# User must manually run systemd-sysv-convert --apply crond\n"
      "# to migrate them to systemd targets\n"
      "/usr/bin/systemd-sysv-convert --save crond\n"
      "\n"
      "# The package is allowed to autostart:\n"
      "/bin/systemctl enable crond.service >/dev/null 2>&1\n"
      "\n"
      "/sbin/chkconfig --del crond >/dev/null 2>&1 || :\n"
      "/bin/systemctl try-restart crond.service >/dev/null 2>&1 || :\n"
      "/bin/systemctl daemon-reload >/dev/null 2>&1 || :");
    COMPARE_NUMBER(triggers.at(1).conditions.size(), 1U);
    COMPARE_STRING(triggers.at(1).conditions.at(0).name, "cronie");
    COMPARE_STRING(triggers.at(1).conditions.at(0).version, "1.4.7-2");
    // triggerin scriptlet (using /bin/sh) -- pam, glibc, libselinux\n"
    COMPARE_STRING(triggers.at(2).script,
      "# changes in pam, glibc or libselinux can make crond crash\n"
      "# when it calls pam\n"
      "/bin/systemctl try-restart crond.service >/dev/null 2>&1 || :");
    COMPARE_NUMBER(triggers.at(2).conditions.size(), 3U);
    COMPARE_STRING(triggers.at(2).conditions.at(0).name, "pam");
    CHECK(triggers.at(2).conditions.at(0).version.empty());
    COMPARE_STRING(triggers.at(2).conditions.at(1).name, "glibc");
    CHECK(triggers.at(2).conditions.at(1).version.empty());
    COMPARE_STRING(triggers.at(2).conditions.at(2).name, "libselinux");
    CHECK(triggers.at(2).conditions.at(2).version.empty());
  }
}

static test_register t("rpm_parser", test);
