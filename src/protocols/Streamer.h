/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Streamer.h
 * Author: ignas
 *
 * Created on November 20, 2017, 12:29 PM
 */

#ifndef STREAMER_H
#define STREAMER_H

#include "dataTypes.h"
#include "fifo.h"
#include <vector>

namespace lime
{

class IConnection;
class FPGA;
class Streamer;
class LMS7002M;

/*!
 * Type alias redefinition. Must be aligned with the IConnection.h.
 */
using host_time_t = uint64_t;

/*!
 * The stream config structure is used with the SetupStream() API.
 */
struct LIME_API StreamConfig
{
    StreamConfig(void){};

    //! True for transmit stream, false for receive
    bool isTx;

    uint8_t channelID;
    bool align;
    float performanceLatency;

    //! Possible stream data formats
    enum StreamDataFormat
    {
        FMT_INT16,
        FMT_INT12,
        FMT_FLOAT32,
    };

    /*!
     * The buffer length is a size in samples
     * that used for allocating internal buffers.
     * Default: 0, meaning automatic selection
     */
    size_t bufferLength;

    //! The format of the samples in Read/WriteStream().
    StreamDataFormat format;

    /*!
     * The format of samples over the wire.
     * This is not the format presented to the API caller.
     * Choosing a compressed format can decrease link use
     * at the expense of additional processing on the PC
     * Default: STREAM_12_BIT_IN_16
     */
    StreamDataFormat linkFormat;
};

class LIME_API StreamChannel
{
public:
    struct Metadata
    {
        uint64_t timestamp;
        uint32_t flags;
    };

    struct Info
    {
        int fifoSize;
        int fifoItemsCount;
        int overrun;
        int underrun;
        bool active;
        float linkRate;
        int droppedPackets;
        uint64_t timestamp;
    };

    StreamChannel(Streamer* streamer);
    ~StreamChannel();

    void Setup(StreamConfig conf);
    void Close();
    int Read(void* samples, const uint32_t count, Metadata* meta, const int32_t timeout_ms = 100);
    int Write(const void* samples, const uint32_t count, const Metadata* meta, const int32_t timeout_ms = 100);
    StreamChannel::Info GetInfo();
    int GetStreamSize();

    bool IsActive() const;
    int Start();
    int Stop();
    StreamConfig config;
    Streamer* mStreamer;
    unsigned pktLost;
    bool mActive;
    bool used;
    RingFIFO* fifo;
protected:

};

/*!
 * A trivial implementation of the Spinlock which satisfies the Lockable concept requirements.
 */
class Spin
{
public:
    Spin() = default;
    Spin(const Spin&) = delete;
    Spin& operator= (const Spin&) = delete;
    void lock() {while (spin.test_and_set(std::memory_order_acquire));}
    bool try_lock() {return !spin.test_and_set(std::memory_order_acquire);}
    void unlock() {spin.clear(std::memory_order_release);}
private:
    std::atomic_flag spin {ATOMIC_FLAG_INIT};
};

/*!
 * The class encapsulates all the necessary logic for collection and providing a relative timestamps.
 */
class RelativeTimestamp
{
public:
    void Set(uint64_t hw, host_time_t host)
    {
        std::unique_lock<Spin> l(lock, std::try_to_lock);
        if (!l.owns_lock()) {
            // Optimized exit. It's OK to not have the latest information, so to avoid waiting skip the update.
            if (++skip < skip_limit)
                return;
            // Prevent thread starvation. If we've skipped the update a lot of times, we should wait this time.
            l.lock();
        }

        skip = 0;
        hwstamp = hw;
        hoststamp = host;
    }

    void Get(uint64_t &hw, host_time_t &host) const
    {
        std::lock_guard<Spin> l(lock);
        hw = hwstamp;
        host = hoststamp;
    }

private:
    uint64_t hwstamp {0};
    host_time_t hoststamp {0};
    const int skip_limit {3};
    int skip {skip_limit};
    mutable Spin lock;
};

class Streamer
{
public:
    Streamer(FPGA* f, LMS7002M* chip, int id);
    ~Streamer();

    StreamChannel* SetupStream(const StreamConfig& config);
    int GetStreamSize(bool tx);

    uint64_t GetHardwareTimestamp(void);
    void SetHardwareTimestamp(const uint64_t now);
    void GetRelativeTimestamp(uint64_t &, host_time_t &) const;
    int UpdateThreads(bool stopAll = false);

    std::atomic<uint32_t> rxDataRate_Bps;
    std::atomic<uint32_t> txDataRate_Bps;
    IConnection* dataPort;
    std::thread rxThread;
    std::thread txThread;
    std::atomic<bool> terminateRx;
    std::atomic<bool> terminateTx;

    std::vector<StreamChannel> mRxStreams;
    std::vector<StreamChannel> mTxStreams;
    std::atomic<uint64_t> rxLastTimestamp;
    std::atomic<uint64_t> txLastTimestamp;
    RelativeTimestamp relTimestamp;
    uint64_t mTimestampOffset;
    int streamSize;
    unsigned txBatchSize;
    unsigned rxBatchSize;
    StreamConfig::StreamDataFormat dataLinkFormat;
    void ReceivePacketsLoop();
    void TransmitPacketsLoop();
private:
    void ResizeChannelBuffers();
    void AlignRxTSP();
    void AlignRxRF(bool restoreValues);
    void AlignQuadrature(bool restoreValues);
    void RstRxIQGen();
    double GetPhaseOffset(int bin);
    FPGA* fpga;
    LMS7002M* lms;
    int chipId;
};
}

#endif /* STREAMER_H */

