#pragma once
#include <memory>
#include <string>
#include <vector>
#include "fake_receiver.h"

/*---------------------------------------------------------*\
| Helpers shared by the suites                              |
\*---------------------------------------------------------*/

/* Packets sent to the keyboard since the last Forget(), without the wireless probe */
inline std::vector<FakePacket> Sent()
{
    return g_receiver.sent;
}

inline void Forget()
{
    g_receiver.sent.clear();
}

inline bool IsGet(const FakePacket& p)
{
    return (p[3] & 0x80) != 0;
}

inline unsigned int CountSets()
{
    unsigned int n = 0;

    for(const FakePacket& p : g_receiver.sent)
    {
        if(!IsGet(p))
        {
            n++;
        }
    }

    return n;
}

inline int I(unsigned int v)
{
    return (int)v;
}

/*---------------------------------------------------------*\
| Cest is macro based, so a comma outside parentheses inside |
| a block splits its arguments. Braced lists go through      |
| these instead of being assigned directly.                  |
\*---------------------------------------------------------*/
inline std::vector<unsigned int> Cols(std::vector<unsigned int> v)
{
    return v;
}

inline std::vector<unsigned char> Bytes(std::vector<unsigned char> v)
{
    return v;
}
