/*
 * Copyright (C) 2018 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "dnsmasq_process.h"

namespace mp = multipass;

namespace
{
static QString pid_file()
{
    QString snap_common = qgetenv("SNAP_COMMON");
    if (!snap_common.isEmpty())
    {
        return QString("%1/dnsmasq.pid").arg(snap_common);
    }
    else
    {
        return QString();
    }
}

} // namespace

mp::DNSMasqProcess::DNSMasqProcess(const mp::AppArmor& apparmor, const QDir& data_dir, const QString& bridge_name,
                                   const mp::IPAddress& bridge_addr, const mp::IPAddress& start_ip,
                                   const mp::IPAddress& end_ip)
    : mp::AppArmoredProcess(apparmor),
      data_dir(data_dir),
      bridge_name(bridge_name),
      pid_file{::pid_file()},
      bridge_addr(bridge_addr),
      start_ip(start_ip),
      end_ip(end_ip)
{
}

QString mp::DNSMasqProcess::program() const
{
    return QStringLiteral("dnsmasq");
}

QStringList mp::DNSMasqProcess::arguments() const
{
    QString pid;
    if (!pid_file.isNull())
    {
        pid = QString("--pid-file=%1").arg(pid_file);
    }

    return QStringList() << "--keep-in-foreground" << pid << "--strict-order"
                         << "--bind-interfaces"
                         << "--except-interface=lo" << QString("--interface=%1").arg(bridge_name)
                         << QString("--listen-address=%1").arg(QString::fromStdString(bridge_addr.as_string()))
                         << "--dhcp-no-override"
                         << "--dhcp-authoritative"
                         << QString("--dhcp-leasefile=%1").arg(data_dir.filePath("dnsmasq.leases"))
                         << QString("--dhcp-hostsfile=%1").arg(data_dir.filePath("dnsmasq.hosts")) << "--dhcp-range"
                         << QString("%1,%2,infinite")
                                .arg(QString::fromStdString(start_ip.as_string()))
                                .arg(QString::fromStdString(end_ip.as_string()));
}

QString mp::DNSMasqProcess::apparmor_profile() const
{
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>
    #include <abstractions/dbus>
    #include <abstractions/nameservice>

    capability chown,
    capability net_bind_service,
    capability setgid,
    capability setuid,
    capability dac_override,
    capability net_admin,         # for DHCP server
    capability net_raw,           # for DHCP server ping checks
    network inet raw,
    network inet6 raw,

    # access to iface mtu needed for Router Advertisement messages in IPv6
    # Neighbor Discovery protocol (RFC 2461)
    @{PROC}/sys/net/ipv6/conf/*/mtu r,

    %2 rw,           # Leases file
    %3 r,            # Hosts file

    %4
}
    )END");

    QString pid;
    if (pid_file.isNull())
    {
        pid = QString("/{,var/}run/*dnsmasq*.pid w,  # pid file");
    }

    return profile_template.arg(apparmor_profile_name(), data_dir.filePath("dnsmasq.leases"),
                                data_dir.filePath("dnsmasq.hosts"), pid);
}