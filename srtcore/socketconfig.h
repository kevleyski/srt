/*
 * SRT - Secure, Reliable, Transport
 * Copyright (c) 2018 Haivision Systems Inc.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 */

/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Haivision Systems Inc.
*****************************************************************************/

#ifndef INC_SRT_SOCKETCONFIG_H
#define INC_SRT_SOCKETCONFIG_H

#include "platform_sys.h"
#ifdef SRT_ENABLE_BINDTODEVICE
#include <linux/if.h>
#endif
#include <string>
#include "haicrypt.h"
#include "congctl.h"
#include "packet.h"
#include "handshake.h"
#include "logger_defs.h"

// SRT Version constants
#define SRT_VERSION_UNK     0
#define SRT_VERSION_MAJ1    0x010000            /* Version 1 major */
#define SRT_VERSION_MAJ(v) (0xFF0000 & (v))     /* Major number ensuring backward compatibility */
#define SRT_VERSION_MIN(v) (0x00FF00 & (v))
#define SRT_VERSION_PCH(v) (0x0000FF & (v))

// NOTE: SRT_VERSION is primarily defined in the build file.
extern const int32_t SRT_DEF_VERSION;

struct CSrtMuxerConfig
{
    static const int DEF_UDP_BUFFER_SIZE = 65536;

    int  m_iIpTTL;
    int  m_iIpToS;
    int  m_iIpV6Only;  // IPV6_V6ONLY option (-1 if not set)
    bool m_bReuseAddr; // reuse an exiting port or not, for UDP multiplexer

#ifdef SRT_ENABLE_BINDTODEVICE
    std::string m_BindToDevice;
#endif
    int m_iUDPSndBufSize; // UDP sending buffer size
    int m_iUDPRcvBufSize; // UDP receiving buffer size

    // Some shortcuts
#define DEFINEP(psym)                                                                                                  \
    char*       pv##psym() { return (char*)&m_i##psym; }                                                               \
    const char* pv##psym() const { return (char*)&m_i##psym; }

    DEFINEP(UDPSndBufSize);
    DEFINEP(UDPRcvBufSize);
    DEFINEP(IpV6Only);
    DEFINEP(IpTTL);
    DEFINEP(IpToS);
#undef DEFINEP

    bool operator==(const CSrtMuxerConfig& other) const
    {
#define CEQUAL(field) (field == other.field)
        return CEQUAL(m_iIpTTL)
            && CEQUAL(m_iIpToS)
            && CEQUAL(m_iIpV6Only)
            && CEQUAL(m_bReuseAddr)
#ifdef SRT_ENABLE_BINDTODEVICE
            && CEQUAL(m_BindToDevice)
#endif
            && CEQUAL(m_iUDPSndBufSize)
            && CEQUAL(m_iUDPRcvBufSize);
#undef CEQUAL
    }

    CSrtMuxerConfig()
        : m_iIpTTL(-1) /* IPv4 TTL or IPv6 HOPs [1..255] (-1:undefined) */
        , m_iIpToS(-1) /* IPv4 Type of Service or IPv6 Traffic Class [0x00..0xff] (-1:undefined) */
        , m_iIpV6Only(-1)
        , m_bReuseAddr(true) // This is default in SRT
        , m_iUDPSndBufSize(DEF_UDP_BUFFER_SIZE)
        , m_iUDPRcvBufSize(DEF_UDP_BUFFER_SIZE)
    {
    }
};

struct CSrtConfig;

typedef void setter_function(CSrtConfig& co, const void* optval, int optlen);

template<SRT_SOCKOPT name>
struct CSrtConfigSetter
{
    static setter_function set;
};

template <size_t SIZE>
class StringStorage
{
    char     stor[SIZE + 1];
    uint16_t len;

    // NOTE: default copying allowed.

public:
    StringStorage()
        : len(0)
    {
        memset(stor, 0, sizeof stor);
    }

    bool set(const char* s, size_t length)
    {
        if (length >= SIZE)
            return false;

        memcpy(stor, s, length);
        stor[length] = 0;
        len          = length;
        return true;
    }

    bool set(const std::string& s)
    {
        if (s.size() >= SIZE)
            return false;

        s.copy(stor, SIZE);
        stor[SIZE] = 0;
        len        = s.size();
        return true;
    }

    std::string str() const
    {
        return len == 0 ? std::string() : std::string(stor);
    }

    const char* c_str() const
    {
        return stor;
    }

    size_t size() const { return size_t(len); }
    bool   empty() const { return len == 0; }
};

struct CSrtConfig: CSrtMuxerConfig
{
    typedef srt::sync::steady_clock::time_point time_point;
    typedef srt::sync::steady_clock::duration   duration;

    static const int
        DEF_MSS = 1500,
        DEF_FLIGHT_SIZE = 25600,
        DEF_BUFFER_SIZE = 8192, //Rcv buffer MUST NOT be bigger than Flight Flag size
        DEF_LINGER_S = 3*60,    // 3 minutes
        DEF_CONNTIMEO_S = 3;    // 3 seconds

    static const int      COMM_RESPONSE_TIMEOUT_MS      = 5 * 1000; // 5 seconds
    static const uint32_t COMM_DEF_STABILITY_TIMEOUT_US = 80 * 1000;

    // Mimimum recv flight flag size is 32 packets
    static const int    DEF_MAX_FLIGHT_PKT = 32;
    static const size_t MAX_SID_LENGTH     = 512;
    static const size_t MAX_PFILTER_LENGTH = 64;
    static const size_t MAX_CONG_LENGTH    = 16;

    int    m_iMSS;            // Maximum Segment Size, in bytes
    size_t m_zExpPayloadSize; // Expected average payload size (user option)

    // Options
    bool   m_bSynSending;     // Sending syncronization mode
    bool   m_bSynRecving;     // Receiving syncronization mode
    int    m_iFlightFlagSize; // Maximum number of packets in flight from the peer side
    int    m_iSndBufSize;     // Maximum UDT sender buffer size
    int    m_iRcvBufSize;     // Maximum UDT receiver buffer size
    linger m_Linger;          // Linger information on close
    bool   m_bRendezvous;     // Rendezvous connection mode

    duration m_tdConnTimeOut; // connect timeout in milliseconds
    bool     m_bDriftTracer;
    int      m_iSndTimeOut; // sending timeout in milliseconds
    int      m_iRcvTimeOut; // receiving timeout in milliseconds
    int64_t  m_llMaxBW;     // maximum data transfer rate (threshold)

    // These fields keep the options for encryption
    // (SRTO_PASSPHRASE, SRTO_PBKEYLEN). Crypto object is
    // created later and takes values from these.
    HaiCrypt_Secret m_CryptoSecret;
    int             m_iSndCryptoKeyLen;

    // XXX Consider removing. The m_bDataSender stays here
    // in order to maintain the HS side selection in HSv4.
    bool m_bDataSender;

    bool     m_bMessageAPI;
    bool     m_bTSBPD;        // Whether AGENT will do TSBPD Rx (whether peer does, is not agent's problem)
    int      m_iRcvLatency;   // Agent's Rx latency
    int      m_iPeerLatency;  // Peer's Rx latency for the traffic made by Agent's Tx.
    bool     m_bTLPktDrop;    // Whether Agent WILL DO TLPKTDROP on Rx.
    int      m_iSndDropDelay; // Extra delay when deciding to snd-drop for TLPKTDROP, -1 to off
    bool     m_bEnforcedEnc;  // Off by default. When on, any connection other than nopw-nopw & pw1-pw1 is rejected.
    int      m_GroupConnect;
    int      m_iPeerIdleTimeout; // Timeout for hearing anything from the peer.
    uint32_t m_uStabilityTimeout;
    int      m_iRetransmitAlgo;

    int64_t m_llInputBW; // Input stream rate (bytes/sec)
    // 0: use internally estimated input bandwidth
    int  m_iOverheadBW;          // Percent above input stream rate (applies if m_llMaxBW == 0)
    bool m_bRcvNakReport;        // Enable Receiver Periodic NAK Reports
    int  m_iMaxReorderTolerance; //< Maximum allowed value for dynamic reorder tolerance

    // For the use of CCryptoControl
    // HaiCrypt configuration
    unsigned int m_uKmRefreshRatePkt;
    unsigned int m_uKmPreAnnouncePkt;

    uint32_t m_lSrtVersion;
    uint32_t m_lMinimumPeerSrtVersion;

    StringStorage<MAX_CONG_LENGTH>    m_Congestion;
    StringStorage<MAX_PFILTER_LENGTH> m_PacketFilterConfig;
    StringStorage<MAX_SID_LENGTH>     m_StreamName;

    // Shortcuts and utilities
    int32_t flightCapacity()
    {
        return std::min(m_iRcvBufSize, m_iFlightFlagSize);
    }

    CSrtConfig()
        : m_iMSS(DEF_MSS)
        , m_zExpPayloadSize(SRT_LIVE_DEF_PLSIZE)
        , m_bSynSending(true)
        , m_bSynRecving(true)
        , m_iFlightFlagSize(DEF_FLIGHT_SIZE)
        , m_iSndBufSize(DEF_BUFFER_SIZE)
        , m_iRcvBufSize(DEF_BUFFER_SIZE)
        , m_bRendezvous(false)
        , m_tdConnTimeOut(srt::sync::seconds_from(DEF_CONNTIMEO_S))
        , m_bDriftTracer(true)
        , m_iSndTimeOut(-1)
        , m_iRcvTimeOut(-1)
        , m_llMaxBW(-1)
        , m_bDataSender(false)
        , m_bMessageAPI(true)
        , m_bTSBPD(true)
        , m_iRcvLatency(SRT_LIVE_DEF_LATENCY_MS)
        , m_iPeerLatency(0)
        , m_bTLPktDrop(true)
        , m_iSndDropDelay(0)
        , m_bEnforcedEnc(true)
        , m_GroupConnect(0)
        , m_iPeerIdleTimeout(COMM_RESPONSE_TIMEOUT_MS)
        , m_uStabilityTimeout(COMM_DEF_STABILITY_TIMEOUT_US)
        , m_iRetransmitAlgo(0)
        , m_llInputBW(0)
        , m_iOverheadBW(25)
        , m_bRcvNakReport(true)
        , m_iMaxReorderTolerance(0) // Sensible optimal value is 10, 0 preserves old behavior
        , m_uKmRefreshRatePkt(0)
        , m_uKmPreAnnouncePkt(0)
        , m_lSrtVersion(SRT_DEF_VERSION)
        , m_lMinimumPeerSrtVersion(SRT_VERSION_MAJ1)

    {
        // Default UDT configurations
        m_iUDPRcvBufSize = m_iRcvBufSize * m_iMSS;

        // Linger: LIVE mode defaults, please refer to `SRTO_TRANSTYPE` option
        // for other modes.
        m_Linger.l_onoff   = 0;
        m_Linger.l_linger  = 0;
        m_CryptoSecret.len = 0;
        m_iSndCryptoKeyLen = 0;

        // Default congestion is "live".
        // Available builtin congestions: "file".
        // Others can be registerred.
        m_Congestion.set("live", 4);
    }

    ~CSrtConfig()
    {
        // Wipeout critical data
        memset(&m_CryptoSecret, 0, sizeof(m_CryptoSecret));
    }

    int set(SRT_SOCKOPT optName, const void* val, int size);

    // Could be later made a more robust version with
    // dispatching to the right data type.
    template <SRT_SOCKOPT optName>
    void set(const void* val, int size)
    {
        CSrtConfigSetter<optName>::set(*this, val, size);
    }
};


#if ENABLE_EXPERIMENTAL_BONDING

struct SRT_SocketOptionObject
{
    struct SingleOption
    {
        uint16_t      option;
        uint16_t      length;
        unsigned char storage[1]; // NOTE: Variable length object!
    };


    std::vector<SingleOption*> options;

    SRT_SocketOptionObject() {}

    ~SRT_SocketOptionObject()
    {
        for (size_t i = 0; i < options.size(); ++i)
        {
            // Convert back
            unsigned char* mem = reinterpret_cast<unsigned char*>(options[i]);
            delete[] mem;
        }
    }

    bool add(SRT_SOCKOPT optname, const void* optval, size_t optlen);
};
#endif

template <typename T>
inline T cast_optval(const void* optval)
{
    return *reinterpret_cast<const T*>(optval);
}

template <typename T>
inline T cast_optval(const void* optval, int optlen)
{
    if (optlen > 0 && optlen != sizeof(T))
        throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

    return cast_optval<T>(optval);
}

// This function is to make it possible for both C and C++
// API to accept both bool and int types for boolean options.
// (it's not that C couldn't use <stdbool.h>, it's that people
// often forget to use correct type).
template <>
inline bool cast_optval(const void* optval, int optlen)
{
    if (optlen == sizeof(bool))
    {
        return *reinterpret_cast<const bool*>(optval);
    }

    if (optlen == sizeof(int))
    {
        // 0!= is a windows warning-killer int-to-bool conversion
        return 0 != *reinterpret_cast<const int*>(optval);
    }
    return false;
}

template<>
struct CSrtConfigSetter<SRTO_MSS>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        int ival = cast_optval<int>(optval, optlen);
        if (ival < int(CPacket::UDP_HDR_SIZE + CHandShake::m_iContentSize))
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

        co.m_iMSS = ival;

        // Packet size cannot be greater than UDP buffer size
        if (co.m_iMSS > co.m_iUDPSndBufSize)
            co.m_iMSS = co.m_iUDPSndBufSize;
        if (co.m_iMSS > co.m_iUDPRcvBufSize)
            co.m_iMSS = co.m_iUDPRcvBufSize;
    }
};

template<>
struct CSrtConfigSetter<SRTO_FC>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        int fc = cast_optval<int>(optval, optlen);
        if (fc < 1)
            throw CUDTException(MJ_NOTSUP, MN_INVAL);

        co.m_iFlightFlagSize = std::min(fc, +co.DEF_MAX_FLIGHT_PKT);
    }
};

template<>
struct CSrtConfigSetter<SRTO_SNDBUF>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        int bs = cast_optval<int>(optval, optlen);
        if (bs <= 0)
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

        co.m_iSndBufSize = bs / (co.m_iMSS - CPacket::UDP_HDR_SIZE);
    }
};

template<>
struct CSrtConfigSetter<SRTO_RCVBUF>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        const int val = cast_optval<int>(optval, optlen);
        if (val <= 0)
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

        // Mimimum recv buffer size is 32 packets
        const int mssin_size = co.m_iMSS - CPacket::UDP_HDR_SIZE;

        // XXX This magic 32 deserves some constant
        if (val > mssin_size * co.DEF_MAX_FLIGHT_PKT)
            co.m_iRcvBufSize = val / mssin_size;
        else
            co.m_iRcvBufSize = co.DEF_MAX_FLIGHT_PKT;

        // recv buffer MUST not be greater than FC size
        if (co.m_iRcvBufSize > co.m_iFlightFlagSize)
            co.m_iRcvBufSize = co.m_iFlightFlagSize;
    }
};

template<>
struct CSrtConfigSetter<SRTO_LINGER>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_Linger = cast_optval<linger>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_UDP_SNDBUF>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iUDPSndBufSize = std::max(co.m_iMSS, cast_optval<int>(optval, optlen));
    }
};

template<>
struct CSrtConfigSetter<SRTO_UDP_RCVBUF>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iUDPRcvBufSize = std::max(co.m_iMSS, cast_optval<int>(optval, optlen));
    }
};
template<>
struct CSrtConfigSetter<SRTO_RENDEZVOUS>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bRendezvous = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_SNDTIMEO>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iSndTimeOut = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_RCVTIMEO>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iRcvTimeOut = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_SNDSYN>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bSynSending = cast_optval<bool>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_RCVSYN>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bSynRecving = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_REUSEADDR>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bReuseAddr = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_MAXBW>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_llMaxBW = cast_optval<int64_t>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_IPTTL>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        int val = cast_optval<int>(optval, optlen);
        if (!(val == -1) && !((val >= 1) && (val <= 255)))
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        co.m_iIpTTL = cast_optval<int>(optval);
    }
};
template<>
struct CSrtConfigSetter<SRTO_IPTOS>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iIpToS = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_BINDTODEVICE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;
#ifdef SRT_ENABLE_BINDTODEVICE
        using namespace std;
        using namespace srt_logging;

        string val;
        if (optlen == -1)
            val = (const char *)optval;
        else
            val.assign((const char *)optval, optlen);
        if (val.size() >= IFNAMSIZ)
        {
            LOGC(kmlog.Error, log << "SRTO_BINDTODEVICE: device name too long (max: IFNAMSIZ=" << IFNAMSIZ << ")");
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }

        co.m_BindToDevice = val;
#else
        (void)co; // prevent warning
        (void)optval;
        (void)optlen;
        LOGC(kmlog.Error, log << "SRTO_BINDTODEVICE is not supported on that platform");
        throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
#endif
    }
};

template<>
struct CSrtConfigSetter<SRTO_INPUTBW>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_llInputBW = cast_optval<int64_t>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_OHEADBW>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        int32_t val = cast_optval<int32_t>(optval, optlen);
        if (val < 5 || val > 100)
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        co.m_iOverheadBW = val;
    }
};
template<>
struct CSrtConfigSetter<SRTO_SENDER>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bDataSender = cast_optval<bool>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_TSBPDMODE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bTSBPD = cast_optval<bool>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_LATENCY>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iRcvLatency     = cast_optval<int>(optval, optlen);
        co.m_iPeerLatency = cast_optval<int>(optval);
    }
};
template<>
struct CSrtConfigSetter<SRTO_RCVLATENCY>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iRcvLatency = cast_optval<int>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_PEERLATENCY>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iPeerLatency = cast_optval<int>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_TLPKTDROP>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bTLPktDrop = cast_optval<bool>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_SNDDROPDELAY>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        // Surprise: you may be connected to alter this option.
        // The application may manipulate this option on sender while transmitting.
        co.m_iSndDropDelay = cast_optval<int>(optval, optlen);
    }
};
template<>
struct CSrtConfigSetter<SRTO_PASSPHRASE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;
#ifdef SRT_ENABLE_ENCRYPTION
        // Password must be 10-80 characters.
        // Or it can be empty to clear the password.
        if ((optlen != 0) && (optlen < 10 || optlen > HAICRYPT_SECRET_MAX_SZ))
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

        memset(&co.m_CryptoSecret, 0, sizeof(co.m_CryptoSecret));
        co.m_CryptoSecret.typ = HAICRYPT_SECTYP_PASSPHRASE;
        co.m_CryptoSecret.len = (optlen <= (int)sizeof(co.m_CryptoSecret.str) ? optlen : (int)sizeof(co.m_CryptoSecret.str));
        memcpy((co.m_CryptoSecret.str), optval, co.m_CryptoSecret.len);
#else
        (void)co; // prevent warning
        (void)optval;
        if (optlen == 0)
            return; // Allow to set empty passphrase if no encryption supported.

        LOGC(aclog.Error, log << "SRTO_PASSPHRASE: encryption not enabled at compile time");
        throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
#endif
    }
};
template<>
struct CSrtConfigSetter<SRTO_PBKEYLEN>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;
#ifdef SRT_ENABLE_ENCRYPTION
        const int v    = cast_optval<int>(optval, optlen);
        int const allowed[4] = {
            0,  // Default value, if this results for initiator, defaults to 16. See below.
            16, // AES-128
            24, // AES-192
            32  // AES-256
        };
        const int *const allowed_end = allowed + 4;
        if (std::find(allowed, allowed_end, v) == allowed_end)
        {
            LOGC(aclog.Error,
                    log << "Invalid value for option SRTO_PBKEYLEN: " << v << "; allowed are: 0, 16, 24, 32");
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }

        // Note: This works a little different in HSv4 and HSv5.

        // HSv4:
        // The party that is set SRTO_SENDER will send KMREQ, and it will
        // use default value 16, if SRTO_PBKEYLEN is the default value 0.
        // The responder that receives KMRSP has nothing to say about
        // PBKEYLEN anyway and it will take the length of the key from
        // the initiator (sender) as a good deal.
        //
        // HSv5:
        // The initiator (independently on the sender) will send KMREQ,
        // and as it should be the sender to decide about the PBKEYLEN.
        // Your application should do the following then:
        // 1. The sender should set PBKEYLEN to the required value.
        // 2. If the sender is initiator, it will create the key using
        //    its preset PBKEYLEN (or default 16, if not set) and the
        //    receiver-responder will take it as a good deal.
        // 3. Leave the PBKEYLEN value on the receiver as default 0.
        // 4. If sender is responder, it should then advertise the PBKEYLEN
        //    value in the initial handshake messages (URQ_INDUCTION if
        //    listener, and both URQ_WAVEAHAND and URQ_CONCLUSION in case
        //    of rendezvous, as it is the matter of luck who of them will
        //    eventually become the initiator). This way the receiver
        //    being an initiator will set m_iSndCryptoKeyLen before setting
        //    up KMREQ for sending to the sender-responder.
        //
        // Note that in HSv5 if both sides set PBKEYLEN, the responder
        // wins, unless the initiator is a sender (the effective PBKEYLEN
        // will be the one advertised by the responder). If none sets,
        // PBKEYLEN will default to 16.

        co.m_iSndCryptoKeyLen = v;
#else
        (void)co; // prevent warning
        (void)optval;
        (void)optlen;
        LOGC(aclog.Error, log << "SRTO_PBKEYLEN: encryption not enabled at compile time");
        throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
#endif
    }
};

template<>
struct CSrtConfigSetter<SRTO_NAKREPORT>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bRcvNakReport = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_CONNTIMEO>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt::sync;
        co.m_tdConnTimeOut = milliseconds_from(cast_optval<int>(optval, optlen));
    }
};

template<>
struct CSrtConfigSetter<SRTO_DRIFTTRACER>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bDriftTracer = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_LOSSMAXTTL>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iMaxReorderTolerance = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_VERSION>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_lSrtVersion = cast_optval<uint32_t>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_MINVERSION>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_lMinimumPeerSrtVersion = cast_optval<uint32_t>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_STREAMID>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        if (size_t(optlen) > CSrtConfig::MAX_SID_LENGTH)
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

        co.m_StreamName.set((const char*)optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_CONGESTION>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        std::string val;
        if (optlen == -1)
            val = (const char*)optval;
        else
            val.assign((const char*)optval, optlen);

        // Translate alias
        if (val == "vod")
            val = "file";

        bool res = SrtCongestion::exists(val);
        if (!res)
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);

        co.m_Congestion.set(val);
    }
};

template<>
struct CSrtConfigSetter<SRTO_MESSAGEAPI>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bMessageAPI = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_PAYLOADSIZE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;

        if (*(int *)optval > SRT_LIVE_MAX_PLSIZE)
        {
            LOGC(aclog.Error, log << "SRTO_PAYLOADSIZE: value exceeds SRT_LIVE_MAX_PLSIZE, maximum payload per MTU.");
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }

        if (!co.m_PacketFilterConfig.empty())
        {
            // This means that the filter might have been installed before,
            // and the fix to the maximum payload size was already applied.
            // This needs to be checked now.
            SrtFilterConfig fc;
            if (!ParseFilterConfig(co.m_PacketFilterConfig.str(), fc))
            {
                // Break silently. This should not happen
                LOGC(aclog.Error, log << "SRTO_PAYLOADSIZE: IPE: failing filter configuration installed");
                throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
            }

            size_t efc_max_payload_size = SRT_LIVE_MAX_PLSIZE - fc.extra_size;
            if (co.m_zExpPayloadSize > efc_max_payload_size)
            {
                LOGC(aclog.Error,
                     log << "SRTO_PAYLOADSIZE: value exceeds SRT_LIVE_MAX_PLSIZE decreased by " << fc.extra_size
                         << " required for packet filter header");
                throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
            }
        }

        co.m_zExpPayloadSize = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_TRANSTYPE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        // XXX Note that here the configuration for SRTT_LIVE
        // is the same as DEFAULT VALUES for these fields set
        // in CUDT::CUDT.
        switch (cast_optval<SRT_TRANSTYPE>(optval, optlen))
        {
        case SRTT_LIVE:
            // Default live options:
            // - tsbpd: on
            // - latency: 120ms
            // - linger: off
            // - congctl: live
            // - extraction method: message (reading call extracts one message)
            co.m_bTSBPD          = true;
            co.m_iRcvLatency     = SRT_LIVE_DEF_LATENCY_MS;
            co.m_iPeerLatency    = 0;
            co.m_bTLPktDrop      = true;
            co.m_iSndDropDelay   = 0;
            co.m_bMessageAPI     = true;
            co.m_bRcvNakReport   = true;
            co.m_zExpPayloadSize = SRT_LIVE_DEF_PLSIZE;
            co.m_Linger.l_onoff  = 0;
            co.m_Linger.l_linger = 0;
            co.m_Congestion.set("live", 4);
            break;

        case SRTT_FILE:
            // File transfer mode:
            // - tsbpd: off
            // - latency: 0
            // - linger: 2 minutes (180s)
            // - congctl: file (original UDT congestion control)
            // - extraction method: stream (reading call extracts as many bytes as available and fits in buffer)
            co.m_bTSBPD          = false;
            co.m_iRcvLatency     = 0;
            co.m_iPeerLatency    = 0;
            co.m_bTLPktDrop      = false;
            co.m_iSndDropDelay   = -1;
            co.m_bMessageAPI     = false;
            co.m_bRcvNakReport   = false;
            co.m_zExpPayloadSize = 0; // use maximum
            co.m_Linger.l_onoff  = 1;
            co.m_Linger.l_linger = CSrtConfig::DEF_LINGER_S;
            co.m_Congestion.set("file", 4);
            break;

        default:
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }
    }
};

#if ENABLE_EXPERIMENTAL_BONDING
template<>
struct CSrtConfigSetter<SRTO_GROUPCONNECT>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_GroupConnect = cast_optval<int>(optval, optlen);
    }
};
#endif

template<>
struct CSrtConfigSetter<SRTO_KMREFRESHRATE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;

        // If you first change the KMREFRESHRATE, KMPREANNOUNCE
        // will be set to the maximum allowed value
        co.m_uKmRefreshRatePkt = cast_optval<int>(optval, optlen);
        if (co.m_uKmPreAnnouncePkt == 0 || co.m_uKmPreAnnouncePkt > (co.m_uKmRefreshRatePkt - 1) / 2)
        {
            co.m_uKmPreAnnouncePkt = (co.m_uKmRefreshRatePkt - 1) / 2;
            LOGC(aclog.Warn,
                 log << "SRTO_KMREFRESHRATE=0x" << std::hex << co.m_uKmRefreshRatePkt << ": setting SRTO_KMPREANNOUNCE=0x"
                     << std::hex << co.m_uKmPreAnnouncePkt);
        }
    }
};

template<>
struct CSrtConfigSetter<SRTO_KMPREANNOUNCE>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;

        const int val = cast_optval<int>(optval, optlen);
        const int kmref = co.m_uKmRefreshRatePkt == 0 ? HAICRYPT_DEF_KM_REFRESH_RATE : co.m_uKmRefreshRatePkt;
        if (val > (kmref - 1) / 2)
        {
            LOGC(aclog.Error,
                    log << "SRTO_KMPREANNOUNCE=0x" << std::hex << val << " exceeds KmRefresh/2, 0x" << ((kmref - 1) / 2)
                    << " - OPTION REJECTED.");
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }

        co.m_uKmPreAnnouncePkt = val;
    }
};

template<>
struct CSrtConfigSetter<SRTO_ENFORCEDENCRYPTION>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_bEnforcedEnc = cast_optval<bool>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_PEERIDLETIMEO>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iPeerIdleTimeout = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_IPV6ONLY>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iIpV6Only = cast_optval<int>(optval, optlen);
    }
};

template<>
struct CSrtConfigSetter<SRTO_PACKETFILTER>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;
        std::string arg((const char*)optval, optlen);
        // Parse the configuration string prematurely
        SrtFilterConfig fc;
        if (!ParseFilterConfig(arg, fc))
        {
            LOGC(aclog.Error,
                    log << "SRTO_FILTER: Incorrect syntax. Use: FILTERTYPE[,KEY:VALUE...]. "
                    "FILTERTYPE ("
                    << fc.type << ") must be installed (or builtin)");
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }

        size_t efc_max_payload_size = SRT_LIVE_MAX_PLSIZE - fc.extra_size;
        if (co.m_zExpPayloadSize > efc_max_payload_size)
        {
            LOGC(aclog.Warn,
                    log << "Due to filter-required extra " << fc.extra_size << " bytes, SRTO_PAYLOADSIZE fixed to "
                    << efc_max_payload_size << " bytes");
            co.m_zExpPayloadSize = efc_max_payload_size;
        }

        co.m_PacketFilterConfig.set(arg);
    }
};

#if ENABLE_EXPERIMENTAL_BONDING
template<>
struct CSrtConfigSetter<SRTO_GROUPSTABTIMEO>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        using namespace srt_logging;
        // This option is meaningless for the socket itself.
        // It's set here just for the sake of setting it on a listener
        // socket so that it is then applied on the group when a
        // group connection is configuired.
        const int val = cast_optval<int>(optval, optlen);

        // Search if you already have SRTO_PEERIDLETIMEO set

        const int idletmo = co.m_iPeerIdleTimeout;

        // Both are in milliseconds.
        // This option is RECORDED in microseconds, while
        // idletmo is recorded in milliseconds, only translated to
        // microseconds directly before use.
        if (val >= idletmo)
        {
            LOGC(aclog.Error, log << "group option: SRTO_GROUPSTABTIMEO(" << val
                    << ") exceeds SRTO_PEERIDLETIMEO(" << idletmo << ")");
            throw CUDTException(MJ_NOTSUP, MN_INVAL, 0);
        }

        co.m_uStabilityTimeout = val * 1000;
    }
};
#endif

template<>
struct CSrtConfigSetter<SRTO_RETRANSMITALGO>
{
    static void set(CSrtConfig& co, const void* optval, int optlen)
    {
        co.m_iRetransmitAlgo = cast_optval<int32_t>(optval, optlen);
    }
};

#if TEMPLATE
#endif

#endif
