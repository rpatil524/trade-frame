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
 * File:      Server_impl.hpp
 * Author:    raymond@burkholder.net
 * Project:   TableTrader
 * Created:   2022/08/03 17:14:41
 */

#pragma once

#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>

#include <TFTrading/Watch.h>
#include <TFTrading/Portfolio.h>
#include <TFTrading/Instrument.h>

#include <TFOptions/Chain.h>
#include <TFOptions/Chains.h>

namespace ou {
namespace tf {
  class db;
  class BuildInstrument;
namespace iqfeed {
  class IQFeedProvider;
  class OptionChainQuery;
} // namespace iqfeed
namespace ib {
  class TWS;
} // namespace ib
} // namespace tf
} // namespace ou

class Server_impl {
public:

  Server_impl();
  ~Server_impl();

  using fUpdateUnderlyingInfo_t = std::function<void(const std::string&,int)>; // generic name, contract size
  using fUpdateUnderlyingPrice_t = std::function<void(double,int)>; // price,precision
  using fAddExpiry_t = std::function<void(boost::gregorian::date)>;
  using fAddExpiryDone_t = std::function<void()>;

  using fPopulateStrike_t = std::function<void(double,int)>; // strike, precision
  using fPopulateStrikeDone_t = std::function<void()>;

  void SessionAttach( const std::string& sSessionId );
  void SessionDetach( const std::string& sSessionId );

  void Start(
    const std::string& sUnderlyingFuture,
    fUpdateUnderlyingInfo_t&&,
    fUpdateUnderlyingPrice_t&&,
    fAddExpiry_t&&,
    fAddExpiryDone_t&&
    );

  void PopulateStrikes(
    boost::gregorian::date,
    fPopulateStrike_t&&,
    fPopulateStrikeDone_t&&
  );

  void TriggerUpdates( const std::string& sSessionId );

  const std::string& Ticker( double strike, ou::tf::OptionSide::EOptionSide ) const;

  using fRealTime_t = std::function<void( double bid, double ask, uint32_t precision, uint32_t volume, uint32_t contracts, double pnl )>;
  using fAllocated_t = std::function<void( double allocatedTotal, double allocatedOption )>;
  using fFill_t = std::function<void(double)>; // fill price

  void AddStrike(
    double strike,
    ou::tf::OptionSide::EOptionSide, ou::tf::OrderSide::EOrderSide,
    fRealTime_t&&, fAllocated_t&&,
    fFill_t&& fEntry, fFill_t&& fExit );
  void DelStrike( double strike );

  void ChangeInvestment( double dblInvestment );
  void ChangeAllocation( double dblStrike, double dblRatio );

  bool PlaceOrders( const std::string& sPortfolioTimeStamp );
  void CancelAll();
  void CloseAll();

protected:
private:

  using pOrder_t = ou::tf::Order::pOrder_t;
  using pWatch_t = ou::tf::Watch::pWatch_t;
  using pPosition_t = ou::tf::Position::pPosition_t;
  using pOption_t = ou::tf::option::Option::pOption_t;
  using pPortfolio_t = ou::tf::Portfolio::pPortfolio_t;
  using pInstrument_t = ou::tf::Instrument::pInstrument_t;

  enum EConnection { quiescent, connecting, connected, disconnecting, disconnected };
  EConnection m_state;

  std::mutex m_mutex;

  std::shared_ptr<ou::tf::ib::TWS> m_pProviderTWS;
  std::shared_ptr<ou::tf::iqfeed::IQFeedProvider> m_pProviderIQFeed;

  std::unique_ptr<ou::tf::db> m_pdb;

  std::unique_ptr<ou::tf::BuildInstrument> m_pBuildInstrumentBoth;
  std::unique_ptr<ou::tf::BuildInstrument> m_pBuildInstrumentIQFeed;

  pWatch_t m_pWatchUnderlying;

  pPortfolio_t m_pPortfolioUnderlying;
  pPortfolio_t m_pPortfolioOptions;

  fUpdateUnderlyingInfo_t m_fUpdateUnderlyingInfo;
  fUpdateUnderlyingPrice_t m_fUpdateUnderlyingPrice;
  fAddExpiry_t m_fAddExpiry;
  fAddExpiryDone_t m_fAddExpiryDone;

  int m_nPrecision;
  unsigned int m_nMultiplier; // used to populate futures options multiplier (not supplied by iqf)

  struct BuiltOption: public ou::tf::option::chain::OptionName {
    pOption_t pOption;
  };

  using chain_t = ou::tf::option::Chain<BuiltOption>;
  using mapChains_t = std::map<boost::gregorian::date, chain_t>;
  mapChains_t m_mapChains;
  mapChains_t::const_iterator m_citerChains; // chosen date

  using pOptionChainQuery_t = std::shared_ptr<ou::tf::iqfeed::OptionChainQuery>;
  pOptionChainQuery_t m_pOptionChainQuery;

  size_t m_nOptionsNames;
  size_t m_nOptionsLoaded;

  double m_dblInvestment;
  double m_dblAllocated;

  struct Session {
    //fUpdateUnderlyingPrice_t m_fUpdateUnderlyingPrice;
    Session()
    //: m_fUpdateUnderlyingPrice {}
    {}
  };

  using mapSession_t = std::unordered_map<std::string,Session>;
  mapSession_t m_mapSession;

  struct UIOption {

    ou::tf::OrderSide::EOrderSide m_orderSide;
    double m_dblRatioAllocation;
    double m_dblAllocated;
    uint32_t m_nMultiplier; // keep local for some speed of lookup
    uint32_t m_nContracts;
    pOption_t m_pOption;
    pPosition_t m_pPosition;
    pOrder_t m_pOrderEntry;
    pOrder_t m_pOrderExit;

    fRealTime_t m_fRealTime;
    fAllocated_t m_fAllocated;
    fFill_t m_fFillEntry;
    fFill_t m_fFillExit;

    enum IBContractState { unknown, acquiring, acquired } m_stateIBContract;

    UIOption( pOption_t pOption, ou::tf::OrderSide::EOrderSide orderSide )
    : m_orderSide( orderSide )
    , m_dblRatioAllocation {}
    , m_dblAllocated {}
    , m_nMultiplier {}
    , m_nContracts {}
    , m_fRealTime {}
    , m_fAllocated {}
    , m_fFillEntry {}
    , m_fFillExit {}
    , m_pOption( pOption )
    , m_stateIBContract( IBContractState::unknown )
    {
      m_nMultiplier = m_pOption->GetInstrument()->GetMultiplier();
      //m_pOption->OnFundamentals.Add( MakeDelegate( this, &UIOption::HandleFundamentals ) );
      //m_pOption->OnQuote.Add( MakeDelegate( this, &UIOption::HandleQuote ) );
      //m_pOption->OnTrade.Add( MakeDelegate( this, &UIOption::HandleTrade ) );
      assert( m_pOption->StartWatch() );
    }

    UIOption( UIOption&& rhs )
    : m_orderSide( rhs.m_orderSide )
    , m_dblRatioAllocation( rhs.m_dblRatioAllocation )
    , m_dblAllocated( rhs.m_dblAllocated )
    , m_nMultiplier( rhs.m_nMultiplier )
    , m_nContracts( rhs.m_nContracts )
    , m_pPosition( std::move( rhs.m_pPosition ) )
    , m_pOrderEntry( std::move( rhs.m_pOrderEntry ) )
    , m_pOrderExit( std::move( rhs.m_pOrderExit ) )
    , m_fRealTime( std::move( rhs.m_fRealTime ) )
    , m_fFillEntry( std::move( rhs.m_fFillEntry ) )
    , m_fFillExit( std::move( rhs.m_fFillExit ) )
    , m_fAllocated( std::move( rhs.m_fAllocated ) )
    , m_stateIBContract( rhs.m_stateIBContract )
    {
      assert( rhs.m_pOption->StopWatch() );
      //rhs.m_pOption->OnFundamentals.Remove( MakeDelegate( &rhs, &UIOption::HandleFundamentals ) );
      //rhs.m_pOption->OnTrade.Remove( MakeDelegate( &rhs, &UIOption::HandleTrade ) );
      //rhs.m_pOption->OnQuote.Remove( MakeDelegate( &rhs, &UIOption::HandleQuote ) );

      m_pOption = std::move( rhs.m_pOption );

      //m_pOption->OnQuote.Add( MakeDelegate( this, &UIOption::HandleQuote ) );
      //m_pOption->OnTrade.Add( MakeDelegate( this, &UIOption::HandleTrade ) );
      //m_pOption->OnFundamentals.Add( MakeDelegate( this, &UIOption::HandleFundamentals ) );
      assert( m_pOption->StartWatch() );
    }

    ~UIOption() {
      if ( m_pOption ) {
        assert( m_pOption->StopWatch() );
        //m_pOption->OnTrade.Remove( MakeDelegate( this, &UIOption::HandleTrade ) );
        //m_pOption->OnQuote.Remove( MakeDelegate( this, &UIOption::HandleQuote ) );
        //m_pOption->OnFundamentals.Remove( MakeDelegate( this, &UIOption::HandleFundamentals ) );
      }
    }

    void UpdateContracts( double price ) {
      if ( 0 < m_nMultiplier ) {
        m_nContracts = ( 0.0 < price ) ? (m_dblAllocated / price ) / m_nMultiplier : 0;
      }
      else {
        m_nContracts = ( 0.0 < price ) ? m_dblAllocated / price : 0;
      }
    }

    void HandleQuote( const ou::tf::Quote& quote ) {}

    void HandleTrade( const ou::tf::Trade& trade ) {}

    void HandleOrderFilledEntry( const ou::tf::Order& order ) {
      m_fFillEntry( order.GetAverageFillPrice() );
    }

    void HandleOrderFilledExit( const ou::tf::Order& order ) {
      m_fFillExit( order.GetAverageFillPrice() );
    }
  };

  using mapUIOption_t = std::map<double,UIOption>;
  mapUIOption_t m_mapUIOption;

  void Connected_IQFeed( int );
  void Connected_TWS( int );
  void Connected( int );

  void Disconnected_IQFeed( int );
  void Disconnected_TWS( int );
  void Disconnected( int );

  void UnderlyingInitialize( pInstrument_t );

  void InstrumentToOption( pInstrument_t );
  void PopulateExpiry();

  void UnderlyingFundamentals( const ou::tf::Watch::Fundamentals& );
  void UnderlyingQuote( const ou::tf::Quote& );
  void UnderlyingTrade( const ou::tf::Trade& );

  void UpdateAllocations();
};