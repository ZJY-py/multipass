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

#include <multipass/client_cert_store.h>
#include <multipass/utils.h>

#include "biomem.h"

#include <openssl/pem.h>
#include <openssl/x509.h>

#include <QDir>
#include <QFile>

#include <stdexcept>

namespace mp = multipass;

namespace
{
constexpr auto chain_name = "multipass_client_certs.pem";

void validate_certificate(const std::string& pem_cert)
{
    mp::BIOMem bio{pem_cert};
    auto raw_cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    std::unique_ptr<X509, decltype(X509_free)*> x509{raw_cert, X509_free};

    if (raw_cert == nullptr)
        throw std::runtime_error("invalid certificate data");
}
} // namespace

mp::ClientCertStore::ClientCertStore(const multipass::Path& cert_dir) : cert_dir{cert_dir}
{
}

void mp::ClientCertStore::add_cert(const std::string& pem_cert)
{
    validate_certificate(pem_cert);
    QDir dir{cert_dir};
    QFile file{dir.filePath(chain_name)};
    auto opened = file.open(QIODevice::WriteOnly | QIODevice::Append);
    if (!opened)
        throw std::runtime_error("failed to create file to store certificate");

    size_t written = file.write(pem_cert.data(), pem_cert.size());
    if (written != pem_cert.size())
        throw std::runtime_error("failed to write certificate");
}

std::string mp::ClientCertStore::PEM_cert_chain() const
{
    QDir dir{cert_dir};
    auto path = dir.filePath(chain_name);
    if (QFile::exists(path))
        return mp::utils::contents_of(path);
    return {};
}
