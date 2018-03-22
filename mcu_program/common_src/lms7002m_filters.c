#include "lms7002m_controls.h"
#include "lms7002m_calibrations.h"
#include "lms7002m_filters.h"
#include "LMS7002M_parameters_compact.h"
#include "spi.h"
#include <math.h>
#include "mcu_defines.h"

#ifdef __cplusplus
    #include <vector>
    #include "ErrorReporting.h"
    #include <math.h>
    using namespace lime;
#endif // __cplusplus

#define E_DECREASE_R 0x0080
#define E_INCREASE_R 0x0081

//rx lpf range limits
static ROM const float_type RxLPF_RF_LimitLow = 1.4e6;
static ROM const float_type RxLPF_RF_LimitHigh = 130e6;

//tx lpf range limits
static ROM const float_type TxLPF_RF_LimitLow = 5e6;
static ROM const float_type TxLPF_RF_LimitLowMid = 40e6;
static ROM const float_type TxLPF_RF_LimitMidHigh = 50e6;
static ROM const float_type TxLPF_RF_LimitHigh = 130e6;

static uint8_t ConfigCGEN_ForLPF_IF(float IF_Hz)
{
    uint8_t cgenMultiplier = clamp(IF_Hz*20 / 46.08e6 + 0.5, 2, 13);
    return SetFrequencyCGEN(46.08e6 * cgenMultiplier + 10e6);
}

static uint8_t RxFilterSearch(const uint16_t addr, const uint8_t msblsb, const uint16_t rssi_3dB, const uint16_t stepLimit)
{
    const bool doDecrement = GetRSSI() < rssi_3dB;
    int16_t value = Get_SPI_Reg_bits(addr, msblsb);
    const uint16_t maxValue = pow2((msblsb>>4)-(msblsb&0xF)+1)-1;
    uint16_t stepSize = 1;
    while(1)
    {
        stepSize <<= 1;
        if(doDecrement)
            value -= stepSize;
        else
            value += stepSize;
        value = clamp(value, 0, maxValue);
        Modify_SPI_Reg_bits(addr, msblsb, value);
        if(doDecrement != (GetRSSI() < rssi_3dB))
            break;
        if(stepSize >= stepLimit)
            return doDecrement ? E_INCREASE_R : E_DECREASE_R;
    }
    while(stepSize > 1)
    {
        stepSize /= 2;
        if(GetRSSI() >= rssi_3dB)
            value += stepSize;
        else
            value -= stepSize;
        Modify_SPI_Reg_bits(addr, msblsb, value);
    }
    return 0;
}

uint8_t TuneRxFilterSetup(const float_type rx_lpf_IF)
{
    uint8_t status;
    const uint16_t ch = SPI_read(0x0020);
    uint8_t g_tia_rfe = Get_SPI_Reg_bits(G_TIA_RFE);
    uint8_t g_pga_rbb = Get_SPI_Reg_bits(G_PGA_RBB);

	if(RxLPF_RF_LimitLow/2 > rx_lpf_IF || rx_lpf_IF > RxLPF_RF_LimitHigh/2)
        return MCU_RX_LPF_OUT_OF_RANGE;
#define BATCH_RX_SETUP 1
#if BATCH_RX_SETUP
    {
        ROM const uint16_t RxFilterSetupAddr[] = {0x0085,0x010D,0x0113,0x0114, 0x0084, 0x008B};
        ROM const uint16_t RxFilterSetupData[] = {0x0001,0x0100,0x0004,0x0010, 0x0400, 0x2100};
        ROM const uint16_t RxFilterSetupMask[] = {0x0007,0x0188,0x003C,0x001F, 0xF83F, 0xC1FF};
        ROM const uint16_t RxFilterSetupWrOnlyAddr[] = {0x0082,0x0086,0x0087,0x0088,0x0089,0x008A,0x008C,0x0100,0x0101,0x0102,0x0103,0x0104,0x0105,0x0106,0x0107,0x0108,0x0109,0x010A,0x010C,0x0115,0x0116,0x0117,0x0118,0x0119,0x011A,0x0200,0x0201,0x0202,0x0203,0x0204,0x0205,0x0206,0x0207,0x0208,0x0209,0x0240,0x0400,0x0401,0x0402,0x0403,0x0404,0x0405,0x0406,0x0407,0x0408,0x0409,0x040A,0x040C,0x0440};
        ROM const uint16_t RxFilterSetupWrOnlyData[] = {0x8003,0x4901,0x0400,0x0780,0x0020,0x0514,0x067B,0x3409,0x6001,0x3180,0x0612,0x0088,0x0007,0x318C,0x318C,0x0426,0x61C1,0x104C,0x88C5,0x0009,0x8180,0x280C,0x018C,0x528B,0x2E02,0x008D,0x07FF,0x07FF,0x0000,0x0000,0x0000,0x0000,0x0000,0x2070,0x0000,0x0020,0x0081,0x07FF,0x07FF,0x4000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x1001,0x2038,0x0020};
        ROM const RegisterBatch batch = {
            RxFilterSetupAddr, RxFilterSetupData, RxFilterSetupMask, sizeof(RxFilterSetupAddr)/sizeof(uint16_t),
            RxFilterSetupWrOnlyAddr, RxFilterSetupWrOnlyData, sizeof(RxFilterSetupWrOnlyAddr)/sizeof(uint16_t), sizeof(RxFilterSetupWrOnlyData)/sizeof(uint16_t)};
        WriteMaskedRegs(&batch);
    }
#else
    const uint8_t ict_vco = Get_SPI_Reg_bits(ICT_VCO_CGEN);
    BeginBatch("RxFilterSetup");

    SetDefaults(SECTION_RFE);
    Modify_SPI_Reg_bits(SEL_PATH_RFE, 2);
    Modify_SPI_Reg_bits(G_RXLOOPB_RFE, 1);
    Modify_SPI_Reg_bits(PD_RLOOPB_2_RFE, 0);
    Modify_SPI_Reg_bits(EN_INSHSW_LB2_RFE, 0);
    Modify_SPI_Reg_bits(PD_MXLOBUF_RFE, 0);
    Modify_SPI_Reg_bits(PD_QGEN_RFE, 0);
    Modify_SPI_Reg_bits(RFB_TIA_RFE, 16);

    //RBB
    SetDefaults(SECTION_RBB);
    Modify_SPI_Reg_bits(ICT_PGA_OUT_RBB, 20);
    Modify_SPI_Reg_bits(ICT_PGA_IN_RBB, 20);

    //TRF
    SetDefaults(SECTION_TRF);
    Modify_SPI_Reg_bits(L_LOOPB_TXPAD_TRF, 0);
    Modify_SPI_Reg_bits(EN_LOOPB_TXPAD_TRF, 1);
    Modify_SPI_Reg_bits(SEL_BAND1_TRF, 0);
    Modify_SPI_Reg_bits(SEL_BAND2_TRF, 1);

    //TBB
    SetDefaults(SECTION_TBB);
    Modify_SPI_Reg_bits(CG_IAMP_TBB, 1);
    Modify_SPI_Reg_bits(ICT_IAMP_FRP_TBB, 1);
    Modify_SPI_Reg_bits(ICT_IAMP_GG_FRP_TBB, 6);

    //AFE
    SetDefaults(SECTION_AFE);
    Modify_SPI_Reg_bits(PD_RX_AFE2, 0);

    //LDO
    //do nothing
    //XBUF
    Modify_SPI_Reg_bits(PD_XBUF_RX, 0);
    Modify_SPI_Reg_bits(PD_XBUF_TX, 0);
    Modify_SPI_Reg_bits(EN_G_XBUF, 1);

    //TxTSP
    SetDefaults(SECTION_TxTSP);
    SetDefaults(SECTION_TxNCO);
    Modify_SPI_Reg_bits(TSGMODE_TXTSP, 1);
    Modify_SPI_Reg_bits(INSEL_TXTSP, 1);
    Modify_SPI_Reg_bits(CMIX_SC_TXTSP, 1);
    Modify_SPI_Reg_bits(GFIR3_BYP_TXTSP, 1);
    Modify_SPI_Reg_bits(GFIR2_BYP_TXTSP, 1);
    Modify_SPI_Reg_bits(GFIR1_BYP_TXTSP, 1);

    //RxTSP
    SetDefaults(SECTION_RxTSP);
    SetDefaults(SECTION_RxNCO);
    Modify_SPI_Reg_bits(AGC_MODE_RXTSP, 1);
    Modify_SPI_Reg_bits(CMIX_BYP_RXTSP, 0);
    Modify_SPI_Reg_bits(GFIR3_BYP_RXTSP, 1);
    Modify_SPI_Reg_bits(GFIR2_BYP_RXTSP, 1);
    Modify_SPI_Reg_bits(GFIR1_BYP_RXTSP, 1);
    Modify_SPI_Reg_bits(AGC_AVG_RXTSP, 1);
    Modify_SPI_Reg_bits(HBD_OVR_RXTSP, 4);
    Modify_SPI_Reg_bits(CMIX_GAIN_RXTSP, 0);
    Modify_SPI_Reg_bits(CMIX_SC_RXTSP, 1);

    //CGEN
    SetDefaults(SECTION_CGEN);
    Modify_SPI_Reg_bits(ICT_VCO_CGEN, ict_vco);
    EndBatch();

    //BIAS
/*
    {
        const uint8_t rp_calib_bias = Get_SPI_Reg_bits(RP_CALIB_BIAS);
        SetDefaults(SECTION_BIAS);
        Modify_SPI_Reg_bits(RP_CALIB_BIAS, rp_calib_bias);
    }
*/
#endif
    Modify_SPI_Reg_bits(G_TIA_RFE, g_tia_rfe);
    if(g_pga_rbb == 31) {
        Modify_SPI_Reg_bits(G_PGA_RBB, 22);
    }
    else {
        Modify_SPI_Reg_bits(G_PGA_RBB, g_pga_rbb);
    }

    status = ConfigCGEN_ForLPF_IF(rx_lpf_IF);
    if(status != MCU_NO_ERROR)
        return status;

    //SXR
    Modify_SPI_Reg_bits(MAC, 1);
    SetDefaultsSX();
    status = SetFrequencySX(LMS7002M_Rx, 539.9e6);
    if(status != MCU_NO_ERROR)
        return status;

    //SXT
    Modify_SPI_Reg_bits(MAC, 2);
    SetDefaultsSX();
    status = SetFrequencySX(LMS7002M_Tx, 550e6);
    if(status != MCU_NO_ERROR)
        return status;
    SPI_write(0x0020, ch);
    //LimeLight & PAD
    //do nothing

    LoadDC_REG_TX_IQ();
    SetNCOFrequency(LMS7002M_Tx, 10e6, 0); //0
    SetNCOFrequency(LMS7002M_Rx, 0, 0); //0


    if(rx_lpf_IF <= 54e6)
    {
        //Modify_SPI_Reg_bits(CFB_TIA_RFE, 1);
        //Modify_SPI_Reg_bits(CCOMP_TIA_RFE, 0);
        SPI_write(0x0112, 1);

        Modify_SPI_Reg_bits(RCOMP_TIA_RFE, 15);
    }
    else
    {
        int16_t cfb_tia_rfe;
        int8_t ccomp_tia_rfe;
        if(g_tia_rfe == 3 || g_tia_rfe == 2)
        {
            cfb_tia_rfe = (int16_t)( 1680e6/rx_lpf_IF - 10);
            ccomp_tia_rfe = cfb_tia_rfe/100;
        }
        else if(g_tia_rfe == 1)
        {
            cfb_tia_rfe = (int16_t)( 5400e6/rx_lpf_IF - 15);
            ccomp_tia_rfe = cfb_tia_rfe/100 + 1;
        }
        else
            return MCU_RX_INVALID_TIA;//ReportError(EINVAL ,"Calibration setup: G_TIA_RFE value not allowed");
        SPI_write(0x0112, (clamp(ccomp_tia_rfe, 0, 15)<<8) | clamp(cfb_tia_rfe, 0, 4095));

        Modify_SPI_Reg_bits(RCOMP_TIA_RFE, clamp(15-cfb_tia_rfe/100, 0, 15));
    }
    {
        const int8_t rcc_ctl_pga_rbb = clamp((430 * pow(0.65, g_pga_rbb/10) - 110.35)/20.45 + 16, 0, 31);
        SPI_write(0x011A, rcc_ctl_pga_rbb<<9 | GetValueOf_c_ctl_pga_rbb(g_pga_rbb));
    }
    if(rx_lpf_IF < 18e6)
    {
        //Modify_SPI_Reg_bits(PD_LPFL_RBB, 0);
        //Modify_SPI_Reg_bits(PD_LPFH_RBB, 1);
        Modify_SPI_Reg_bits(0x0115, MSB_LSB(3, 2), 2);

        Modify_SPI_Reg_bits(INPUT_CTL_PGA_RBB, 0);
        {
            const float freqIF = rx_lpf_IF*1.3;
            int16_t c_ctl_lpfl_rbb = clamp(2160e6/freqIF - 103, 0, 2047);

            uint8_t rcc_ctl_lpfl_rbb = 5;
            if(freqIF < 15e6)
                rcc_ctl_lpfl_rbb = 4;
            if(freqIF < 10e6)
                rcc_ctl_lpfl_rbb = 3;
            if(freqIF < 5e6)
                rcc_ctl_lpfl_rbb = 2;
            if(freqIF < 3e6)
                rcc_ctl_lpfl_rbb = 1;
            if(freqIF < 1.4e6)
                rcc_ctl_lpfl_rbb = 0;
            SPI_write(0x0117, rcc_ctl_lpfl_rbb<<11 | c_ctl_lpfl_rbb);
        }
    }
    else if(rx_lpf_IF <= 54e6)
    {
        //Modify_SPI_Reg_bits(PD_LPFL_RBB, 1);
        //Modify_SPI_Reg_bits(PD_LPFH_RBB, 0);
        Modify_SPI_Reg_bits(0x0115, MSB_LSB(3, 2), 1);

        Modify_SPI_Reg_bits(INPUT_CTL_PGA_RBB, 1);
        {
            uint8_t c_ctl_lpfh_rbb = clamp( 6000e6/(rx_lpf_IF*1.3) - 50, 0, 255);
            uint8_t rcc_ctl_lpfh_rbb = clamp((rx_lpf_IF*1.3/10)-3, 0, 8);
            Modify_SPI_Reg_bits(0x0116, MSB_LSB(10, 8), (rcc_ctl_lpfh_rbb<<8) | c_ctl_lpfh_rbb);
        }
    }
    else // rx_lpf_IF > 54e6
    {
        //Modify_SPI_Reg_bits(PD_LPFL_RBB, 1);
        //Modify_SPI_Reg_bits(PD_LPFH_RBB, 1);
        Modify_SPI_Reg_bits(0x0115, MSB_LSB(3, 2), 3);

        Modify_SPI_Reg_bits(INPUT_CTL_PGA_RBB, 2);
    }

    if( (ch&0x3) == 2)
    {
        Modify_SPI_Reg_bits(PD_TX_AFE2, 0);
        Modify_SPI_Reg_bits(MAC, 1);
        Modify_SPI_Reg_bits(EN_NEXTRX_RFE, 1);
        Modify_SPI_Reg_bits(EN_NEXTTX_TRF, 1);
        Modify_SPI_Reg_bits(MAC, ch);
    }
    return MCU_NO_ERROR;
}

uint8_t TuneRxFilter(const float_type rx_lpf_freq_RF)
{
    uint16_t rssi_3dB ;
    uint8_t status = 0;
    //calculate intermediate frequency
    const float_type rx_lpf_IF = rx_lpf_freq_RF/2;
    SaveChipState(0);

    status = TuneRxFilterSetup(rx_lpf_IF);
    if(status != MCU_NO_ERROR)
        goto RxFilterSearchEndStage;
    UpdateRSSIDelay();
    {
        uint8_t g_rxloopb_rfe = Get_SPI_Reg_bits(G_RXLOOPB_RFE);
        while(GetRSSI() < 0x2700 && g_rxloopb_rfe < 14)
        {
            g_rxloopb_rfe += 2;
            Modify_SPI_Reg_bits(G_RXLOOPB_RFE, g_rxloopb_rfe);
        }
    }
    {
        uint8_t cg_iamp_tbb = Get_SPI_Reg_bits(CG_IAMP_TBB);
        while(GetRSSI() < 0x2700 && cg_iamp_tbb < 30)
        {
            cg_iamp_tbb += 2;
            Modify_SPI_Reg_bits(CG_IAMP_TBB, cg_iamp_tbb);
        }
    }

    rssi_3dB = GetRSSI() * 0.7071 * pow(10, (-0.0018 * rx_lpf_IF/1e6)/20);

    if(rx_lpf_IF <= 54e6)
    {
        status = SetFrequencySX(LMS7002M_Rx, 539.9e6-rx_lpf_IF*1.3);
        if(status != MCU_NO_ERROR)
            goto RxFilterSearchEndStage;
        SetNCOFrequency(LMS7002M_Rx, rx_lpf_IF*1.3, 0); //0

        if(rx_lpf_IF < 18e6)
        {
            //LPFL START
            status = RxFilterSearch(C_CTL_LPFL_RBB, rssi_3dB, 2048);
            if(status == E_DECREASE_R)
            {
                uint8_t r_ctl_lpf = Get_SPI_Reg_bits(R_CTL_LPF_RBB);
                while(r_ctl_lpf > 1)
                {
                    r_ctl_lpf /= 2;
                    Modify_SPI_Reg_bits(R_CTL_LPF_RBB, r_ctl_lpf);
                    status = RxFilterSearch(C_CTL_LPFL_RBB, rssi_3dB, 2048);
                }
            }
            if(status == E_INCREASE_R)
            {
                uint8_t r_ctl_lpf = Get_SPI_Reg_bits(R_CTL_LPF_RBB);
                while(r_ctl_lpf < 31)
                {
                    r_ctl_lpf += 4;
                    if(r_ctl_lpf > 31)
                        break;
                    Modify_SPI_Reg_bits(R_CTL_LPF_RBB, r_ctl_lpf);
                    status = RxFilterSearch(C_CTL_LPFL_RBB, rssi_3dB, 2048);
                }
            }
            else if(status != 0)
                goto RxFilterSearchEndStage;
            //LPFL END
        }
        else
        {
            //LPFH START
            status = RxFilterSearch(C_CTL_LPFH_RBB, rssi_3dB, 256);
            if(status == E_DECREASE_R)
            {
                uint8_t r_ctl_lpf = Get_SPI_Reg_bits(R_CTL_LPF_RBB);
                while(r_ctl_lpf > 0)
                {
                    r_ctl_lpf -= 1;
                    Modify_SPI_Reg_bits(R_CTL_LPF_RBB, r_ctl_lpf);
                    if(GetRSSI() < rssi_3dB)
                    {
                        status = 0;
                        break;
                    }

                }
            }
            if(status == E_INCREASE_R)
            {
                uint8_t r_ctl_lpf = Get_SPI_Reg_bits(R_CTL_LPF_RBB);
                while(r_ctl_lpf < 31)
                {
                    r_ctl_lpf += 1;
                    if(r_ctl_lpf > 31)
                        break;
                    Modify_SPI_Reg_bits(R_CTL_LPF_RBB, r_ctl_lpf);
                    if(GetRSSI() > rssi_3dB)
                    {
                        status = 0;
                        break;
                    }
                }
            }
            else if(status != 0)
                goto RxFilterSearchEndStage;
            //LPFH END
        }
        status = SetFrequencySX(LMS7002M_Rx, 539.9e6-rx_lpf_IF);
        if(status != MCU_NO_ERROR)
            goto RxFilterSearchEndStage;
        SetNCOFrequency(LMS7002M_Rx, rx_lpf_IF, 0); //0

        {
            uint16_t cfb_tia_rfe;
            uint8_t g_tia_rfe = Get_SPI_Reg_bits(G_TIA_RFE);
            if(g_tia_rfe == 3 || g_tia_rfe == 2)
                cfb_tia_rfe = (int)( 1680e6 / (rx_lpf_IF * 0.72) - 10);
            else if(g_tia_rfe == 1)
                cfb_tia_rfe = (int)( 5400e6 / (rx_lpf_IF * 0.72) - 15);
            else
            {
                status = MCU_RX_INVALID_TIA; //ReportError(EINVAL, "g_tia_rfe not allowed value");
                goto RxFilterSearchEndStage;
            }
            Modify_SPI_Reg_bits(CFB_TIA_RFE, clamp(cfb_tia_rfe, 0, 4095));

            {
                uint8_t ccomp_tia_rfe;
                if(g_tia_rfe > 1)
                    ccomp_tia_rfe = cfb_tia_rfe / 100;
                else if(g_tia_rfe == 1)
                    ccomp_tia_rfe = cfb_tia_rfe / 100 + 1;
                else
                {
                    status = MCU_RX_INVALID_TIA;//ReportError(EINVAL, "g_tia_rfe not allowed value");
                    goto RxFilterSearchEndStage;
                }

                Modify_SPI_Reg_bits(CCOMP_TIA_RFE, clamp(ccomp_tia_rfe, 0, 15));
            }
            Modify_SPI_Reg_bits(RCOMP_TIA_RFE, clamp(15 - cfb_tia_rfe/100, 0, 15));
        }
    }
    else//if(rx_lpf_IF > 54e6)
    {
        status = SetFrequencySX(LMS7002M_Rx, 539.9e6 - rx_lpf_IF);
        if(status != 0)
            goto RxFilterSearchEndStage;
        SetNCOFrequency(LMS7002M_Rx, rx_lpf_IF, 0); //0
    }
    //START TIA
    status = RxFilterSearch(CFB_TIA_RFE, rssi_3dB, 4096);
    if(status != MCU_NO_ERROR)
        goto RxFilterSearchEndStage;
    //END TIA

    {
    //Restore settings
    uint16_t ccomp_cfb_tia_rfe = SPI_read(0x0112);
    uint16_t rcc_c_ctl_lpfl_rbb = SPI_read(0x0117);
    uint16_t rcc_c_ctl_pga_rbb = SPI_read(0x011A);
    uint16_t rcc_c_ctl_lpfh_rbb = SPI_read(0x0116) & 0x07FF;
    uint8_t pd_lpfhl = Get_SPI_Reg_bits(0x0115, MSB_LSB(3, 2));
    uint8_t input_ctl_pga_rbb = Get_SPI_Reg_bits(INPUT_CTL_PGA_RBB);
    uint8_t rcomp_tia_rfe = Get_SPI_Reg_bits(RCOMP_TIA_RFE);
RxFilterSearchEndStage:
    SaveChipState(1);
    if(status != MCU_NO_ERROR)
        return status;
    SPI_write(0x0112, ccomp_cfb_tia_rfe);
    SPI_write(0x0117, rcc_c_ctl_lpfl_rbb);
    SPI_write(0x011A, rcc_c_ctl_pga_rbb);
    SPI_write(0x0116, (16 << 11) | rcc_c_ctl_lpfh_rbb);
    SPI_write(0x0118, input_ctl_pga_rbb << 13 | 0x018C);
    SPI_write(0x0114, rcomp_tia_rfe << 5 | 16);
    Modify_SPI_Reg_bits(0x0119, MSB_LSB(14, 5), (20 << 5) | 20);
    Modify_SPI_Reg_bits(0x0115, MSB_LSB(3, 2), pd_lpfhl);
    }
    return MCU_NO_ERROR;
}

uint8_t TuneTxFilterSetup(const float_type tx_lpf_IF)
{
    uint8_t status;
    const uint16_t reg0020 = SPI_read(0x0020);

#define BATCH_TX_SETUP 1
#if BATCH_TX_SETUP
    {
        ROM const uint16_t TxFilterSetupAddr[] = {0x0082,0x0085,0x0100,0x010C,0x010D, 0x0084, 0x008B};
        ROM const uint16_t TxFilterSetupData[] = {0x8001,0x0001,0x0000,0x0000,0x001E, 0x0400, 0x2100};
        ROM const uint16_t TxFilterSetupMask[] = {0x1FFF,0x0007,0x0001,0x0001,0x001E, 0xF83F, 0xC1FF};
        ROM const uint16_t TxFilterSetupWrOnlyAddr[] = {0x0086,0x0087,0x0088,0x0089,0x008A,0x008C,0x0105,0x0106,0x0107,0x0108,0x0109,0x010A,0x0115,0x0116,0x0117,0x0118,0x0119,0x011A,0x0200,0x0201,0x0202,0x0203,0x0204,0x0205,0x0206,0x0207,0x0208,0x0240,0x0241,0x0400,0x0401,0x0402,0x0403,0x0404,0x0405,0x0406,0x0407,0x0408,0x0409,0x040A,0x040C,0x0440,0x0441};
        ROM const uint16_t TxFilterSetupWrOnlyData[] = {0x4901,0x0400,0x0780,0x0020,0x0514,0x067B,0x3007,0x318C,0x318C,0x058C,0x61C1,0x104C,0x000D,0x8180,0x280C,0x618C,0x528C,0x2E02,0x008D,0x07FF,0x07FF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0070,0x0020,0x0000,0x0081,0x07FF,0x07FF,0x4000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x1001,0x2038,0x0020,0x0000};
        ROM const RegisterBatch batch = {
            TxFilterSetupAddr, TxFilterSetupData, TxFilterSetupMask, sizeof(TxFilterSetupAddr)/sizeof(uint16_t),
            TxFilterSetupWrOnlyAddr, TxFilterSetupWrOnlyData, sizeof(TxFilterSetupWrOnlyAddr)/sizeof(uint16_t), sizeof(TxFilterSetupWrOnlyData)/sizeof(uint16_t)};
        WriteMaskedRegs(&batch);
    }
#else
    const uint8_t ict_vco = Get_SPI_Reg_bits(ICT_VCO_CGEN);
    BeginBatch("TxFilterSetup");
    //RFE
    Modify_SPI_Reg_bits(EN_G_RFE, 0);
    Modify_SPI_Reg_bits(0x010D, 4 << 4 | 1, 0xF);

    //RBB
    SetDefaults(SECTION_RBB);
    Modify_SPI_Reg_bits(PD_LPFL_RBB, 1);
    Modify_SPI_Reg_bits(INPUT_CTL_PGA_RBB, 3);
    Modify_SPI_Reg_bits(G_PGA_RBB, 12);
    Modify_SPI_Reg_bits(RCC_CTL_PGA_RBB, 23);

    //TRF
    Modify_SPI_Reg_bits(EN_G_TRF, 0);

    //TBB
    SetDefaults(SECTION_TBB);
    Modify_SPI_Reg_bits(CG_IAMP_TBB, 1);
    Modify_SPI_Reg_bits(LOOPB_TBB, 3);

    //AFE
    //if(reg0020 & 0x3 == 2)
    {
        Modify_SPI_Reg_bits(PD_RX_AFE2, 0);
        Modify_SPI_Reg_bits(PD_TX_AFE2, 0);
    {
    const uint8_t isel_dac_afe = Get_SPI_Reg_bits(ISEL_DAC_AFE);
    SetDefaults(SECTION_AFE);
    Modify_SPI_Reg_bits(ISEL_DAC_AFE, isel_dac_afe);
    }

    //XBUF
    Modify_SPI_Reg_bits(PD_XBUF_RX, 0);
    Modify_SPI_Reg_bits(PD_XBUF_TX, 0);
    Modify_SPI_Reg_bits(EN_G_XBUF, 1);

    SetDefaults(SECTION_CGEN);
    Modify_SPI_Reg_bits(ICT_VCO_CGEN, ict_vco);

    //TxTSP
    SetDefaults(SECTION_TxTSP);
    SetDefaults(SECTION_TxNCO);
    Modify_SPI_Reg_bits(TSGMODE_TXTSP, 1);
    Modify_SPI_Reg_bits(INSEL_TXTSP, 1);

    //RXTSP
    SetDefaults(SECTION_RxTSP);
    SetDefaults(SECTION_RxNCO);
    Modify_SPI_Reg_bits(AGC_MODE_RXTSP, 1);
    Modify_SPI_Reg_bits(AGC_AVG_RXTSP, 1);
    Modify_SPI_Reg_bits(HBD_OVR_RXTSP, 4);
    Modify_SPI_Reg_bits(CMIX_SC_RXTSP, 1);

    EndBatch();
#endif // BATCH_TX_SETUP
    //BIAS
    /*{
    const uint8_t rp_calib_bias = Get_SPI_Reg_bits(RP_CALIB_BIAS);
    //SetDefaults(SECTION_BIAS);
    Modify_SPI_Reg_bits(RP_CALIB_BIAS, rp_calib_bias);
    }*/

    if(tx_lpf_IF <= TxLPF_RF_LimitLowMid/2)
    {
        int16_t rcal_lpflad_tbb;
        const float_type freq = (16.0/20.0)*tx_lpf_IF/1e6;
        Modify_SPI_Reg_bits(PD_LPFH_TBB, 1);
        Modify_SPI_Reg_bits(PD_LPFLAD_TBB, 0);
        Modify_SPI_Reg_bits(PD_LPFS5_TBB, 0);
        //Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, 16);
        Modify_SPI_Reg_bits(R5_LPF_BYP_TBB, 1);

        rcal_lpflad_tbb =
              pow(freq, 4)*1.29858903647958e-16
            + pow(freq, 3)*(-0.000110746929967704)
            + pow(freq, 2)*0.00277593485991029
            + freq*21.0384293169607
            + (-48.4092606238297);
        Modify_SPI_Reg_bits(RCAL_LPFLAD_TBB, clamp(rcal_lpflad_tbb, 0, 255));
    }
    else
    {
        int16_t rcal_lpfh_tbb;
        const float_type freq = tx_lpf_IF/1e6;
        Modify_SPI_Reg_bits(PD_LPFH_TBB, 0);
        Modify_SPI_Reg_bits(PD_LPFLAD_TBB, 1);
        Modify_SPI_Reg_bits(PD_LPFS5_TBB, 1);
        //Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, 16);

        rcal_lpfh_tbb = pow(freq, 4)*1.10383261611112e-06
            + pow(freq, 3)*(-0.000210800032517545)
            + pow(freq,2)*0.0190494874803309
            + freq*1.43317445923528
            + (-47.6950779298333);
        Modify_SPI_Reg_bits(RCAL_LPFH_TBB, clamp(rcal_lpfh_tbb, 0, 255));
    }

    //CGEN
    status = ConfigCGEN_ForLPF_IF(tx_lpf_IF);
    if(status != MCU_NO_ERROR)
        return status;

    //SXR
    Modify_SPI_Reg_bits(MAC, 1);
    Modify_SPI_Reg_bits(PD_VCO, 1);

    //SXT
    Modify_SPI_Reg_bits(MAC, 2);
    Modify_SPI_Reg_bits(PD_VCO, 1);

    SPI_write(0x0020, reg0020);

    //TXTSP
    LoadDC_REG_TX_IQ();
    SetNCOFrequency(LMS7002M_Tx, 1e6, 0);
    SetNCOFrequency(LMS7002M_Tx, tx_lpf_IF, 1);

    //RxTSP
    SetNCOFrequency(LMS7002M_Rx, 0.9e6, 0);
    SetNCOFrequency(LMS7002M_Rx, tx_lpf_IF-0.1e6, 1);

    return MCU_NO_ERROR;
}

uint8_t TuneTxFilter(const float_type tx_lpf_freq_RF)
{
    uint16_t rssi;
    float_type tx_lpf_IF;
    int status;

    if(tx_lpf_freq_RF < TxLPF_RF_LimitLow || tx_lpf_freq_RF > TxLPF_RF_LimitHigh)
        return MCU_TX_LPF_OUT_OF_RANGE;
    //calculate intermediate frequency
    tx_lpf_IF = tx_lpf_freq_RF/2;
    if(tx_lpf_freq_RF > TxLPF_RF_LimitLowMid && tx_lpf_freq_RF < TxLPF_RF_LimitMidHigh)
        tx_lpf_IF = TxLPF_RF_LimitMidHigh/2;
    SaveChipState(0);
    status = TuneTxFilterSetup(tx_lpf_IF);
    if(status != MCU_NO_ERROR)
        goto TxFilterSearchEndStage;
    UpdateRSSIDelay();

    Modify_SPI_Reg_bits(SEL_RX, 0);
    Modify_SPI_Reg_bits(SEL_TX, 0);
    rssi = GetRSSI();
    {
        uint8_t cg_iamp_tbb = Get_SPI_Reg_bits(CG_IAMP_TBB);
        while(rssi < 0x2700 && cg_iamp_tbb < 43)
        {
            ++cg_iamp_tbb;
            Modify_SPI_Reg_bits(CG_IAMP_TBB, cg_iamp_tbb);
            rssi = GetRSSI();
        }
    }

    if(tx_lpf_IF <= TxLPF_RF_LimitLowMid/2)
    {
        bool targetLevelNotReached = false;
        int8_t iterationsLeft = 5;
        do
        {
            uint16_t rssi_dc_lad;
            uint16_t rssi_3dB_lad;
            int16_t ccal_lpflad_tbb;

            Modify_SPI_Reg_bits(SEL_TX, 0);
            Modify_SPI_Reg_bits(SEL_RX, 0);
            rssi_dc_lad = GetRSSI();
            rssi_3dB_lad = rssi_dc_lad * 0.7071;
            Modify_SPI_Reg_bits(SEL_TX, 1);
            Modify_SPI_Reg_bits(SEL_RX, 1);
            rssi = GetRSSI();

            ccal_lpflad_tbb = Get_SPI_Reg_bits(CCAL_LPFLAD_TBB);
            if(rssi < rssi_3dB_lad)
            {
                while(rssi < rssi_3dB_lad && ccal_lpflad_tbb > 0)
                {
                    ccal_lpflad_tbb -= 1;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
                    rssi = GetRSSI();
                }
                if(rssi < rssi_3dB_lad && ccal_lpflad_tbb == 0)
                {
                    uint16_t R;
                    targetLevelNotReached = true;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, 16);
                    R = clamp(Get_SPI_Reg_bits(RCAL_LPFLAD_TBB)+25, 0, 255);
                    Modify_SPI_Reg_bits(RCAL_LPFLAD_TBB, R);
                }
                else
                    targetLevelNotReached = false;
            }
            else if(rssi > rssi_3dB_lad)
            {
                while(rssi > rssi_3dB_lad && ccal_lpflad_tbb < 31)
                {
                    ccal_lpflad_tbb += 1;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
                    rssi = GetRSSI();
                }
                if(rssi > rssi_3dB_lad && ccal_lpflad_tbb == 31)
                {
                    int16_t R;
                    targetLevelNotReached = true;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, 16);
                    R = clamp((int16_t)Get_SPI_Reg_bits(RCAL_LPFLAD_TBB)-10, 0, 255);
                    Modify_SPI_Reg_bits(RCAL_LPFLAD_TBB, R);
                }
                else
                    targetLevelNotReached = false;
                ccal_lpflad_tbb = clamp(++ccal_lpflad_tbb, 0, 31);
                Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
            }
            --iterationsLeft;
            {
                const uint8_t R = Get_SPI_Reg_bits(RCAL_LPFLAD_TBB);
                if (R==0 || R==255)
                    break;
            }
        } while(targetLevelNotReached && iterationsLeft>=0);
    }

    else // LPFH
    {
        bool targetLevelNotReached = false;
        int8_t iterationsLeft = 5;
        Modify_SPI_Reg_bits(C_CTL_PGA_RBB, 2);

        do
        {
            uint16_t rssi_dc_h;
            uint16_t rssi_3dB_h;
            int16_t ccal_lpflad_tbb;
            Modify_SPI_Reg_bits(SEL_TX, 0);
            Modify_SPI_Reg_bits(SEL_RX, 0);
            rssi_dc_h = GetRSSI();
            rssi_3dB_h = rssi_dc_h * 0.7071;
            Modify_SPI_Reg_bits(SEL_TX, 1);
            Modify_SPI_Reg_bits(SEL_RX, 1);
            rssi = GetRSSI();

            ccal_lpflad_tbb = Get_SPI_Reg_bits(CCAL_LPFLAD_TBB);
            if(rssi < rssi_3dB_h)
            {
                while(rssi < rssi_3dB_h && ccal_lpflad_tbb > 0)
                {
                    ccal_lpflad_tbb -= 1;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
                    rssi = GetRSSI();
                }
                if(rssi < rssi_3dB_h && ccal_lpflad_tbb == 0)
                {
                    uint8_t R;
                    targetLevelNotReached = true;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, 16);
                    R = clamp(Get_SPI_Reg_bits(RCAL_LPFH_TBB)+25, 0, 255);
                    Modify_SPI_Reg_bits(RCAL_LPFH_TBB, R);
                }
                else
                    targetLevelNotReached = false;
            }
            else if(rssi > rssi_3dB_h)
            {
                while(rssi > rssi_3dB_h && ccal_lpflad_tbb < 31)
                {
                    ccal_lpflad_tbb += 1;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
                    rssi = GetRSSI();
                }
                if(rssi > rssi_3dB_h && ccal_lpflad_tbb == 31)
                {
                    uint8_t R;
                    targetLevelNotReached = true;
                    Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, 16);
                    //R = (int16_t)Get_SPI_Reg_bits(RCAL_LPFH_TBB)-10;
                    R = clamp((int16_t)Get_SPI_Reg_bits(RCAL_LPFH_TBB)-10, 0, 255);
                    Modify_SPI_Reg_bits(RCAL_LPFH_TBB, R);
                }
                else
                    targetLevelNotReached = false;
                ccal_lpflad_tbb = clamp(++ccal_lpflad_tbb, 0, 31);
                Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
            }
            --iterationsLeft;
            {
            const uint8_t R = Get_SPI_Reg_bits(RCAL_LPFH_TBB);
            if (R==0 || R==255)
                break;
            }
        }
        while(targetLevelNotReached && iterationsLeft>=0);
    }
    {
        uint16_t powerDowns;
        uint16_t ccal_lpflad_tbb = Get_SPI_Reg_bits(CCAL_LPFLAD_TBB);
        uint16_t rcal_lpfh_lpflad_tbb = SPI_read(0x0109);
    TxFilterSearchEndStage:
        SaveChipState(1);
        if(status != MCU_NO_ERROR)
            return status;
        Modify_SPI_Reg_bits(CCAL_LPFLAD_TBB, ccal_lpflad_tbb);
        powerDowns = SPI_read(0x0105) & ~0x0016;
        if(tx_lpf_IF <= TxLPF_RF_LimitLowMid/2)
        {
            powerDowns |= 0x10;
            Modify_SPI_Reg_bits(R5_LPF_BYP_TBB, 1);
        }
        else
            powerDowns |= 0x06;

        SPI_write(0x0106, 0x318C);
        SPI_write(0x0107, 0x318C);
        SPI_write(0x0109, rcal_lpfh_lpflad_tbb);
        SPI_write(0x0105, powerDowns);
    }

    return MCU_NO_ERROR;
}
