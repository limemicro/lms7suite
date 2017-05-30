#include "pnlBoardControls.h"
#include "wx/wxprec.h"
#include "lime/LimeSuite.h"
#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif //WX_PRECOMP

#include "pnlUltimateEVB.h"
#include "pnluLimeSDR.h"
#include "pnlLimeSDR.h"
#include "pnlBuffers.h"
#include "lms7002m_novena_wxgui.h"
#include "RFSpark_wxgui.h"
 #include "pnlQSpark.h"
#include <IConnection.h>
#include <LMSBoards.h>
#include <ADCUnits.h>
#include <assert.h>
#include <wx/spinctrl.h>
#include <vector>
#include "lms7suiteEvents.h"

using namespace std;
using namespace lime;

static wxString power2unitsString(char powerx3)
{
    switch (powerx3)
    {
    case -8:
        return "y";
    case -7:
        return "z";
    case -6:
        return "a";
    case -5:
        return "f";
    case -4:
        return "p";
    case -3:
        return "n";
    case -2:
        return "u";
    case -1:
        return "m";
    case 0:
        return "";
    case 1:
        return "k";
    case 2:
        return "M";
    case 3:
        return "G";
    case 4:
        return "T";
    case 5:
        return "P";
    case 6:
        return "E";
    case 7:
        return "Y";
    default:
        return "";
    }
}

pnlBoardControls::pnlBoardControls(wxWindow* parent, wxWindowID id, const wxString &title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style), lmsControl(nullptr)
{
    additionalControls = nullptr;
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	wxFlexGridSizer* fgSizer247;
	fgSizer247 = new wxFlexGridSizer( 0, 1, 0, 0 );
	fgSizer247->AddGrowableCol( 0 );
	fgSizer247->AddGrowableRow( 1 );
	fgSizer247->SetFlexibleDirection( wxBOTH );
	fgSizer247->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	wxFlexGridSizer* fgSizer248;
	fgSizer248 = new wxFlexGridSizer( 0, 4, 0, 0 );
	fgSizer248->SetFlexibleDirection( wxBOTH );
	fgSizer248->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	btnReadAll = new wxButton( this, wxID_ANY, wxT("Read all"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer248->Add( btnReadAll, 0, wxALL, 5 );

	btnWriteAll = new wxButton( this, wxID_ANY, wxT("Write all"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer248->Add( btnWriteAll, 0, wxALL, 5 );

	m_staticText349 = new wxStaticText( this, wxID_ANY, wxT("Labels:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText349->Wrap( -1 );
	fgSizer248->Add( m_staticText349, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

	wxArrayString cmbBoardSelectionChoices;
	cmbBoardSelection = new wxChoice( this, wxNewId(), wxDefaultPosition, wxDefaultSize, cmbBoardSelectionChoices, 0 );
	cmbBoardSelection->SetSelection( 0 );
	fgSizer248->Add( cmbBoardSelection, 0, wxALL, 5 );

	for (int i = 0; i < LMS_DEV_COUNT; ++i) {
            cmbBoardSelection->AppendString(wxString::From8BitData(GetDeviceName((eLMS_DEV)i)));
	}

	fgSizer247->Add( fgSizer248, 1, wxEXPAND, 5 );

    pnlCustomControls = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, _("Custom controls"));
    wxFlexGridSizer* sizerCustomControls = new wxFlexGridSizer(0, 5, 5, 5);

    sizerCustomControls->Add(new wxStaticText(pnlCustomControls, wxID_ANY, _("Ch.")));
    sizerCustomControls->Add(new wxStaticText(pnlCustomControls, wxID_ANY, _("Value")));
    sizerCustomControls->Add(new wxStaticText(pnlCustomControls, wxID_ANY, _("Power")));
    sizerCustomControls->Add(new wxStaticText(pnlCustomControls, wxID_ANY, _("Units")));
    sizerCustomControls->Add(new wxStaticText(pnlCustomControls, wxID_ANY, _("")));

    //reading
    spinCustomChannelRd = new wxSpinCtrl(pnlCustomControls, wxNewId(), _("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 0);
    sizerCustomControls->Add(spinCustomChannelRd, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    txtCustomValueRd = new wxStaticText(pnlCustomControls, wxID_ANY, _("0"));
    sizerCustomControls->Add(txtCustomValueRd, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    txtCustomPowerOf10Rd = new wxStaticText(pnlCustomControls, wxID_ANY, _(""));
    sizerCustomControls->Add(txtCustomPowerOf10Rd, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    txtCustomUnitsRd = new wxStaticText(pnlCustomControls, wxID_ANY, _(""));
    sizerCustomControls->Add(txtCustomUnitsRd, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    btnCustomRd = new wxButton(pnlCustomControls, wxNewId(), _("Read"));
    btnCustomRd->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(pnlBoardControls::OnCustomRead), NULL, this);
    sizerCustomControls->Add(btnCustomRd, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);

    //writing
    spinCustomChannelWr = new wxSpinCtrl(pnlCustomControls, wxNewId(), _("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 255, 0);
    sizerCustomControls->Add(spinCustomChannelWr, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    spinCustomValueWr = new wxSpinCtrl(pnlCustomControls, wxNewId(), _(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 65535, 0);
    sizerCustomControls->Add(spinCustomValueWr, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    spinCustomValueWr->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(pnlBoardControls::OnSetDACvalues), NULL, this);

    wxArrayString powerChoices;
    for (int i = -8; i <= 7; ++i)
        powerChoices.push_back(wxString::From8BitData(power2unitsString(i)));
    cmbCustomPowerOf10Wr = new wxChoice(pnlCustomControls, wxNewId(), wxDefaultPosition, wxDefaultSize, powerChoices, 0);
    cmbCustomPowerOf10Wr->SetSelection(0);
    sizerCustomControls->Add(cmbCustomPowerOf10Wr, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);

    wxArrayString unitChoices;
    for (int i = 0; i < ADC_UNITS_COUNT; ++i) //add all defined units
        unitChoices.push_back(wxString::From8BitData(adcUnits2string(i)));
    for (int i = ADC_UNITS_COUNT; i < ADC_UNITS_COUNT + 4; ++i) //add some options to use undefined units
        unitChoices.push_back(wxString::Format(_("%i"), i));
    cmbCustomUnitsWr = new wxChoice(pnlCustomControls, wxNewId(), wxDefaultPosition, wxDefaultSize, unitChoices, 0);
    cmbCustomUnitsWr->SetSelection(0);
    sizerCustomControls->Add(cmbCustomUnitsWr, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);

    btnCustomWr = new wxButton(pnlCustomControls, wxNewId(), _("Write"));
    btnCustomWr->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(pnlBoardControls::OnCustomWrite), NULL, this);
    sizerCustomControls->Add(btnCustomWr, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);

    pnlCustomControls->SetSizer(sizerCustomControls);
    pnlCustomControls->Fit();

    fgSizer247->Add(pnlCustomControls, 1, wxEXPAND, 5);

    wxFlexGridSizer* fgSizer249 = new wxFlexGridSizer( 0, 2, 5, 5 );
    fgSizer249->AddGrowableCol( 0 );
    fgSizer249->AddGrowableCol( 1 );
    fgSizer249->SetFlexibleDirection( wxBOTH );
    fgSizer249->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    pnlReadControls = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, _("Read"));
    wxStaticBoxSizer* sbSizer133 = new wxStaticBoxSizer( new wxStaticBox( pnlReadControls, wxID_ANY, wxT("Read") ), wxVERTICAL );
    sizerAnalogRd = new wxFlexGridSizer( 0, 4, 2, 2 );
    sizerAnalogRd->SetFlexibleDirection( wxBOTH );
    sizerAnalogRd->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
    sizerAnalogRd->Add(new wxStaticText(pnlReadControls, wxID_ANY, _("Ch.")), 1, wxALL, 5);
    sizerAnalogRd->Add(new wxStaticText(pnlReadControls, wxID_ANY, _("Name")), 1, wxALL, 5);
    sizerAnalogRd->Add(new wxStaticText(pnlReadControls, wxID_ANY, _("Value")), 1, wxALL, 5);
    sizerAnalogRd->Add(new wxStaticText(pnlReadControls, wxID_ANY, _("Units")), 1, wxALL, 5);
    sbSizer133->Add( sizerAnalogRd, 1, wxEXPAND, 5 );
    pnlReadControls->SetSizer(sbSizer133);
    pnlReadControls->Fit();
    pnlReadControls->Hide();
    fgSizer249->Add( pnlReadControls, 1, wxEXPAND, 5 );

    pnlWriteControls = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, _("Read"));
    wxStaticBoxSizer* sbSizer134 = new wxStaticBoxSizer( new wxStaticBox( pnlWriteControls, wxID_ANY, wxT("Write") ), wxVERTICAL );
    sizerAnalogWr = new wxFlexGridSizer( 0, 4, 2, 2 );
    sizerAnalogWr->SetFlexibleDirection( wxBOTH );
    sizerAnalogWr->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
    sizerAnalogWr->Add(new wxStaticText(pnlWriteControls, wxID_ANY, _("Ch.")), 1, wxALL, 5);
    sizerAnalogWr->Add(new wxStaticText(pnlWriteControls, wxID_ANY, _("Name")), 1, wxALL, 5);
    sizerAnalogWr->Add(new wxStaticText(pnlWriteControls, wxID_ANY, _("Value")), 1, wxALL, 5);
    sizerAnalogWr->Add(new wxStaticText(pnlWriteControls, wxID_ANY, _("Units")), 1, wxALL, 5);
    sbSizer134->Add( sizerAnalogWr, 1, wxEXPAND, 5 );
    pnlWriteControls->SetSizer(sbSizer134);
    pnlWriteControls->Fit();
    pnlWriteControls->Hide();
    fgSizer249->Add( pnlWriteControls, 1, wxEXPAND, 5 );

    fgSizer247->Add( fgSizer249, 1, wxEXPAND, 5 );

    sizerAdditionalControls = new wxFlexGridSizer(0, 1, 0, 0);
    fgSizer247->Add(sizerAdditionalControls, 1, wxEXPAND, 5);
    this->SetSizer( fgSizer247 );
    this->Layout();
    fgSizer247->Fit( this );

	// Connect Events
    cmbBoardSelection->Connect(wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler(pnlBoardControls::OnUserChangedBoardType), NULL, this);
    btnReadAll->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( pnlBoardControls::OnReadAll ), NULL, this );
    btnWriteAll->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( pnlBoardControls::OnWriteAll ), NULL, this );

    SetupControls(GetDeviceName(LMS_DEV_UNKNOWN));
}

pnlBoardControls::~pnlBoardControls()
{
	// Disconnect Events
	btnReadAll->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( pnlBoardControls::OnReadAll ), NULL, this );
	btnWriteAll->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( pnlBoardControls::OnWriteAll ), NULL, this );
	cmbBoardSelection->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( pnlBoardControls::OnUserChangedBoardType), NULL, this );
}

void pnlBoardControls::OnReadAll( wxCommandEvent& event )
{
    vector<uint8_t> ids;
    vector<double> values;
    vector<string> units;

    for (const auto& param : mADCparameters)
    {
        ids.push_back(param.channel);
        values.push_back(0);
        units.push_back("");
    }


    for (size_t i = 0; i < mADCparameters.size(); ++i)
    {
        float_type value;
        lms_name_t units;
        int status = LMS_ReadCustomBoardParam(lmsControl,mADCparameters[i].channel,&value,units);
        if (status != 0)
        {
            wxMessageBox(LMS_GetLastErrorMessage(), _("Warning"));
            return;
        }
        mADCparameters[i].channel = ids[i];
        mADCparameters[i].units = units;
        mADCparameters[i].value = value;
    }
    for (size_t i = 0; i < mDACparameters.size(); ++i)
    {
        for (size_t j = 0; j < mADCparameters.size(); ++j)
        {
            if (mDACparameters[i].channel == mADCparameters[j].channel)
            {
                mDACparameters[i].value = mADCparameters[j].value;
                mDACparameters[i].units = mADCparameters[j].units;
                mDAC_GUI_widgets[i]->value->SetValue(mDACparameters[i].value);
                break;
            }
        }
    }
    UpdatePanel();
}

void pnlBoardControls::OnWriteAll( wxCommandEvent& event )
{
    if (!LMS_IsOpen(lmsControl,1))
    {
        wxMessageBox(_("Device not connected"), _("Warning"));
        return;
    }

    vector<uint8_t> ids;
    vector<double> values;
    //vector<string> units; currently is not used

    for (size_t i = 0; i < mDACparameters.size(); ++i)
    {
        ids.push_back(mDACparameters[i].channel);
        values.push_back(mDACparameters[i].value);
        int status = LMS_WriteCustomBoardParam(lmsControl,mADCparameters[i].channel,mDACparameters[i].value,NULL);
        if (status != 0)
        {
            wxMessageBox(_("Failes to write values"), _("Warning"));
            return;
        }
    }

    if(additionalControls)
    {
        wxCommandEvent evt;
        evt.SetEventType(WRITE_ALL_VALUES);
        evt.SetId(additionalControls->GetId());
        wxPostEvent(additionalControls, evt);
    }
}

void pnlBoardControls::Initialize(lms_device_t* controlPort)
{
    lmsControl = controlPort;
    if(!LMS_IsOpen(lmsControl,0))
        return;
    const lms_dev_info_t* info;
    if ((info = LMS_GetDeviceInfo(lmsControl))!=nullptr)
    {
        SetupControls(info->deviceName);
        wxCommandEvent evt;
        OnReadAll(evt);
    }
}

void pnlBoardControls::UpdatePanel()
{
    assert(mADCparameters.size() == mADC_GUI_widgets.size());
    for (size_t i = 0; i < mADCparameters.size(); ++i)
    {
        mADC_GUI_widgets[i]->channel->SetLabel(wxString::Format(_("%i"), mADCparameters[i].channel));
        mADC_GUI_widgets[i]->title->SetLabel(wxString(mADCparameters[i].name));
        mADC_GUI_widgets[i]->value->SetLabel(wxString::Format(_("%1.0f"), mADCparameters[i].value));
        mADC_GUI_widgets[i]->units->SetLabelText(wxString::Format("%s", mADCparameters[i].units));
    }

    if(additionalControls)
    {
        wxCommandEvent evt;
        evt.SetEventType(READ_ALL_VALUES);
        evt.SetId(additionalControls->GetId());
        wxPostEvent(additionalControls, evt);
    }
}

std::vector<pnlBoardControls::ADC_DAC> pnlBoardControls::getBoardADCs(const string &boardID)
{
    std::vector<ADC_DAC> paramList;
    if(boardID == GetDeviceName(LMS_DEV_LIMESDR)
        || boardID == GetDeviceName(LMS_DEV_ULIMESDR)
        || boardID == GetDeviceName(LMS_DEV_LIMESDR_PCIE)
        || boardID == GetDeviceName(LMS_DEV_LIMESDR_QPCIE)
        || boardID == GetDeviceName(LMS_DEV_LIMESDR_USB_SP)
        || boardID == GetDeviceName(LMS_DEV_LMS7002M_ULTIMATE_EVB))
    {

        paramList.push_back(ADC_DAC{"DAC", 0, 0, adcUnits2string(RAW)});
        paramList.push_back(ADC_DAC{"Board Temperature", 0, 1, adcUnits2string(TEMPERATURE)});
    }
    return paramList;
}

std::vector<pnlBoardControls::ADC_DAC> pnlBoardControls::getBoardDACs(const std::string &boardID)
{
    std::vector<ADC_DAC> paramList;
    if(boardID == GetDeviceName(LMS_DEV_LIMESDR)
        || boardID == GetDeviceName(LMS_DEV_ULIMESDR)
        || boardID == GetDeviceName(LMS_DEV_LIMESDR_PCIE)
        || boardID == GetDeviceName(LMS_DEV_LIMESDR_QPCIE)
        || boardID == GetDeviceName(LMS_DEV_LIMESDR_USB_SP)
        || boardID == GetDeviceName(LMS_DEV_LMS7002M_ULTIMATE_EVB))
    {
        ADC_DAC dacVoltage;
        dacVoltage.name = "DAC";
        dacVoltage.channel = 0;
        dacVoltage.powerOf10 = 0;
        dacVoltage.units = RAW;
        dacVoltage.value = 0;
        dacVoltage.minValue = 0;
        dacVoltage.maxValue = 255;

        paramList.push_back(dacVoltage);
    }
    return paramList;
}

void pnlBoardControls::SetupControls(const std::string &boardID)
{

    if(additionalControls)
    {
        additionalControls->Destroy();
        additionalControls = nullptr;
    }

    if (boardID == GetDeviceName(LMS_DEV_UNKNOWN))
        pnlCustomControls->Show();
    else
        pnlCustomControls->Hide();
    for(int i=0; i<LMS_DEV_COUNT; ++i)
    {
        if(boardID == GetDeviceName((eLMS_DEV)i))
        {
            cmbBoardSelection->SetSelection(i);
            break;
        }
    }

    for (auto &widget : mADC_GUI_widgets)
        delete widget;
    mADC_GUI_widgets.clear(); //delete previously existing controls
    mADCparameters = getBoardADCs(boardID); //update controls list by board type

    for (auto &widget : mDAC_GUI_widgets)
        delete widget;
    mDAC_GUI_widgets.clear(); //delete previously existing controls
    mDACparameters = getBoardDACs(boardID); //update controls list by board type

    if (boardID != GetDeviceName(LMS_DEV_UNKNOWN))
    {
        if (mADCparameters.size()!=0)
            pnlReadControls->Show();
        else
            pnlReadControls->Hide();

        for (size_t i = 0; i < mADCparameters.size(); ++i)
        {
            ADC_GUI *gui = new ADC_GUI();
            gui->channel = new wxStaticText(pnlReadControls, wxID_ANY, wxString::Format(_("%i"), mADCparameters[i].channel));
            gui->title = new wxStaticText(pnlReadControls, wxID_ANY, wxString(mADCparameters[i].name));
            gui->value = new wxStaticText(pnlReadControls, wxID_ANY, _(""));
            gui->units = new wxStaticText(pnlReadControls, wxID_ANY, wxString::Format("%s", mADCparameters[i].units));
            mADC_GUI_widgets.push_back(gui);
            sizerAnalogRd->Add(gui->channel, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
            sizerAnalogRd->Add(gui->title, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
            sizerAnalogRd->Add(gui->value, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
            sizerAnalogRd->Add(gui->units, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
        }

        if (mDACparameters.size()!=0)
            pnlWriteControls->Show();
        else
            pnlWriteControls->Hide();

        for (size_t i = 0; i < mDACparameters.size(); ++i)
        {
            DAC_GUI *gui = new DAC_GUI();
            gui->channel = new wxStaticText(pnlWriteControls, wxID_ANY, wxString::Format(_("%i"), mDACparameters[i].channel));
            gui->title = new wxStaticText(pnlWriteControls, wxID_ANY, wxString(mDACparameters[i].name));
            gui->value = new wxSpinCtrl(pnlWriteControls, wxNewId(), _(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, mDACparameters[i].minValue, mDACparameters[i].maxValue, mDACparameters[i].minValue);
            gui->value->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(pnlBoardControls::OnSetDACvalues), NULL, this);
            gui->value->Connect(wxEVT_TEXT_ENTER, wxCommandEventHandler(pnlBoardControls::OnSetDACvaluesENTER), NULL, this);
            gui->units = new wxStaticText(pnlWriteControls, wxID_ANY, wxString::Format("%s", mDACparameters[i].units));
            mDAC_GUI_widgets.push_back(gui);
            sizerAnalogWr->Add(gui->channel, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
            sizerAnalogWr->Add(gui->title, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
            sizerAnalogWr->Add(gui->value, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
            sizerAnalogWr->Add(gui->units, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
        }
    }
    sizerAnalogRd->Layout();
    sizerAnalogWr->Layout();

    if(boardID == GetDeviceName(LMS_DEV_ULIMESDR))
    {
        pnluLimeSDR* pnl = new pnluLimeSDR(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    else if(boardID == GetDeviceName(LMS_DEV_LIMESDR)
         || boardID == GetDeviceName(LMS_DEV_LIMESDR_PCIE))
    {
        pnlLimeSDR* pnl = new pnlLimeSDR(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    else if(boardID == GetDeviceName(LMS_DEV_LMS7002M_ULTIMATE_EVB))
    {
        pnlUltimateEVB* pnl = new pnlUltimateEVB(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    else if(boardID == GetDeviceName(LMS_DEV_RFSPARK)
         || boardID == GetDeviceName(LMS_DEV_EVB7)
         || boardID == GetDeviceName(LMS_DEV_EVB7V2))
    {
        pnlBuffers* pnl = new pnlBuffers(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    else if (boardID == GetDeviceName(LMS_DEV_NOVENA))
    {
        LMS7002M_Novena_wxgui* pnl = new LMS7002M_Novena_wxgui(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    else if (boardID == GetDeviceName(LMS_DEV_RFESPARK))
    {
        RFSpark_wxgui* pnl = new RFSpark_wxgui(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    else if (boardID == GetDeviceName(LMS_DEV_LIMESDR_QPCIE))
    {
        pnlQSpark* pnl = new pnlQSpark(this, wxNewId());
        pnl->Initialize(lmsControl);
        additionalControls = pnl;
        sizerAdditionalControls->Add(additionalControls);
    }
    Layout();
    Fit();
}

void pnlBoardControls::OnSetDACvaluesENTER(wxCommandEvent &event)
{
    wxSpinEvent evt;
    evt.SetEventObject(event.GetEventObject());
    OnSetDACvalues(evt);
}

void pnlBoardControls::OnSetDACvalues(wxSpinEvent &event)
{
    for (size_t i = 0; i < mDAC_GUI_widgets.size(); ++i)
    {
        if (event.GetEventObject() == mDAC_GUI_widgets[i]->value)
        {
            mDACparameters[i].value = mDAC_GUI_widgets[i]->value->GetValue();
            //write to chip
            lms_name_t units;
            strncpy(units,mDACparameters[i].units.c_str(),sizeof(lms_name_t)-1);

            if (lmsControl == nullptr)
                return;

            int status = LMS_WriteCustomBoardParam(lmsControl,mADCparameters[i].channel,mDACparameters[i].value,units);
            if (status != 0)
                wxMessageBox(_("Failed to set value"), _("Warning"));
            return;
        }
    }
}

void pnlBoardControls::OnUserChangedBoardType(wxCommandEvent& event)
{
    SetupControls(GetDeviceName((eLMS_DEV)cmbBoardSelection->GetSelection()));
}

void pnlBoardControls::OnCustomRead(wxCommandEvent& event)
{
    if (!LMS_IsOpen(lmsControl,1))
    {
        wxMessageBox(_("Board not connected"), _("Warning"));
        return;
    }

    uint8_t id = spinCustomChannelRd->GetValue();
    double value = 0;
    lms_name_t units;

    int status = LMS_ReadCustomBoardParam(lmsControl,id,&value,units);
    if (status != 0)
    {
        wxMessageBox(_("Failed to read value"), _("Warning"));
        return;
    }

    txtCustomUnitsRd->SetLabel(units);
    txtCustomValueRd->SetLabel(wxString::Format(_("%1.1f"), value));

}

void pnlBoardControls::OnCustomWrite(wxCommandEvent& event)
{
    if (!LMS_IsOpen(lmsControl,1))
    {
        wxMessageBox(_("Board not connected"), _("Warning"));
        return;
    }

    uint8_t id = spinCustomChannelWr->GetValue();
    int powerOf10 = cmbCustomPowerOf10Wr->GetSelection()*3;
    lms_name_t units;
    strncpy(units,adcUnits2string(cmbCustomUnitsWr->GetSelection()),sizeof(units)-1);

    double value = spinCustomValueWr->GetValue()*pow(10, powerOf10);


    int status = LMS_WriteCustomBoardParam(lmsControl,id,value,units);
    if (status != 0)
    {
        wxMessageBox(_("Failed to write value"), _("Warning"));
        return;
    }
}
