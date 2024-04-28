/************************************************************************
 * Copyright(c) 2024, One Unified. All rights reserved.                 *
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
 * File:    TrackOrder.cpp
 * Author:  raymond@burkholder.net
 * Project: CurrencyTrader
 * Created: April 28, 2024 10:39:16
 */

#include <boost/log/trivial.hpp>

#include <boost/lexical_cast.hpp>

#include "TrackOrder.hpp"

TrackOrder::TrackOrder()
: m_stateTrade( ETradeState::Init )
{}

TrackOrder::~TrackOrder() {}

void TrackOrder::Set( quantity_t quantity, pPosition_t pPosition, ou::ChartDataView& cdv, int slot ) {

  assert( 0 < quantity );
  m_quantityToOrder = quantity;

  assert( pPosition );
  m_pPosition = pPosition;

  cdv.Add( slot, &m_ceLongEntry );
  cdv.Add( slot, &m_ceLongExit );

  cdv.Add( slot, &m_ceShortEntry );
  cdv.Add( slot, &m_ceShortExit );

}

void TrackOrder::EnterLong( const ou::tf::Quote& quote ) { // limit orders, in real, will need to be normalized
  double dblMidPoint( quote.Midpoint() );
  //assert( nullptr == m_pOrderPending.get() );
  m_dblProfitMax = m_dblUnRealized = m_dblProfitMin = 0.0;
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, m_quantityToOrder );
  //pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, 1, m_quote.Bid() );
  assert( pOrder );
  pOrder->SetSignalPrice( dblMidPoint );
  pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  m_ceLongEntry.AddLabel( quote.DateTime(), dblMidPoint, "LeS-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  m_stateTrade = ETradeState::LongSubmitted;
  m_pOrderPending = pOrder;
  m_pPosition->PlaceOrder( pOrder );
  //ShowOrder( pOrder );
}

void TrackOrder::EnterShort( const ou::tf::Quote& quote ) { // limit orders, in real, will need to be normalized
  double dblMidPoint( quote.Midpoint() );
  //assert( nullptr == m_pOrderPending.get() );
  m_dblProfitMax = m_dblUnRealized = m_dblProfitMin = 0.0;
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, m_quantityToOrder );
  //pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, 1, m_quote.Ask() );
  assert( pOrder );
  pOrder->SetSignalPrice( dblMidPoint );
  pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  m_ceShortEntry.AddLabel( quote.DateTime(), dblMidPoint, "SeS-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  m_stateTrade = ETradeState::ShortSubmitted;
  m_pOrderPending = pOrder;
  m_pPosition->PlaceOrder( pOrder );
  //ShowOrder( pOrder );
}

void TrackOrder::ExitLong( const ou::tf::Quote& quote ) {
  double dblMidPoint( quote.Midpoint() );
  //assert( nullptr == m_pOrderPending.get() );
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, m_quantityToOrder );
  //pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Sell, 1, m_quote.Ask() );
  assert( pOrder );
  pOrder->SetSignalPrice( dblMidPoint );
  pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  m_ceLongExit.AddLabel( quote.DateTime(), dblMidPoint, "LxS1-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  m_stateTrade = ETradeState::LongExitSubmitted;
  m_pOrderPending = pOrder;
  m_pPosition->PlaceOrder( pOrder );
  //ShowOrder( pOrder );
}

void TrackOrder::ExitShort( const ou::tf::Quote& quote ) {
  double dblMidPoint( quote.Midpoint() );
  //assert( nullptr == m_pOrderPending.get() );
  pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, m_quantityToOrder );
  //pOrder_t pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Limit, ou::tf::OrderSide::Buy, 1, m_quote.Bid() );
  assert( pOrder );
  pOrder->SetSignalPrice( dblMidPoint );
  pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  m_ceShortExit.AddLabel( quote.DateTime(), dblMidPoint, "SxS1-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
  m_stateTrade = ETradeState::ShortExitSubmitted;
  m_pOrderPending = pOrder;
  m_pPosition->PlaceOrder( pOrder );
  //ShowOrder( pOrder );
}

void TrackOrder::ShowOrder( pOrder_t pOrder ) {
  //m_pTreeItemOrder = m_pTreeItemSymbol->AppendChild(
  //    "Order "
  //  + boost::lexical_cast<std::string>( m_pOrder->GetOrderId() )
  //  );
}

void TrackOrder::HandleOrderCancelled( const ou::tf::Order& order ) {
  m_pOrderPending->OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  m_pOrderPending->OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  switch ( m_stateTrade ) {
    case ETradeState::EndOfDayCancel:
    case ETradeState::EndOfDayNeutral:
      BOOST_LOG_TRIVIAL(info)
        << m_pPosition->GetInstrument()->GetInstrumentName()
        << " order " << order.GetOrderId() << " cancelled - end of day";
      break;
    case ETradeState::LongExitSubmitted:
    case ETradeState::ShortExitSubmitted:
      //assert( false );  // TODO: need to figure out a plan to retry exit
      BOOST_LOG_TRIVIAL(error)
        << m_pPosition->GetInstrument()->GetInstrumentName()
        << " order " << order.GetOrderId() << " cancelled - state machine needs fixes";
      m_stateTrade = ETradeState::Done;
      break;
    default:
      m_stateTrade = ETradeState::Search;
  }
  m_pOrderPending.reset();
}

void TrackOrder::HandleOrderFilled( const ou::tf::Order& order ) {
  m_pOrderPending->OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderCancelled ) );
  m_pOrderPending->OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleOrderFilled ) );
  switch ( m_stateTrade ) {
    case ETradeState::LongSubmitted:
      m_ceLongFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Long Fill" );
      m_stateTrade = ETradeState::LongExitSignal;
      break;
    case ETradeState::ShortSubmitted:
      m_ceShortFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Short Fill" );
      m_stateTrade = ETradeState::ShortExitSignal;
      break;
    case ETradeState::LongExitSubmitted:
      m_ceShortFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Long Exit Fill" );
      m_stateTrade = ETradeState::Search;
      break;
    case ETradeState::ShortExitSubmitted:
      m_ceLongFill.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "Short Exit Fill" );
      m_stateTrade = ETradeState::Search;
      break;
    case ETradeState::EndOfDayCancel:
    case ETradeState::EndOfDayNeutral:
      // figure out what labels to apply
      break;
    case ETradeState::Done:
      break;
    default:
       assert( false ); // TODO: unravel the state mess if we get here
  }
  m_pOrderPending.reset();
}

void TrackOrder::HandleCancel( boost::gregorian::date, boost::posix_time::time_duration ) { // one shot
  m_stateTrade = TrackOrder::ETradeState::EndOfDayCancel;
  if ( m_pPosition ) {
    m_pPosition->CancelOrders();
  }
}

void TrackOrder::HandleGoNeutral( boost::gregorian::date, boost::posix_time::time_duration ) { // one shot
  switch ( m_stateTrade ) {
    case ETradeState::NoTrade:
      // do nothing
      break;
    default:
      m_stateTrade = TrackOrder::ETradeState::EndOfDayNeutral;
      if ( m_pPosition ) {
        m_pPosition->ClosePosition();
      }
      break;
  }
}

void TrackOrder::HandleExitOrderCancelled( const ou::tf::Order& order ) {
  ou::tf::Order& order_( const_cast<ou::tf::Order&>( order ) );
  order_.OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
  order_.OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );

  switch ( order.GetOrderSide() ) {
    case ou::tf::OrderSide::EOrderSide::Buy: // is dt filled at 'internal' time?
      //m_ceLongExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetSignalPrice(), "LxC-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      m_stateTrade = ETradeState::Cancelled;  // or use cancelled for custom processing
      break;
    case ou::tf::OrderSide::EOrderSide::Sell: // is dt filled at 'internal' time?
      //m_ceShortExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetSignalPrice(), "SxC-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      m_stateTrade = ETradeState::Cancelled;  // or use cancelled for custom processing
      break;
    default:
      assert( false );
  }
}

void TrackOrder::HandleExitOrderFilled( const ou::tf::Order& order ) {
  ou::tf::Order& order_( const_cast<ou::tf::Order&>( order ) );
  order_.OnOrderCancelled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
  order_.OnOrderFilled.Remove( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );

  switch ( order.GetOrderSide() ) {
    case ou::tf::OrderSide::EOrderSide::Buy:
      //m_ceLongExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "LxF-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      switch( m_stateTrade ) {
        case ETradeState::ShortExitSubmitted:
          m_stateTrade = ETradeState::Search;
          break;
      }
      break;
    case ou::tf::OrderSide::EOrderSide::Sell:
      //m_ceShortExit.AddLabel( order.GetDateTimeOrderFilled(), order.GetAverageFillPrice(), "SxF-" + boost::lexical_cast<std::string>( order.GetOrderId() ) );
      switch( m_stateTrade ) {
        case ETradeState::LongExitSubmitted:
          m_stateTrade = ETradeState::Search;
          break;
      }
      break;
    default:
      assert( false );
  }
}

void TrackOrder::ExitPosition( const ou::tf::Quote& quote ) {
  pOrder_t pOrder;
  double dblMidPoint( quote.Midpoint() );

  if ( m_pPosition->IsActive() ) {
    assert( false ); // size of 1 not going to work with currency
    assert( 1 == m_pPosition->GetActiveSize() );
    switch ( m_pPosition->GetRow().eOrderSideActive ) {
      case ou::tf::OrderSide::EOrderSide::Buy:
        pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, 1 );
        pOrder->SetSignalPrice( dblMidPoint );
        pOrder->SetDescription(
            m_sProfitDescription + ","
          + boost::lexical_cast<std::string>( m_dblProfitMin ) + ","
          + boost::lexical_cast<std::string>( m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax - m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax )
          );
        pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
        pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );
        m_ceLongExit.AddLabel( quote.DateTime(), dblMidPoint, "LxS2-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
        m_stateTrade = ETradeState::LongExitSubmitted;
        m_pPosition->PlaceOrder( pOrder );
        ShowOrder( pOrder );
        break;
      case ou::tf::OrderSide::EOrderSide::Sell:
        pOrder = m_pPosition->ConstructOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, 1 );
        pOrder->SetSignalPrice( dblMidPoint );
        pOrder->SetDescription(
            m_sProfitDescription + ","
          + boost::lexical_cast<std::string>( m_dblProfitMin ) + ","
          + boost::lexical_cast<std::string>( m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax - m_dblUnRealized ) + ","
          + boost::lexical_cast<std::string>( m_dblProfitMax )
          );
        pOrder->OnOrderCancelled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderCancelled ) );
        pOrder->OnOrderFilled.Add( MakeDelegate( this, &TrackOrder::HandleExitOrderFilled ) );
        m_ceShortExit.AddLabel( quote.DateTime(), dblMidPoint, "SxS2-" + boost::lexical_cast<std::string>( pOrder->GetOrderId() ) );
        m_stateTrade = ETradeState::ShortExitSubmitted;
        m_pPosition->PlaceOrder( pOrder );
        ShowOrder( pOrder );
        break;
      default:
        assert( false ); // maybe check for unknown
    }
  }
  else {
    m_stateTrade = ETradeState::Search;
  }
}
