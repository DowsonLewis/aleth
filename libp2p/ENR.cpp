// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "ENR.h"
#include <libdevcore/SHA3.h>

namespace dev
{
namespace p2p
{
namespace
{
    template<class Address>
    bytes addressToBytes(Address const& _address)
    {
        auto const addressBytes = _address.to_bytes();
        return bytes(addressBytes.begin(), addressBytes.end());
    }    
}  // namespace

ENR::ENR(RLP _rlp)
{
    m_signature = _rlp[0].toBytes();

    m_seq = _rlp[1].toInt<uint64_t>();
    for (size_t i = 2; i < _rlp.itemCount(); i+= 2)
    {
        auto key = _rlp[i].toString();
        auto value = _rlp[i + 1].toBytes();
        m_map.insert({key, value});
    }
    // TODO check signature
    // TODO fail if > 300 bytes
}

ENR::ENR(uint64_t _seq, std::map<std::string, bytes> const& _keyValues, SignFunction _signFunction)
    : m_seq(_seq),
    m_map(_keyValues),
    m_signature(_signFunction(dev::ref(contentRlpList())))
{
}

std::vector<bytes> ENR::content() const
{
    std::vector<bytes> result{rlp(m_seq)};
    for (auto const keyValue : m_map)
    {
        result.push_back(bytes(keyValue.first.begin(), keyValue.first.end()));
        result.push_back(keyValue.second);
    }
    return result;
}

bytes ENR::contentRlpList() const
{
    RLPStream stream;
    stream.appendVector(content());
    return stream.out();
}

void ENR::streamRLP(RLPStream& _s) const
{
    _s.appendList(m_map.size() * 2 + 2);
    _s << m_signature;
    
    auto const contentItems = content();
    for (auto const& contentItem : contentItems)
        _s << contentItem;
}

ENR createV4ENR(Secret const& _secret, boost::asio::ip::address const& _ip, uint16_t _tcpPort,  uint16_t _udpPort)
{
    ENR::SignFunction signFunction = [&_secret](bytesConstRef _data) { 
        Signature s = dev::sign(_secret, sha3(_data)); 
        // The resulting 64-byte signature is encoded as the concatenation of the r and s signature values.
        return bytes(s.data(), s.data() + 64);
    };

    PublicCompressed publicKey = toPublicCompressed(_secret);
    
    auto const address = _ip.is_v4() ? addressToBytes(_ip.to_v4()) : addressToBytes(_ip.to_v6());
    
    std::map<std::string, bytes> keyValues = {
        { "id", bytes{ 'v', '4' } },
        { "sec256k1", publicKey.asBytes() },
        { "ip", address },
        { "tcp", rlp(_tcpPort) },
        { "udp", rlp(_udpPort) }
    };

    return ENR{0, keyValues, signFunction};
}

std::ostream& operator<<(std::ostream& _out, ENR const& _enr)
{
    RLPStream s;
    _enr.streamRLP(s);

    _out << RLP(s.out());
    return _out;
}

}  // namespace p2p
}  // namespace dev