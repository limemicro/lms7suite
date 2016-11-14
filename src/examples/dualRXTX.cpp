/**
    @file   dualRXTX.cpp
    @author Lime Microsystems (www.limemicro.com)
    @brief  Dual channel RX/TX example
 */

#include <cstdlib>
#include "lime/LimeSuite.h"
#include <iostream>
#include "math.h"
#include <thread>
#include <chrono>

#ifdef USE_GNU_PLOT
#include "gnuPlotPipe.h"
#endif

using namespace std;

//Device structure, should be initialize to NULL
lms_device_t* device = NULL;

int error()
{
    //print last error message
    cout << "ERROR:" << LMS_GetLastErrorMessage();
    if (device != NULL)
        LMS_Close(device);
    exit(-1);
}

int main(int argc, char** argv)
{
    //Find devices
    int n;
    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    if ((n = LMS_GetDeviceList(list)) < 0) //NULL can be passed to only get number of devices
        error();

    cout << "Devices found: " << n << endl; //print number of devices
    if (n < 1)
        return -1;

    //open the first device
    if (LMS_Open(&device, list[0], NULL))
        error();

    //Initialize device with default configuration
    //Do not use if you want to keep existing configuration
    if (LMS_Init(device) != 0)
        error();

    //Get number of channels
    if ((n = LMS_GetNumChannels(device, LMS_CH_RX)) < 0)
        error();
    cout << "Number of RX channels: " << n << endl;
    if ((n = LMS_GetNumChannels(device, LMS_CH_TX)) < 0)
        error();
    cout << "Number of TX channels: " << n << endl;

    //Enable RX channel
    //Channels are numbered starting at 0
    if (LMS_EnableChannel(device, LMS_CH_RX, 0, true) != 0)
        error();
    if (LMS_EnableChannel(device, LMS_CH_RX, 1, true) != 0)
        error();
    //Enable TX channels
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, true) != 0)
        error();
    if (LMS_EnableChannel(device, LMS_CH_TX, 1, true) != 0)
        error();

    //Set RX center frequency to 1 GHz
    //Automatically selects antenna port
    if (LMS_SetLOFrequency(device, LMS_CH_RX, 0, 1e9) != 0)
        error();
    if (LMS_SetLOFrequency(device, LMS_CH_RX, 1, 1e9) != 0)
        error();
    //Set TX center frequency to 1 GHz
    //Automatically selects antenna port
    if (LMS_SetLOFrequency(device, LMS_CH_TX, 0, 1e9) != 0)
        error();
    if (LMS_SetLOFrequency(device, LMS_CH_TX, 1, 1e9) != 0)
        error();

    //Set sample rate to 10 MHz, preferred oversampling in RF 4x
    //This set sampling rate for all channels
    if (LMS_SetSampleRate(device, 10e6, 4) != 0)
        error();

    //Set RX gain
    if (LMS_SetNormalizedGain(device, LMS_CH_RX, 0, 0.7) != 0)
        error();
    if (LMS_SetNormalizedGain(device, LMS_CH_RX, 1, 0.7) != 0)
        error();
    //Set TX gain
    if (LMS_SetNormalizedGain(device, LMS_CH_TX, 0, 0.4) != 0)
        error();
    if (LMS_SetNormalizedGain(device, LMS_CH_TX, 1, 0.4) != 0)
        error();

    //Enable test signals generation in RX channels
    //To receive data from RF, remove these lines or change signal to LMS_TESTSIG_NONE
    if (LMS_SetTestSignal(device, LMS_CH_RX, 0, LMS_TESTSIG_NCODIV4, 0, 0) != 0)
        error();
    if (LMS_SetTestSignal(device, LMS_CH_RX, 1, LMS_TESTSIG_NCODIV8F, 0, 0) != 0)
        error();

    //Streaming Setup

    const int chCount = 2; //number of RX/TX steams
    lms_stream_t rx_streams[chCount];
    lms_stream_t tx_streams[chCount];
    //Initialize streams
    //All streams setups should be done before starting streams. New streams cannot be set-up if at least stream is running.
    for (int i = 0; i < chCount; ++i)
    {
        rx_streams[i].channel = i; //channel number
        rx_streams[i].fifoSize = 1024 * 128; //fifo size in samples
        rx_streams[i].throughputVsLatency = 0.0; //optimize for minimum latency
        rx_streams[i].isTx = false; //RX channel
        rx_streams[i].dataFmt = lms_stream_t::LMS_FMT_I16; //16-bit integers
        if (LMS_SetupStream(device, &rx_streams[i]) != 0)
            error();
        tx_streams[i].channel = i; //channel number
        tx_streams[i].fifoSize = 1024 * 128; //fifo size in samples
        tx_streams[i].throughputVsLatency = 0.0; //optimize for minimum latency
        tx_streams[i].isTx = true; //TX channel
        tx_streams[i].dataFmt = lms_stream_t::LMS_FMT_I16; //16-bit integers
        if (LMS_SetupStream(device, &tx_streams[i]) != 0)
            error();
    }

    //Initialize data buffers
    const int bufersize = 1024 * 8; //complex samples per buffer
    int16_t * buffers[chCount];
    for (int i = 0; i < chCount; ++i)
    {
        buffers[i] = new int16_t[bufersize * 2]; //buffer to hold complex values (2*samples))
    }

    //Start streaming
    for (int i = 0; i < chCount; ++i)
    {
        LMS_StartStream(&rx_streams[i]);
        LMS_StartStream(&tx_streams[i]);
    }

    //Streaming

    lms_stream_meta_t rx_metadata; //Use metadata for additional control over sample receive function behaviour
    rx_metadata.flushPartialPacket = false; //Do not discard data remainder when read size differs from packet size
    rx_metadata.waitForTimestamp = false; //Do not wait for specific timestamps

    lms_stream_meta_t tx_metadata; //Use metadata for additional control over sample send function behaviour
    tx_metadata.flushPartialPacket = false; //Do not discard data remainder when read size differs from packet size
    tx_metadata.waitForTimestamp = true; //Enable synchronization to HW timestamp

#ifdef USE_GNU_PLOT
    GNUPlotPipe gp;
#endif
    auto t1 = chrono::high_resolution_clock::now();
    auto t2 = chrono::high_resolution_clock::now();

    while (chrono::high_resolution_clock::now() - t1 < chrono::seconds(10)) //run for 10 seconds
    {
        for (int i = 0; i < chCount; ++i)
        {
            int samplesRead;
            //Receive samples
            samplesRead = LMS_RecvStream(&rx_streams[i], buffers[i], bufersize, &rx_metadata, 1000);
            //Send samples with 1024*1024 sample delay from RX (waitForTimestamp is enabled)
            tx_metadata.timestamp = rx_metadata.timestamp + 1024 * 512;
            LMS_SendStream(&tx_streams[i], buffers[i], samplesRead, &tx_metadata, 1000);
        }

        //Print stats every 1s
        if (chrono::high_resolution_clock::now() - t2 > chrono::seconds(1))
        {
#ifdef USE_GNU_PLOT
            //Plot samples 
            t2 = chrono::high_resolution_clock::now();
            gp.write("set title 'Channels Rx AB'\n");
            gp.write("set size square\n set xrange[-2050:2050]\n set yrange[-2050:2050]\n");
            gp.write("plot '-' with points");
            for (int i = 1; i < chCount; ++i)
                gp.write(", '-' with points");
            gp.write("\n");

            for (int i = 0; i < chCount; ++i)
            {
                for (uint32_t j = 0; j < bufersize / 8; ++j)
                    gp.writef("%i %i\n", buffers[i][2 * j], buffers[i][2 * j + 1]);
                gp.write("e\n");
                gp.flush();
            }
#endif
            //Print stats
            lms_stream_status_t status;
            LMS_GetStreamStatus(rx_streams, &status); //Obtain RX stream stats
            cout << "RX rate: " << status.linkRate / 1e6 << " MB/s\n"; //link data rate (both channels))
            cout << "RX 0 FIFO: " << 100 * status.fifoFilledCount / status.fifoSize << "%" << endl; //percentage of RX 0 fifo filled

            LMS_GetStreamStatus(tx_streams, &status); //Obtain TX stream stats
            cout << "TX rate: " << status.linkRate / 1e6 << " MB/s\n"; //link data rate (both channels))
            cout << "TX 0 FIFO: " << 100 * status.fifoFilledCount / status.fifoSize << "%" << endl; //percentage of TX 0 fifo filled
        }
    }

    //Stop streaming
    for (int i = 0; i < chCount; ++i)
    {
        LMS_StopStream(&rx_streams[i]); //stream is stopped but can be started again with LMS_StartStream()
        LMS_DestroyStream(device, &rx_streams[i]); //stream is deallocated and can no longer be used
        LMS_StopStream(&tx_streams[i]);
        LMS_DestroyStream(device, &tx_streams[i]);
        delete [] buffers[i];
    }
    //Close device
    LMS_Close(device);

    return 0;
}

