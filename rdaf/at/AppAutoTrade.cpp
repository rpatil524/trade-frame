/************************************************************************
 * Copyright(c) 2022, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

/*
 * File:    AppAutoTrade.cpp
 * Author:  raymond@burkholder.net
 * Project: rdaf/at
 * Created: March 7, 2022 14:35
 */

#include <sstream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <wx/sizer.h>

#include <TFTrading/Watch.h>
#include <TFTrading/Position.h>
#include <TFTrading/BuildInstrument.h>

#include <TFVuTrading/FrameMain.h>
#include <TFVuTrading/PanelLogging.h>
#include <TFVuTrading/WinChartView.h>

#include "Strategy.h"
#include "AppAutoTrade.h"

namespace {
  static const std::string sAppName( "ROOT AutoTrade (rdaf_at)" );
  static const std::string sConfigFilename( "rdaf/at/example.cfg" );
  static const std::string sChoicesFilename( "rdaf/at/choices.cfg" );
  static const std::string sDbName( "rdaf/at/example.db" );
  static const std::string sStateFileName( "rdaf/at/example.state" );
  static const std::string sTimeZoneSpec( "../date_time_zonespec.csv" );
}

IMPLEMENT_APP(AppAutoTrade)

bool AppAutoTrade::OnInit() {

  wxApp::SetAppDisplayName( sAppName );
  wxApp::SetVendorName( "One Unified Net Limited" );
  wxApp::SetVendorDisplayName( "(c)2022 One Unified Net Limited" );

  wxApp::OnInit();

  m_nTSDataStreamSequence = 0;
  {
    std::stringstream ss;
    auto dt = ou::TimeSource::Instance().External();
    ss
      << ou::tf::Instrument::BuildDate( dt.date() )
      << "-"
      << dt.time_of_day()
      ;
    m_sTSDataStreamStarted = ss.str();  // will need to make this generic if need some for multiple providers.
  }

  ou::tf::config::Load( sChoicesFilename, m_choices );

  if ( Load( sConfigFilename, m_options ) ) {
  }
  else {
    return 0;
  }

  //if ( m_options.bSimStart ) {
    // just always delete it
    if ( boost::filesystem::exists( sDbName ) ) {
    boost::filesystem::remove( sDbName );
    }
  //}

  m_pdb = std::make_unique<ou::tf::db>( sDbName );

  if ( 0 < m_options.sGroupDirectory.size() ) {
    m_sim->SetGroupDirectory( m_options.sGroupDirectory );
  }

  m_tws->SetClientId( m_options.nIbInstance );

  m_pFrameMain = new FrameMain( 0, wxID_ANY, sAppName );
  wxWindowID idFrameMain = m_pFrameMain->GetId();

  m_pFrameMain->SetSize( 800, 500 );
  SetTopWindow( m_pFrameMain );

    wxBoxSizer* sizerFrame;
    wxBoxSizer* sizerLeft;
    wxBoxSizer* sizerRight;

    // Sizer for FrameMain
    sizerFrame = new wxBoxSizer(wxHORIZONTAL);
    m_pFrameMain->SetSizer(sizerFrame);

    // Sizer for Controls, Logging
    sizerLeft = new wxBoxSizer(wxVERTICAL);
    sizerFrame->Add(sizerLeft, 0, wxGROW, 2);

    // m_pPanelProviderControl
    m_pPanelProviderControl = new ou::tf::PanelProviderControl( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    m_pPanelProviderControl->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    sizerLeft->Add(m_pPanelProviderControl, 0, wxALIGN_CENTER_HORIZONTAL, 2);

    // m_pPanelLogging
    m_pPanelLogging = new ou::tf::PanelLogging( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    m_pPanelLogging->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    sizerLeft->Add(m_pPanelLogging, 1, wxGROW, 2);

    sizerRight = new wxBoxSizer(wxHORIZONTAL);
    sizerFrame->Add(sizerRight, 1, wxGROW, 2);

    // m_pPanelChart
    m_pWinChartView = new ou::tf::WinChartView( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    m_pWinChartView->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    sizerRight->Add(m_pWinChartView, 1, wxGROW, 2);

  LinkToPanelProviderControl();

  std::cout << "symbol: " << m_options.sSymbol << std::endl;

  m_pWinChartView->SetChartDataView( &m_ChartDataView );
  m_pStrategy = std::make_unique<Strategy>( "rdaf/at/" + m_sTSDataStreamStarted, m_options, m_ChartDataView );

  if ( m_options.bSimStart ) {
    boost::regex expr{ "(20[2-3][0-9][0-1][0-9][0-3][0-9])" };
    boost::smatch what;
    if ( boost::regex_search( m_options.sGroupDirectory, what, expr ) ) {
      boost::gregorian::date date( boost::gregorian::from_undelimited_string( what[ 0 ] ) );
      std::cout << "date " << date << std::endl;
      m_pStrategy->InitForUSEquityExchanges( date );
    }
  }

  m_pFrameMain->SetAutoLayout( true );
  m_pFrameMain->Layout();
  m_pFrameMain->Show( true );

  m_pFrameMain->Bind( wxEVT_CLOSE_WINDOW, &AppAutoTrade::OnClose, this );  // start close of windows and controls

  FrameMain::vpItems_t vItems;
  using mi = FrameMain::structMenuItem;  // vxWidgets takes ownership of the objects
  vItems.push_back( new mi( "Close, Done", MakeDelegate( this, &AppAutoTrade::HandleMenuActionCloseAndDone ) ) );
  vItems.push_back( new mi( "Save Values", MakeDelegate( this, &AppAutoTrade::HandleMenuActionSaveValues ) ) );
  m_pFrameMain->AddDynamicMenu( "Actions", vItems );

  if ( !boost::filesystem::exists( sTimeZoneSpec ) ) {
    std::cout << "Required file does not exist:  " << sTimeZoneSpec << std::endl;
  }

  CallAfter(
    [this](){
      LoadState();
    }
  );

  if ( m_options.bSimStart ) {
    CallAfter(
      [this](){
        using Provider_t = ou::tf::PanelProviderControl::Provider_t;
        m_pPanelProviderControl->SetProvider( Provider_t::ESim, Provider_t::ESim, Provider_t::ESim );
        m_pPanelProviderControl->SetSimulatorState( ou::tf::ProviderOn );
        m_sim->Connect();
      }
    );
  }

  return 1;
}

void AppAutoTrade::HandleMenuActionCloseAndDone() {
  std::cout << "Closing & Done" << std::endl;
  m_pStrategy->CloseAndDone();
}

void AppAutoTrade::HandleMenuActionSaveValues() {
  std::cout << "Saving collected values ... " << std::endl;
  CallAfter(
    [this](){
      m_nTSDataStreamSequence++;
      m_pStrategy->SaveWatch(
        "/app/rdaf/at/" +
        m_sTSDataStreamStarted + "-" +
        boost::lexical_cast<std::string>( m_nTSDataStreamSequence ) ); // sequence number on each save
      std::cout << "  ... Done " << std::endl;
    }
  );
}

void AppAutoTrade::ConstructIBInstrument() {

  using pInstrument_t = ou::tf::Instrument::pInstrument_t;
  using pWatch_t = ou::tf::Watch::pWatch_t;
  using pPosition_t = ou::tf::Position::pPosition_t;

  m_pBuildInstrument = std::make_unique<ou::tf::BuildInstrument>( m_iqfeed, m_tws );
  m_pBuildInstrument->Queue(
    m_options.sSymbol,
    [this]( pInstrument_t pInstrument ){
      const ou::tf::Instrument::idInstrument_t& idInstrument( pInstrument->GetInstrumentName() );
      ou::tf::PortfolioManager& pm( ou::tf::PortfolioManager::GlobalInstance() );
      pPosition_t pPosition;
      if ( pm.PositionExists( "USD", idInstrument ) ) {
        pPosition = pm.GetPosition( "USD", idInstrument );
        std::cout << "position loaded " << pPosition->GetInstrument()->GetInstrumentName() << std::endl;
      }
      else {
        pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrument, m_iqfeed );
        pPosition = pm.ConstructPosition(
          "USD", idInstrument, "ema",
          "ib01", "iq01", m_pExecutionProvider,
          pWatch
        );
        std::cout << "position constructed " << pPosition->GetInstrument()->GetInstrumentName() << std::endl;
      }
      m_pStrategy->SetPosition( pPosition );
    } );

}

void AppAutoTrade::ConstructSimInstrument() {

  using pInstrument_t = ou::tf::Instrument::pInstrument_t;
  using pWatch_t = ou::tf::Watch::pWatch_t;
  using pPosition_t = ou::tf::Position::pPosition_t;

  ou::tf::Instrument::pInstrument_t pInstrument = std::make_shared<ou::tf::Instrument>( m_options.sSymbol );
  const ou::tf::Instrument::idInstrument_t& idInstrument( pInstrument->GetInstrumentName() );
  ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance().Instance() );
  im.Register( pInstrument );  // is a CallAfter required, or can this run in a thread?
  pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrument, m_pData1Provider );
  ou::tf::PortfolioManager& pm( ou::tf::PortfolioManager::GlobalInstance() );
  pPosition_t pPosition;
  if ( pm.PositionExists( "USD", idInstrument ) ) {
    pPosition = pm.GetPosition( "USD", idInstrument );
    std::cout << "sim: probably should delete database first" << std::endl;
    std::cout << "sim: position loaded " << pPosition->GetInstrument()->GetInstrumentName() << std::endl;
  }
  else {
    pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrument, m_pData1Provider );
    pPosition = pm.ConstructPosition(
      "USD", idInstrument, "ema",
      "sim01", "sim01", m_pExecutionProvider,
      pWatch
    );
    std::cout << "Constructed " << pPosition->GetInstrument()->GetInstrumentName() << std::endl;
  }
  m_pStrategy->SetPosition( pPosition );

  FrameMain::vpItems_t vItems;
  using mi = FrameMain::structMenuItem;  // vxWidgets takes ownership of the objects
  vItems.push_back( new mi( "Start", MakeDelegate( this, &AppAutoTrade::HandleMenuActionSimStart ) ) );
  vItems.push_back( new mi( "Stop",  MakeDelegate( this, &AppAutoTrade::HandleMenuActionSimStop ) ) );
  vItems.push_back( new mi( "Stats",  MakeDelegate( this, &AppAutoTrade::HandleMenuActionSimEmitStats ) ) );
  m_pFrameMain->AddDynamicMenu( "Simulation", vItems );

  m_sim->Run();

}

void AppAutoTrade::HandleMenuActionSimStart() {
  CallAfter(
    [this](){
      m_sim->Run();
    }
  );
}

void AppAutoTrade::HandleMenuActionSimStop() {
  CallAfter(
    [this](){
      m_sim->Stop();
    }
  );
}

void AppAutoTrade::HandleMenuActionSimEmitStats() {
  std::stringstream ss;
  m_sim->EmitStats( ss );
  std::cout << "Stats: " << ss.str() << std::endl;
}

int AppAutoTrade::OnExit() {
  // Exit Steps: #4
//  DelinkFromPanelProviderControl();  generates stack errors

  return wxAppConsole::OnExit();
}

void AppAutoTrade::OnClose( wxCloseEvent& event ) {
  // Exit Steps: #2 -> FrameMain::OnClose

  //m_pWinChartView->SetChartDataView( nullptr, false );
  //delete m_pChartData;
  //m_pChartData = nullptr;

  //m_pFrameControls->Close();

  DelinkFromPanelProviderControl();
//  if ( 0 != OnPanelClosing ) OnPanelClosing();
  // event.Veto();  // possible call, if needed
  // event.CanVeto(); // if not a
  SaveState();
  event.Skip();  // auto followed by Destroy();
}

void AppAutoTrade::OnData1Connected( int ) {
  //m_bData1Connected = true;
  ConfirmProviders();
}

void AppAutoTrade::OnData2Connected( int ) {
  //m_bData2Connected = true;
  // Data2 Connection not used
}

void AppAutoTrade::OnExecConnected( int ) {
  //m_bExecConnected = true;
  ConfirmProviders();
}

void AppAutoTrade::OnData1Disconnected( int ) {
  //m_bData1Connected = false;
}

void AppAutoTrade::OnData2Disconnected( int ) {
  //m_bData2Connected = false;
}

void AppAutoTrade::OnExecDisconnected( int ) {
  //m_bExecConnected = false;
}

void AppAutoTrade::ConfirmProviders() {
  if ( m_bData1Connected && m_bExecConnected ) {
    bool bValidCombo( false );
    if (
         ( ou::tf::ProviderInterfaceBase::eidProvider_t::EProviderIQF == m_pData1Provider->ID() )
      && ( ou::tf::ProviderInterfaceBase::eidProvider_t::EProviderIB  == m_pExecutionProvider->ID() )
    ) {
      bValidCombo = true;
      ConstructIBInstrument();
    }
    if (
         ( ou::tf::ProviderInterfaceBase::eidProvider_t::EProviderSimulator == m_pData1Provider->ID() )
      && ( ou::tf::ProviderInterfaceBase::eidProvider_t::EProviderSimulator == m_pExecutionProvider->ID() )
    ) {
      bValidCombo = true;
      ConstructSimInstrument();
    }
    if ( !bValidCombo ) {
      std::cout << "invalid combo of data and execution providers" << std::endl;
    }
  }
}

void AppAutoTrade::SaveState() {
  std::cout << "Saving Config ..." << std::endl;
  std::ofstream ofs( sStateFileName );
  boost::archive::text_oarchive oa(ofs);
  oa & *this;
  std::cout << "  done." << std::endl;
}

void AppAutoTrade::LoadState() {
  try {
    std::cout << "Loading Config ..." << std::endl;
    std::ifstream ifs( sStateFileName );
    boost::archive::text_iarchive ia(ifs);
    ia & *this;
    std::cout << "  done." << std::endl;
  }
  catch(...) {
    std::cout << "load exception" << std::endl;
  }
}