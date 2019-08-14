﻿/* Copyright (c) 2019 James Jackson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "UBLOX/parsers/nav.h"
#include "UBLOX/bit_tools.h"

#define DBG(...) fprintf(stderr, __VA_ARGS__)

void NavParser::registerCallback(eph_cb cb)
{
    eph_callbacks.push_back(cb);
}

void NavParser::registerCallback(geph_cb cb)
{
    geph_callbacks.push_back(cb);
}

NavParser::NavParser()
{
    subscribe(ublox::CLASS_RXM, ublox::RXM_SFRBX);
}

bool NavParser::convertUBX(const ublox::RXM_SFRBX_t &msg)
{
    using namespace ublox;
    if (msg.gnssId == GnssID_Glonass)
    {
        GlonassEphemeris *geph = nullptr;
        for (int i = 0; i < geph_.size(); i++)
        {
            if (geph_[i].sat == msg.svId)
                geph = &geph_[i];
        }
        if (!geph)
        {
            geph_.emplace_back();
            geph_.back().sat = msg.svId;
            geph = &geph_.back();
        }
        // DBG("glonass\n");
        return decodeGlonass(msg, *geph);
    }
    else
    {
        Ephemeris *eph = nullptr;
        for (int i = 0; i < eph_.size(); i++)
        {
            if (eph_[i].sat == msg.svId)
                eph = &eph_[i];
        }
        if (!eph)
        {
            eph_.emplace_back();
            eph = &eph_.back();
        }

        eph->sat = msg.svId;

        switch (msg.gnssId)
        {
        case GnssID_GPS:
            // DBG("GPS\n");
            return decodeGPS((uint8_t *)msg.dwrd, eph);
        case GnssID_Galileo:
            // DBG("GAL\n");
            return decodeGalileo((uint8_t *)msg.dwrd, eph);
        case GnssID_Beidou:
            // DBG("BEIDOU\n");
            return decodeBeidou((uint8_t *)msg.dwrd, eph);
        }
    }
}

bool NavParser::decodeGPSSubframe1(const uint8_t *const buf, Ephemeris *eph)
{
    // double tow = getBit<17>(buf, 24) * 6.0;
    int week = getBit<10>(buf, 48);
    eph->code_on_L2 = getBit<2>(buf, 58);
    eph->ura = getBit<4>(buf, 60);
    eph->health = getBit<6>(buf, 64);
    uint8_t iodc0 = getBit<2>(buf, 70);
    eph->L2_P_data_flag = getBit<1>(buf, 72);
    int tgd = getBit<8, Signed>(buf, 160);
    eph->iode = getBit<8>(buf, 168);
    eph->tocs = getBit<16>(buf, 176) * 16.0;
    eph->af2 = getBit<8, Signed>(buf, 192) * P2_55;
    eph->af1 = getBit<16, Signed>(buf, 200) * P2_43;
    eph->af0 = getBit<22, Signed>(buf, 216) * P2_31;

    eph->alert_flag = 0;  // TODO: populate

    eph->tgd[0] = tgd == -128 ? 0.0 : tgd * P2_31;
    eph->tgd[1] = eph->tgd[2] = eph->tgd[3] = 0;
    eph->iodc = (iodc0 << 8) + eph->iode;
    eph->week = week + UTCTime::GPS_WEEK_ROLLOVER;

    eph->iode1 = eph->iode;
    eph->got_subframe1 = true;

    return true;
}

bool NavParser::decodeGPSSubframe2(const uint8_t *buf, Ephemeris *eph)
{
    eph->iode = getBit<8>(buf, 48);
    eph->crs = getBit<16, Signed>(buf, 56) * P2_5;
    eph->delta_n = getBit<16, Signed>(buf, 72) * P2_43 * PI;
    eph->m0 = getBit<32, Signed>(buf, 88) * P2_31 * PI;
    eph->cuc = getBit<16, Signed>(buf, 120) * P2_29;
    eph->ecc = getBit<32>(buf, 136) * P2_33;
    eph->cus = getBit<16, Signed>(buf, 168) * P2_29;
    eph->sqrta = getBit<32>(buf, 184) * P2_19;
    eph->toes = getBit<16>(buf, 216) * 16.0;
    eph->fit_interval_flag = getBit<1>(buf, 232) ? 0.0 : 4.0; /* 0:4hr,1:>4hr */

    eph->iode2 = eph->iode;
    eph->got_subframe2 = true;

    return true;
}

bool NavParser::decodeGPSSubframe3(const uint8_t *buf, Ephemeris *eph)
{
    eph->cic = getBit<16, Signed>(buf, 48) * P2_29;
    eph->omega0 = getBit<32, Signed>(buf, 64) * P2_31 * PI;
    eph->cis = getBit<16, Signed>(buf, 96) * P2_29;
    eph->i0 = getBit<32, Signed>(buf, 112) * P2_31 * PI;
    eph->crc = getBit<16, Signed>(buf, 144) * P2_5;
    eph->w = getBit<32, Signed>(buf, 160) * P2_31 * PI;
    eph->omegadot = getBit<24, Signed>(buf, 192) * P2_43 * PI;
    eph->iode3 = getBit<8>(buf, 216);
    eph->idot = getBit<14, Signed>(buf, 224) * P2_43 * PI;

    eph->iode3 = eph->iode;
    eph->got_subframe3 = true;

    return true;
}

bool NavParser::decodeGPS(const uint8_t *buf, Ephemeris *eph)
{
    const uint8_t *p = buf;

    eph->gnssID = 0;

    uint8_t subfrm[30];
    for (int i = 0; i < 10; i++)
    {
        // Strip parity
        setBit<24>(subfrm, i * 24, *(reinterpret_cast<const uint32_t *>(p)) >> 6);
        p += 4;
    }

    // int id = getBit<3>(subfrm, 43);
    int id = (subfrm[5] >> 2) & 7;

    if (id < 1 || 5 < id)
    {
        return -1;
    }

    switch (id)
    {
    case 1:
        decodeGPSSubframe1(subfrm, eph);
        break;
    case 2:
        decodeGPSSubframe2(subfrm, eph);
        break;
    case 3:
        decodeGPSSubframe3(subfrm, eph);
        break;
    default:
        return false;
    }

    // Check that the iodes are the same across the subframes
    if (eph->got_subframe1 && eph->got_subframe2 && eph->got_subframe3 &&
        eph->iode1 == eph->iode2 && eph->iode1 == eph->iode3 && eph->iode == (eph->iodc & 0xFF))
    {
        // Set toe and toc
        eph->toe = UTCTime::fromGPS(eph->week, eph->toes * 1000);
        eph->toc = UTCTime::fromGPS(eph->week, eph->tocs * 1000);
        GPS_time_ = eph->toe;

        for (auto &cb : eph_callbacks)
        {
            cb(*eph);
        }

        eph->got_subframe1 = false;
        eph->got_subframe2 = false;
        eph->got_subframe3 = false;
    }

    return 0;
}

int gloTest(const uint8_t *buf)
{
    static const uint8_t xor8[256] = {
        // 8 bit xor (see ref)
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1,
        0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0,
        0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0,
        1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1,
        1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
    static const uint8_t hamming_mask[][12] = {
        // Hamming Code Mask
        {0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x6A, 0xD8, 0x08},
        {0x66, 0x66, 0x6C, 0xCC, 0xCC, 0xCC, 0xD9, 0x99, 0xB3, 0x68, 0x10},
        {0x87, 0x87, 0x8F, 0x0F, 0x0F, 0x0F, 0x1E, 0x1E, 0x3C, 0x70, 0x20},
        {0x07, 0xF8, 0x0F, 0xF0, 0x0F, 0xF0, 0x1F, 0xE0, 0x3F, 0x80, 0x40},
        {0xF8, 0x00, 0x0F, 0xFF, 0xF0, 0x00, 0x1F, 0xFF, 0xC0, 0x00, 0x80},
        {0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00, 0x01, 0x00},
        {0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8}};

    uint8_t crc;
    int n = 0;
    for (int i = 0; i < 8; ++i)
    {
        crc = 0;
        for (int j = 0; j < 11; ++j)
        {
            crc ^= xor8[buf[j] & hamming_mask[i][j]];
        }
        if (crc)
            ++n;
    }
    return n == 0 || (n == 2 && crc);
}

bool NavParser::decodeGlonassFrame1(const uint8_t *buf, GlonassEphemeris *geph)
{
    // int P1 = getBit<2>(buf, 7);
    int tk_h = getBit<5>(buf, 9);
    int tk_m = getBit<6>(buf, 14);
    int tk_s = getBit<1>(buf, 20) * 30;
    geph->vel[0] = getBitGlo<24>(buf, 21) * P2_20 * 1E3;
    geph->acc[0] = getBitGlo<5>(buf, 45) * P2_30 * 1E3;
    geph->pos[0] = getBitGlo<27>(buf, 50) * P2_11 * 1E3;

    // Convert Time into UTC, using the last GPS time stamp to figure
    // out what day it is.
    int frame_tod_ms = (tk_h * 3600 + tk_m * 60 + tk_s) * 1000;
    int dbg = frame_tod_ms / UTCTime::SEC_IN_DAY;
    geph->tof = UTCTime::fromGlonassTimeOfDay(GPS_time_, frame_tod_ms);
    geph->got_frame1 = true;
}

bool NavParser::decodeGlonassFrame2(const uint8_t *buf, GlonassEphemeris *geph)
{
    geph->svh = getBit<3>(buf, 5);
    // int P2 = getBit<1>(buf, 8);
    int tb = getBit<7>(buf, 9);
    geph->iode = tb;
    geph->vel[1] = getBitGlo<24>(buf, 21) * P2_20 * 1E3;
    geph->acc[1] = getBitGlo<5>(buf, 45) * P2_30 * 1E3;
    geph->pos[1] = getBitGlo<27>(buf, 50) * P2_11 * 1E3;

    // Convert Time into UTC, using the last GPS time stamp to figure
    // out what day it is.
    uint64_t eph_tod_ms = (tb * 60 * 15) * 1000;
    geph->toe = UTCTime::fromGlonassTimeOfDay(GPS_time_, eph_tod_ms);
    geph->got_frame2 = true;
}

bool NavParser::decodeGlonassFrame3(const uint8_t *buf, GlonassEphemeris *geph)
{
    // int P3 = getBit<1>(buf, 5);
    geph->gamn = getBitGlo<11>(buf, 6) * P2_40;
    // int P = getBit<2>(buf, 18);
    // int ln = getBit<1>(buf, 20);
    geph->vel[2] = getBitGlo<24>(buf, 21) * P2_20 * 1E3;
    geph->acc[2] = getBitGlo<5>(buf, 45) * P2_30 * 1E3;
    geph->pos[2] = getBitGlo<27>(buf, 50) * P2_11 * 1E3;
    geph->got_frame3 = true;
}
bool NavParser::decodeGlonassFrame4(const uint8_t *buf, GlonassEphemeris *geph)
{
    geph->taun = getBitGlo<22>(buf, 5) * P2_30;
    geph->dtaun = getBitGlo<5>(buf, 27) * P2_30;
    geph->age = getBit<5>(buf, 32);
    // int P4 = getBit<1>(buf, 51);
    geph->sva = getBit<4>(buf, 52);
    // int NT = getBit<11>(buf, 59);
    // int slot = getBit<5>(buf, 70);
    // int M = getBit<2>(buf, 75);
    geph->got_frame4 = true;
}

bool NavParser::decodeGlonassString(const uint8_t *buf, GlonassEphemeris *geph)
{
    int frame_num = getBit<4>(buf, 1);

    // We only care about strings [1-4]
    if (frame_num < 1 || 4 < frame_num)
    {
        return false;
    }

    switch (frame_num)
    {
    case 1:
        DBG("GLO Sat %d, frm %d", geph->sat, frame_num);
        decodeGlonassFrame1(buf, geph);
        break;
    case 2:
        DBG("GLO Sat %d, frm %d", geph->sat, frame_num);
        decodeGlonassFrame2(buf, geph);
        break;
    case 3:
        DBG("GLO Sat %d, frm %d", geph->sat, frame_num);
        decodeGlonassFrame3(buf, geph);
        break;
    case 4:
        DBG("GLO Sat %d, frm %d", geph->sat, frame_num);
        decodeGlonassFrame4(buf, geph);
        break;
    }

    // Only spit out ephemeris if we got the fourth frame.  This keeps us from spamming too much
    if (!geph->got_frame1 || !geph->got_frame2 || !geph->got_frame3 || !geph->got_frame4 ||
        frame_num != 4)
    {
        return false;
    }

    // Handle The day wrap, because we are comparing GPS with Gal
    // to get the beginning of the day, we might have the wrong
    // day.  This assumes that we get at least two GLONASS ephemeris
    // message per day, which is probably okay
    if ((GPS_time_ - geph->toe).toSec() > UTCTime::SEC_IN_DAY / 2)
        geph->toe += (int)(UTCTime::SEC_IN_DAY);
    else if ((GPS_time_ - geph->toe).toSec() < -(UTCTime::SEC_IN_DAY / 2))
        geph->toe -= (int)(UTCTime::SEC_IN_DAY);

    if ((GPS_time_ - geph->tof).toSec() > UTCTime::SEC_IN_DAY / 2)
        geph->tof += (int)(UTCTime::SEC_IN_DAY);
    else if ((GPS_time_ - geph->tof).toSec() < -(UTCTime::SEC_IN_DAY / 2))
        geph->tof -= (int)(UTCTime::SEC_IN_DAY);

    return true;
}

bool NavParser::decodeGlonass(const ublox::RXM_SFRBX_t &msg, GlonassEphemeris &geph)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(msg.dwrd);
    uint8_t buf[64];

    if (msg.svId == 255)
        return false;  // svId==255 means UBLOX doesn't know who this data came from
    if (GPS_time_ == UTCTime{0, 0})
        return false;  // We use the GPS week count to calculate the GLONASS time

    int prn = msg.svId;
    int sat = msg.svId;

    int i = 0;
    for (int k = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            buf[k++] = p[3 - j];  // Switch order of bytes
        }
        p += 4;
    }

    // Check the Hamming Code
    if (!gloTest(buf))
    {
        return -1;
    }

    // reset the geph parser if the time mark (MB) has changed
    int time_mark = (uint16_t)(buf[12] << 8) | buf[13];
    if (time_mark != geph.time_mark)
    {
        geph.got_frame1 = geph.got_frame2 = geph.got_frame3 = geph.got_frame4 = false;
    }
    geph.time_mark = time_mark;

    if (!decodeGlonassString(buf, &geph))
        return 0;

    geph.frq = msg.freqId - 7;
    geph.sat = msg.svId;
    geph.gnssID = msg.gnssId;

    // Call the callbacks
    for (auto &cb : geph_callbacks) cb(geph);

    return true;
}

bool NavParser::decodeBeidou(const uint8_t *const buf, Ephemeris *eph)
{
    // TODO
}
bool NavParser::decodeGalileo(const uint8_t *const buf, Ephemeris *eph)
{
    // TODO
}
